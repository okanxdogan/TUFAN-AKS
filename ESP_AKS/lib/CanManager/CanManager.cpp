#include "CanManager.h"
#include "SystemConfig.h"
#include "esp_err.h"
#include "esp_log.h"

static constexpr const char* TAG = "CanManager";

CanManager::CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin)
    : g_config(TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL)),
      t_config(TWAI_TIMING_CONFIG_500KBITS()),
      f_config(TWAI_FILTER_CONFIG_ACCEPT_ALL()) {}

void CanManager::setEventCallback(CAN_EventCallback CAN_callback,
                                  void* CAN_context) {
    CAN_eventCallback = CAN_callback;
    CAN_eventContext = CAN_context;
}

bool CanManager::begin() {
    if (isInitialized)
        return true;

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(s_mutex);
        s_mutex = nullptr;
        return false;
    }

    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        vSemaphoreDelete(s_mutex);
        s_mutex = nullptr;
        return false;
    }

    isInitialized = true;
    ESP_LOGI(TAG, "CAN initialized at 500kbps");
    return true;
}

/* bool CanManager::sendTorqueCommand(uint16_t torqueValue) {
    if (!isInitialized)
        return false;

    twai_message_t msg = {};
    msg.identifier = CAN_ID_TORQUE_CMD;
    msg.data_length_code = 2;
    msg.data[0] = static_cast<uint8_t>(torqueValue >> 8);
    msg.data[1] = static_cast<uint8_t>(torqueValue & 0xFF);
    msg.flags = TWAI_MSG_FLAG_NONE;

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(10));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Torque TX failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
*/

void CanManager::processRxMessages() {
    if (!isInitialized)
        return;

    twai_message_t msg;
    // Process up to 5 messages per call to avoid blocking the task
    for (int i = 0; i < 5; i++) {
        esp_err_t err = twai_receive(&msg, 0);  // non-blocking
        if (err == ESP_ERR_TIMEOUT)
            break;  // no more messages
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "RX error: %s", esp_err_to_name(err));
            break;
        }

        if (msg.extd) {
            // --- 29-bit Extended Frame ID'ler ---
            switch (msg.identifier) {
                case CAN_ID_LB_BMS_E000:
                    handleLbBmsE000(msg);
                    break;
                case CAN_ID_LB_BMS_E001:
                case CAN_ID_LB_BMS_E002:
                case CAN_ID_LB_BMS_E003:
                case CAN_ID_LB_BMS_E004:
                case CAN_ID_LB_BMS_E005:
                case CAN_ID_LB_BMS_E032:
                case CAN_ID_LB_BMS_E033:
                    handleLbBmsStub(msg, msg.identifier);
                    break;
                default:
                    ESP_LOGD(TAG, "Unknown EXT CAN ID: 0x%08lX", msg.identifier);
                    break;
            }
        } else {
            // --- 11-bit Standard Frame ID'ler ---
            switch (msg.identifier) {
                case CAN_ID_MOTOR_STATUS:
                    handleMotorStatus(msg);
                    break;
                case CAN_ID_LB_STD_0x000:
                    handleLbBmsStub(msg, msg.identifier);
                    break;
                case CAN_ID_BMS_STATUS:
                    ESP_LOGD(TAG, "Legacy BMS frame received: 0x%03lX",
                             msg.identifier);
                    break;
                default:
                    ESP_LOGD(TAG, "Unknown STD CAN ID: 0x%03lX", msg.identifier);
                    break;
            }
        }
    }

    updateMotorStatusValidity();
    updateBmsValidity();
}

MotorStatus CanManager::getMotorStatus() const {
    if (s_mutex == nullptr)
        return s_motorStatus;

    MotorStatus CAN_statusCopy = {};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    CAN_statusCopy = s_motorStatus;
    xSemaphoreGive(s_mutex);
    return CAN_statusCopy;
}

TelemetryData CanManager::getTelemetryData() const {
    if (s_mutex == nullptr)
        return s_telemetryData;

    TelemetryData CAN_telemetryCopy = {};
    const TickType_t CAN_nowTick = xTaskGetTickCount();
    bool CAN_shouldLogSysState = false;
    bool CAN_shouldLogSoc = false;
    bool CAN_shouldLogCurrent = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    CAN_telemetryCopy = s_telemetryData;

    // UKS Decode_Line, alan bazinda sert aralik kontrolu yapar ve TEK
    // alan aralik disindaysa TUM frame'i reddeder (parse_fail) — bu
    // yuzden UKS'e giden anlik goruntu, gonderilmeden hemen once burada
    // sanitize edilir. Ham CAN parse (CanParse.cpp) ve ic durum
    // (s_telemetryData) DEGISTIRILMEZ; yalnizca disariya kopyalanan
    // deger duzeltilir.
    const uint8_t CAN_sanitizedState =
        TelemetrySanitize::sanitizeSystemState(CAN_telemetryCopy.TEL_bmsSystemState);
    if (CAN_sanitizedState != CAN_telemetryCopy.TEL_bmsSystemState) {
        CAN_telemetryCopy.TEL_bmsSystemState = CAN_sanitizedState;
        if (static_cast<TickType_t>(CAN_nowTick - CAN_lastSysStateSanitizeWarnTick) >=
            pdMS_TO_TICKS(TEL_SANITIZE_WARN_THROTTLE_MS)) {
            CAN_shouldLogSysState = true;
            CAN_lastSysStateSanitizeWarnTick = CAN_nowTick;
        }
    }

    const uint16_t CAN_sanitizedSoc =
        TelemetrySanitize::sanitizeSoc(CAN_telemetryCopy.TEL_bmsSocHundredths);
    if (CAN_sanitizedSoc != CAN_telemetryCopy.TEL_bmsSocHundredths) {
        CAN_telemetryCopy.TEL_bmsSocHundredths = CAN_sanitizedSoc;
        if (static_cast<TickType_t>(CAN_nowTick - CAN_lastSocSanitizeWarnTick) >=
            pdMS_TO_TICKS(TEL_SANITIZE_WARN_THROTTLE_MS)) {
            CAN_shouldLogSoc = true;
            CAN_lastSocSanitizeWarnTick = CAN_nowTick;
        }
    }

    const int32_t CAN_sanitizedCurrent =
        TelemetrySanitize::sanitizeCurrent(CAN_telemetryCopy.TEL_bmsCurrentCentiMa);
    if (CAN_sanitizedCurrent != CAN_telemetryCopy.TEL_bmsCurrentCentiMa) {
        CAN_telemetryCopy.TEL_bmsCurrentCentiMa = CAN_sanitizedCurrent;
        if (static_cast<TickType_t>(CAN_nowTick - CAN_lastCurrentSanitizeWarnTick) >=
            pdMS_TO_TICKS(TEL_SANITIZE_WARN_THROTTLE_MS)) {
            CAN_shouldLogCurrent = true;
            CAN_lastCurrentSanitizeWarnTick = CAN_nowTick;
        }
    }

    xSemaphoreGive(s_mutex);

    if (CAN_shouldLogSysState)
        ESP_LOGW(TAG, "BMS sysState UKS araligi disinda (1..4) — FAULT'a sanitize edildi");
    if (CAN_shouldLogSoc)
        ESP_LOGW(TAG, "BMS soc UKS araligi disinda (0..10000) — clamp edildi");
    if (CAN_shouldLogCurrent)
        ESP_LOGW(TAG, "BMS current == INT32_MIN, UKS sinirina gore +1 kaydirildi");

    return CAN_telemetryCopy;
}

void CanManager::handleMotorStatus(const twai_message_t& msg) {
    MotorStatus parsed{};
    if (!CanParse::parseMotorStatus(msg, parsed)) {
        ESP_LOGW(TAG, "Motor status DLC too short: %d", msg.data_length_code);
        return;
    }

    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "Motor status received before mutex initialization");
        return;
    }

    uint8_t CAN_previousMotorErrorFlags = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    CAN_previousMotorErrorFlags = s_motorStatus.errorFlags;
    s_motorStatus = parsed;
    CAN_lastMotorStatusTick = xTaskGetTickCount();
    CAN_hasSeenMotorStatus = true;
    CAN_motorTimeoutLogged = false;

    s_telemetryData.TEL_motorRpm = s_motorStatus.rpm;
   // s_telemetryData.TEL_motorTorqueFeedback = s_motorStatus.torqueFeedback;
    s_telemetryData.TEL_motorErrorFlags = s_motorStatus.errorFlags;
    s_telemetryData.TEL_motorDataValid = s_motorStatus.isValid;
    s_telemetryData.TEL_motorTimeoutActive = false;

    xSemaphoreGive(s_mutex);

    notifyFaultIfNeeded(CAN_previousMotorErrorFlags, s_motorStatus.errorFlags,
                        "Motor");

 //   ESP_LOGD(TAG, "Motor: RPM=%d, Torque=%d", s_motorStatus.rpm,
  //           s_motorStatus.torqueFeedback);
}

// =========================================================================
// Lithium Balance c-BMS Handlers
// =========================================================================

void CanManager::handleLbBmsE000(const twai_message_t& msg) {
    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "LB BMS E000 received before mutex initialization");
        return;
    }

    TelemetryData parsed{};
    if (!CanParse::parseLbBmsE000(msg, parsed)) {
        ESP_LOGW(TAG, "LB BMS E000 DLC too short: %d", msg.data_length_code);
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // DOĞRULANDI: packV
    s_telemetryData.TEL_bmsPackVoltageDeciV = parsed.TEL_bmsPackVoltageDeciV;

    CAN_lastBmsE000Tick = xTaskGetTickCount();
    CAN_hasSeen_BmsE000 = true;
    CAN_bmsE000Valid = true;
    CAN_bmsTimeoutLogged = false;
    s_telemetryData.TEL_bmsDataValid = CAN_bmsE000Valid;

    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "LB-E000: packV=%u deciV (%.1f V)",
             parsed.TEL_bmsPackVoltageDeciV,
             parsed.TEL_bmsPackVoltageDeciV * 0.1f);
}

void CanManager::handleLbBmsStub(const twai_message_t& msg, uint32_t canId) {
    // TODO: alan anlamı doğrulanmadı — ham byte'ları debug log'a bas,
    // TelemetryData'ya hiçbir anlam yüklenmiyor.
    if (msg.data_length_code == 0)
        return;

    // Debug log: ID + ham byte dump (yalnızca VERBOSE/DEBUG seviyesinde görünür)
    char hexBuf[8 * 3 + 1] = {};
    for (uint8_t i = 0; i < msg.data_length_code && i < 8; i++) {
        snprintf(hexBuf + i * 3, 4, "%02X ", msg.data[i]);
    }
    ESP_LOGD(TAG, "LB-0x%04lX [DLC=%d]: %s(DOĞRULANMADI — ham veri)",
             (unsigned long)canId, msg.data_length_code, hexBuf);
}

void CanManager::updateMotorStatusValidity() {
    if (s_mutex == nullptr)
        return;

    const TickType_t CAN_nowTick = xTaskGetTickCount();
    bool CAN_shouldLogTimeout = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (CanParse::isMotorStatusTimedOut(
            CAN_hasSeenMotorStatus, s_motorStatus.isValid, CAN_nowTick,
            CAN_lastMotorStatusTick,
            pdMS_TO_TICKS(CAN_MOTOR_STATUS_TIMEOUT_MS))) {
        s_motorStatus.isValid = false;
        s_telemetryData.TEL_motorDataValid = false;
        s_telemetryData.TEL_motorTimeoutActive = true;
        CAN_shouldLogTimeout = !CAN_motorTimeoutLogged;
        CAN_motorTimeoutLogged = true;
    }

    xSemaphoreGive(s_mutex);

    if (CAN_shouldLogTimeout) {
        ESP_LOGW(TAG, "Motor status timeout after %d ms",
                 CAN_MOTOR_STATUS_TIMEOUT_MS);
    }
}

void CanManager::updateBmsValidity() {
    if (s_mutex == nullptr)
        return;

    const TickType_t CAN_nowTick = xTaskGetTickCount();
    const TickType_t CAN_timeoutTicks = pdMS_TO_TICKS(CAN_BMS_STATUS_TIMEOUT_MS);
    bool CAN_shouldLogTimeout = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (CanParse::isBmsStatusTimedOut(CAN_hasSeen_BmsE000, CAN_bmsE000Valid,
                                      CAN_nowTick, CAN_lastBmsE000Tick,
                                      CAN_timeoutTicks)) {
        CAN_bmsE000Valid = false;
    }

    if (!CAN_bmsE000Valid) {
        s_telemetryData.TEL_bmsDataValid = false;
        // TODO(ekip-karari): BMS timeout'ta allOff tetiklensin mi?
        // Motor timeout'u TEL_motorTimeoutActive -> VcuLogic FAULT yoluyla allOff
        // tetikliyor (motor kontrolsuz kalabilir). BMS verisi bayatladiginda
        // arac gucunu kesmek gerekli olabilir — ekip ve danismanla karara baglanmali.
        if (CAN_hasSeen_BmsE000 && !CAN_bmsTimeoutLogged) {
            CAN_shouldLogTimeout = true;
            CAN_bmsTimeoutLogged = true;
        }
    }

    xSemaphoreGive(s_mutex);

    if (CAN_shouldLogTimeout) {
        ESP_LOGW(TAG, "BMS status timeout after %d ms", CAN_BMS_STATUS_TIMEOUT_MS);
    }
}

void CanManager::notifyFaultIfNeeded(uint8_t CAN_previousFlags,
                                     uint8_t CAN_currentFlags,
                                     const char* CAN_faultSource) {
    if (CAN_currentFlags == 0 || CAN_currentFlags == CAN_previousFlags)
        return;

    ESP_LOGW(TAG, "%s error flags: 0x%02X", CAN_faultSource, CAN_currentFlags);
    if (CAN_eventCallback != nullptr) {
        CAN_eventCallback(CAN_Event::FAULT_DETECTED, CAN_eventContext);
    }
}
