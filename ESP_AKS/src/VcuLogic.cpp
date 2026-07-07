#include <atomic>

#include "VcuLogic.h"
#include "SystemConfig.h"
#include "RelayManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static constexpr const char* TAG = "VCU_LOGIC";
static constexpr uint32_t TASK_PERIOD_MS = 20;

namespace VcuLogic {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static std::atomic<VcuState> s_state{VcuState::INIT};
static QueueHandle_t s_eventQueue = nullptr;
static uint32_t s_stateTimer = 0;
static TelemetryData s_TEL_latestData = {};
static bool s_VCU_warningLogged = false;
static SemaphoreHandle_t s_TEL_dataMutex = nullptr;
// E-STOP bypass: set atomically in postEvent so queue saturation
// cannot swallow an emergency stop command.
static std::atomic<bool> s_eStopPending{false};

static bool s_relaysOpenedInEstop = false;
static bool s_relaysOpenedInFault = false;
static uint32_t s_lastEstopLogMs = 0;
static uint32_t s_lastFaultLogMs = 0;

// READY girişi reddedildiğinde log spam'ini önlemek için: aynı ret nedeni her
// tick değil, neden değiştiğinde veya en fazla 1 sn'de bir loglanır. Neden
// string'leri statik literal olduğundan pointer karşılaştırması geçerlidir.
static const char* s_lastReadyRejectReason = nullptr;
static uint32_t s_lastReadyRejectLogMs = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next);
static bool pollEvent(VcuEvent& out);
static bool isResetInterlockSatisfied();
static bool hasWarningCondition();
static bool hasCriticalCondition();
static const char* readyRejectReason(const TelemetryData& VCU_data);
static TelemetryData getTelemetrySnapshot();

static void handleIdle();
static void handleReady();
static void handleDrive();
static void handleEmergencyStop();
static void handleFault();

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void init() {
    s_eventQueue = xQueueCreate(8, sizeof(VcuEvent));
    if (s_eventQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    s_TEL_dataMutex = xSemaphoreCreateMutex();
    if (s_TEL_dataMutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create telemetry mutex");
        return;
    }

    // Safety first — ensure all relays are off at startup
    RelayManager::instance().allOff();

    transitionTo(VcuState::IDLE);
}

void run() {
    s_stateTimer += TASK_PERIOD_MS;

    if (s_eStopPending.exchange(false, std::memory_order_acquire)) {
        if (s_state.load(std::memory_order_relaxed) != VcuState::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        // Already in EMERGENCY_STOP — clear flag but let run() continue
        // so handleEmergencyStop() keeps executing (timer must not reset).
    }

    VcuState currentState = s_state.load(std::memory_order_relaxed);
    if ((currentState == VcuState::IDLE || currentState == VcuState::READY ||
         currentState == VcuState::DRIVE) &&
        hasCriticalCondition()) {
        ESP_LOGE(TAG, "Critical safety threshold exceeded, entering FAULT");
        transitionTo(VcuState::FAULT);
        return;
    }

    if (hasWarningCondition()) {
        if (!s_VCU_warningLogged) {
            ESP_LOGW(TAG, "Warning threshold active, derating policy pending");
            s_VCU_warningLogged = true;
        }
    } else {
        s_VCU_warningLogged = false;
    }

    VcuEvent event = VcuEvent::NONE;
    if (pollEvent(event)) {
        // High priority events — handled regardless of current state
        if (event == VcuEvent::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        if (event == VcuEvent::FAULT_DETECTED) {
            transitionTo(VcuState::FAULT);
            return;
        }
        
        currentState = s_state.load(std::memory_order_relaxed);
        if (event == VcuEvent::RESET &&
            (currentState == VcuState::FAULT ||
             currentState == VcuState::EMERGENCY_STOP)) {
            if (!isResetInterlockSatisfied()) {
                ESP_LOGW(TAG, "RESET rejected: safety interlock still active");
                return;
            }
            transitionTo(VcuState::IDLE);
            return;
        }

        // State-specific event handling
        switch (currentState) {
            case VcuState::IDLE:
                if (event == VcuEvent::START_REQUEST) {
                    // READY 10 kontaktörü kapatıp HV bus'ı enerjilendirir;
                    // batarya hakkında doğrulanmış/taze veri yoksa geçme.
                    TelemetryData VCU_snap = getTelemetrySnapshot();
                    if (isReadyEntryPermitted(VCU_snap)) {
                        transitionTo(VcuState::READY);
                    } else {
                        const char* reason = readyRejectReason(VCU_snap);
                        if (reason != s_lastReadyRejectReason ||
                            s_stateTimer - s_lastReadyRejectLogMs >= 1000) {
                            ESP_LOGW(TAG, "READY reddedildi: %s", reason);
                            s_lastReadyRejectReason = reason;
                            s_lastReadyRejectLogMs = s_stateTimer;
                        }
                    }
                }
                break;

            case VcuState::READY:
                if (event == VcuEvent::DRIVE_ENABLE)
                    transitionTo(VcuState::DRIVE);
                break;

            default:
                break;
        }
    }

    // Periodic state logic
    switch (s_state.load(std::memory_order_relaxed)) {
        case VcuState::IDLE:
            handleIdle();
            break;
        case VcuState::READY:
            handleReady();
            break;
        case VcuState::DRIVE:
            handleDrive();
            break;
        case VcuState::EMERGENCY_STOP:
            handleEmergencyStop();
            break;
        case VcuState::FAULT:
            handleFault();
            break;
        default:
            break;
    }
}

void postEvent(VcuEvent event) {
    if (s_eventQueue == nullptr)
        return;
    if (event == VcuEvent::EMERGENCY_STOP) {
        // Bypass the queue so a full queue cannot swallow an E-STOP.
        // The flag is checked at the top of run() before any queue drain.
        s_eStopPending.store(true, std::memory_order_release);
        return;
    }
    if (xQueueSend(s_eventQueue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropped event %d",
                 static_cast<int>(event));
    }
}

VcuState getState() {
    return s_state.load(std::memory_order_relaxed);
}

void setTelemetryData(const TelemetryData& TEL_data) {
    if (s_TEL_dataMutex == nullptr)
        return;

    xSemaphoreTake(s_TEL_dataMutex, portMAX_DELAY);
    s_TEL_latestData = TEL_data;
    xSemaphoreGive(s_TEL_dataMutex);
}

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------
static void handleIdle() {
    // All relays off — safe resting state
    // Waiting for START_REQUEST from LoRa/UKS
}

static void handleReady() {
    // Close all positive contactors on entry (runs once via stateTimer guard)
    if (s_stateTimer <= TASK_PERIOD_MS) {
        RelayManager::instance().allOn();
        ESP_LOGI(TAG, "All contactors closed — system READY");
    }
    // DRIVE is entered only after an explicit DRIVE_ENABLE command.
    // Future interlocks should be added here before propulsion is allowed.
}

static void handleDrive() {
    // Contactors remain closed during drive
    // Torque output is still held at zero in the CAN task until the Phase 1.2
    // input-to-torque model is implemented.
}

static void handleEmergencyStop() {
    if (s_stateTimer <= TASK_PERIOD_MS) {
        ESP_LOGW(TAG, "EMERGENCY STOP: zero torque phase started");
    }

    // The CAN task sends zero torque as soon as state != DRIVE.
    // Hold the contactors closed briefly so the zero-torque command can be
    // transmitted before opening the positive contactor bank.
    if (s_stateTimer >= VCU_CONTACTOR_OPEN_DELAY_MS) {
        if (!s_relaysOpenedInEstop) {
            RelayManager::instance().allOff(false); // First time, log it
            s_relaysOpenedInEstop = true;
        } else if (s_stateTimer - s_lastEstopLogMs >= 1000) {
            RelayManager::instance().allOff(true); // Silent re-assert for safety
        }
    }

    // Log once per second to avoid flooding
    if (s_stateTimer - s_lastEstopLogMs >= 1000) {
        ESP_LOGE(TAG, "EMERGENCY STOP active — all relays off");
        s_lastEstopLogMs = s_stateTimer;
    }
    // Recovery only via physical reset or explicit RESET event
}

static void handleFault() {
    if (s_stateTimer <= TASK_PERIOD_MS) {
        ESP_LOGW(TAG, "FAULT: zero torque phase started");
    }

    if (s_stateTimer >= VCU_CONTACTOR_OPEN_DELAY_MS) {
        if (!s_relaysOpenedInFault) {
            RelayManager::instance().allOff(false); // First time, log it
            s_relaysOpenedInFault = true;
        } else if (s_stateTimer - s_lastFaultLogMs >= 1000) {
            RelayManager::instance().allOff(true); // Silent re-assert for safety
        }
    }

    if (s_stateTimer - s_lastFaultLogMs >= 1000) {
        ESP_LOGE(TAG, "FAULT state — send RESET event to recover");
        s_lastFaultLogMs = s_stateTimer;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next) {
    VcuState current = s_state.load(std::memory_order_relaxed);
    ESP_LOGI(TAG, "State: %d → %d", static_cast<int>(current),
             static_cast<int>(next));
    s_state.store(next, std::memory_order_relaxed);
    s_stateTimer = 0;

    if (next == VcuState::EMERGENCY_STOP) {
        s_relaysOpenedInEstop = false;
        s_lastEstopLogMs = (uint32_t)-1000;
    } else if (next == VcuState::FAULT) {
        s_relaysOpenedInFault = false;
        s_lastFaultLogMs = (uint32_t)-1000;
    }
}

static bool pollEvent(VcuEvent& out) {
    if (s_eventQueue == nullptr)
        return false;
    return xQueueReceive(s_eventQueue, &out, 0) == pdTRUE;
}

static TelemetryData getTelemetrySnapshot() {
    if (s_TEL_dataMutex == nullptr)
        return s_TEL_latestData;

    TelemetryData VCU_dataCopy = {};
    xSemaphoreTake(s_TEL_dataMutex, portMAX_DELAY);
    VCU_dataCopy = s_TEL_latestData; // <-- V HARFİ DÜZELTİLDİ
    xSemaphoreGive(s_TEL_dataMutex);
    return VCU_dataCopy;
}

static bool isResetInterlockSatisfied() {
    return isResetInterlockSatisfied(getTelemetrySnapshot(), s_state.load(std::memory_order_relaxed));
}

static bool hasWarningCondition() {
    return hasWarningCondition(getTelemetrySnapshot());
}

static bool hasCriticalCondition() {
    return hasCriticalCondition(getTelemetrySnapshot(), s_state.load(std::memory_order_relaxed));
}

// isReadyEntryPermitted() reddettiğinde hangi koşulun sağlanmadığını döndürür.
// Sıralama predicate ile birebir aynı olmalı; loglama için statik literal
// döndürür (pointer karşılaştırmasıyla "neden değişti mi" tespiti için).
static const char* readyRejectReason(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return "bmsDataValid=0";
    if (hasCriticalCondition(VCU_data, VcuState::IDLE))
        return "kritik kosul aktif";
    if (hasWarningCondition(VCU_data))
        return "uyari kosulu aktif";
#if MOTOR_DRIVER_PRESENT
    if (!VCU_data.TEL_motorDataValid)
        return "motorDataValid=0";
#endif
    return "bilinmiyor";
}

// Motor timeout detection already lives in CanParse::isMotorStatusTimedOut +
// CanManager::updateMotorStatusValidity; if the Teknofest spec needs an
// error-flag bit for this, it should hook into that timeout path, not a
// separate one here (separate task).

#ifdef VCU_LOGIC_TESTABLE
void resetForTest() {
    s_state.store(VcuState::INIT, std::memory_order_relaxed);
    s_stateTimer = 0;
    s_TEL_latestData = {};
    s_VCU_warningLogged = false;
    s_eStopPending.store(false, std::memory_order_relaxed);

    s_relaysOpenedInEstop = false;
    s_relaysOpenedInFault = false;
    s_lastEstopLogMs = 0;
    s_lastFaultLogMs = 0;
    s_lastReadyRejectReason = nullptr;
    s_lastReadyRejectLogMs = 0;

    // Olay kuyruğunu (queue) boşalt
    if (s_eventQueue != nullptr) {
        VcuEvent drained = VcuEvent::NONE;
        while (xQueueReceive(s_eventQueue, &drained, 0) == pdTRUE) {
            // discard
        }
    }
}
#endif

}  // namespace VcuLogic