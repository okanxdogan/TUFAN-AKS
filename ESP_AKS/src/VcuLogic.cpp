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
static VcuState s_state = VcuState::INIT;
static QueueHandle_t s_eventQueue = nullptr;
static uint32_t s_stateTimer = 0;
static TelemetryData s_TEL_latestData = {};
static bool s_VCU_warningLogged = false;
static SemaphoreHandle_t s_TEL_dataMutex = nullptr;
// E-STOP bypass: set atomically in postEvent so queue saturation
// cannot swallow an emergency stop command.
static std::atomic<bool> s_eStopPending{false};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next);
static bool pollEvent(VcuEvent& out);
static bool isResetInterlockSatisfied();
static bool hasWarningCondition();
static bool hasCriticalCondition();
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
        if (s_state != VcuState::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        // Already in EMERGENCY_STOP — clear flag but let run() continue
        // so handleEmergencyStop() keeps executing (timer must not reset).
    }

    if ((s_state == VcuState::IDLE || s_state == VcuState::READY ||
         s_state == VcuState::DRIVE) &&
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
        if (event == VcuEvent::RESET &&
            (s_state == VcuState::FAULT ||
             s_state == VcuState::EMERGENCY_STOP)) {
            if (!isResetInterlockSatisfied()) {
                ESP_LOGW(TAG, "RESET rejected: safety interlock still active");
                return;
            }
            transitionTo(VcuState::IDLE);
            return;
        }

        // State-specific event handling
        switch (s_state) {
            case VcuState::IDLE:
                if (event == VcuEvent::START_REQUEST)
                    transitionTo(VcuState::READY);
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
    switch (s_state) {
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
    return s_state;
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
        RelayManager::instance().allOff();
    }

    // Log once per second to avoid flooding
    static uint32_t lastLog = 0;
    if (s_stateTimer - lastLog >= 1000) {
        ESP_LOGE(TAG, "EMERGENCY STOP active — all relays off");
        lastLog = s_stateTimer;
    }
    // Recovery only via physical reset or explicit RESET event
}

static void handleFault() {
    if (s_stateTimer <= TASK_PERIOD_MS) {
        ESP_LOGW(TAG, "FAULT: zero torque phase started");
    }

    if (s_stateTimer >= VCU_CONTACTOR_OPEN_DELAY_MS) {
        RelayManager::instance().allOff();
    }

    static uint32_t lastLog = 0;
    if (s_stateTimer - lastLog >= 1000) {
        ESP_LOGE(TAG, "FAULT state — send RESET event to recover");
        lastLog = s_stateTimer;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next) {
    ESP_LOGI(TAG, "State: %d → %d", static_cast<int>(s_state),
             static_cast<int>(next));
    s_state = next;
    s_stateTimer = 0;
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
    VCU_dataCopy = s_TEL_latestData;
    xSemaphoreGive(s_TEL_dataMutex);
    return VCU_dataCopy;
}

static bool isResetInterlockSatisfied() {
    return isResetInterlockSatisfied(getTelemetrySnapshot(), s_state);
}

static bool hasWarningCondition() {
    return hasWarningCondition(getTelemetrySnapshot());
}

static bool hasCriticalCondition() {
    return hasCriticalCondition(getTelemetrySnapshot(), s_state);
}

#ifdef VCU_LOGIC_TESTABLE
void resetForTest() {
    s_state = VcuState::INIT;
    s_stateTimer = 0;
    s_TEL_latestData = {};
    s_VCU_warningLogged = false;
    s_eStopPending.store(false, std::memory_order_relaxed);

    // Olay queue'sunu boşalt — kalıntı event'lerin sonraki teste sızmaması için.
    if (s_eventQueue != nullptr) {
        VcuEvent drained = VcuEvent::NONE;
        while (xQueueReceive(s_eventQueue, &drained, 0) == pdTRUE) {
            // discard
        }
    }
}
#endif

}  // namespace VcuLogic
