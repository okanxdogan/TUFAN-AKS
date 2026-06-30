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

        switch (msg.identifier) {
            case CAN_ID_MOTOR_STATUS:
                handleMotorStatus(msg);
                break;

            case CAN_ID_SOLION_BMS_A:
                handleSolionBmsA(msg);
                break;

            case CAN_ID_SOLION_BMS_B:
                handleSolionBmsB(msg);
                break;

            case CAN_ID_BMS_STATUS:
                ESP_LOGD(TAG, "Legacy BMS frame received: 0x%03lX",
                         msg.identifier);
                break;

            default:
                ESP_LOGD(TAG, "Unknown CAN ID: 0x%03lX", msg.identifier);
                break;
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
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    CAN_telemetryCopy = s_telemetryData;
    xSemaphoreGive(s_mutex);
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

void CanManager::handleSolionBmsA(const twai_message_t& msg) {
    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "Solion BMS-A received before mutex initialization");
        return;
    }

    TelemetryData parsed{};
    if (!CanParse::parseSolionBmsA(msg, parsed)) {
        ESP_LOGW(TAG, "Solion BMS-A DLC too short: %d", msg.data_length_code);
        return;
    }

    uint8_t CAN_prevState = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    CAN_prevState = CAN_prevBmsSystemState;

    s_telemetryData.TEL_bmsCellVoltageMaxDeciMv = parsed.TEL_bmsCellVoltageMaxDeciMv;
    s_telemetryData.TEL_bmsCellVoltageMinDeciMv = parsed.TEL_bmsCellVoltageMinDeciMv;
    s_telemetryData.TEL_bmsTempHighestC = parsed.TEL_bmsTempHighestC;
    s_telemetryData.TEL_bmsTempLowestC  = parsed.TEL_bmsTempLowestC;
    s_telemetryData.TEL_bmsSystemState  = parsed.TEL_bmsSystemState;
    CAN_prevBmsSystemState = parsed.TEL_bmsSystemState;
    CAN_lastBmsConfigTick = xTaskGetTickCount();
    CAN_hasSeen_BmsConfig = true;
    CAN_bmsConfigValid = true;
    CAN_bmsTimeoutLogged = false;
    s_telemetryData.TEL_bmsDataValid = CAN_bmsConfigValid && CAN_bmsLiveValid;

    xSemaphoreGive(s_mutex);

    if (parsed.TEL_bmsSystemState == 4 && CAN_prevState != 4) {
        ESP_LOGW(TAG, "BMS entered FAULT state");
        if (CAN_eventCallback != nullptr)
            CAN_eventCallback(CAN_Event::FAULT_DETECTED, CAN_eventContext);
    }

    ESP_LOGD(TAG, "BMS-A: cellMax=%u cellMin=%u deciMv, tempH=%d tempL=%d C, state=%u",
             parsed.TEL_bmsCellVoltageMaxDeciMv, parsed.TEL_bmsCellVoltageMinDeciMv,
             parsed.TEL_bmsTempHighestC, parsed.TEL_bmsTempLowestC,
             parsed.TEL_bmsSystemState);
}

void CanManager::handleSolionBmsB(const twai_message_t& msg) {
    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "Solion BMS-B received before mutex initialization");
        return;
    }

    TelemetryData parsed{};
    if (!CanParse::parseSolionBmsB(msg, parsed)) {
        ESP_LOGW(TAG, "Solion BMS-B DLC too short: %d", msg.data_length_code);
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    s_telemetryData.TEL_bmsPackVoltageDeciV = parsed.TEL_bmsPackVoltageDeciV;
    s_telemetryData.TEL_bmsCurrentCentiMa   = parsed.TEL_bmsCurrentCentiMa;
    s_telemetryData.TEL_bmsSocHundredths    = parsed.TEL_bmsSocHundredths;
    CAN_lastBmsLiveTick = xTaskGetTickCount();
    CAN_hasSeen_BmsLive = true;
    CAN_bmsLiveValid = true;
    CAN_bmsTimeoutLogged = false;
    s_telemetryData.TEL_bmsDataValid = CAN_bmsConfigValid && CAN_bmsLiveValid;

    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "BMS-B: pack=%u deciV, current=%ld centi-mA, soc=%u (x0.01%%)",
             parsed.TEL_bmsPackVoltageDeciV, parsed.TEL_bmsCurrentCentiMa,
             parsed.TEL_bmsSocHundredths);
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

    if (CanParse::isBmsStatusTimedOut(CAN_hasSeen_BmsConfig, CAN_bmsConfigValid,
                                      CAN_nowTick, CAN_lastBmsConfigTick,
                                      CAN_timeoutTicks)) {
        CAN_bmsConfigValid = false;
    }

    if (CanParse::isBmsStatusTimedOut(CAN_hasSeen_BmsLive, CAN_bmsLiveValid,
                                      CAN_nowTick, CAN_lastBmsLiveTick,
                                      CAN_timeoutTicks)) {
        CAN_bmsLiveValid = false;
    }

    if (!CAN_bmsConfigValid || !CAN_bmsLiveValid) {
        s_telemetryData.TEL_bmsDataValid = false;
        // TODO(ekip-karari): BMS timeout'ta allOff tetiklensin mi?
        // Motor timeout'u TEL_motorTimeoutActive -> VcuLogic FAULT yoluyla allOff
        // tetikliyor (motor kontrolsuz kalabilir). BMS verisi bayatladiginda
        // arac gucunu kesmek gerekli olabilir — ekip ve danismanla karara baglanmali.
        if ((CAN_hasSeen_BmsConfig || CAN_hasSeen_BmsLive) &&
            !CAN_bmsTimeoutLogged) {
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
