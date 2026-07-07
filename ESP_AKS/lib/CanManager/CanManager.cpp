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

    g_config.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED;
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

    uint32_t alerts;
    if (twai_read_alerts(&alerts, 0) == ESP_OK) {
        if (alerts & TWAI_ALERT_BUS_OFF) {
            if (!CAN_busOffLogged) {
                ESP_LOGE(TAG, "CAN BUS-OFF detected, initiating recovery");
                CAN_busOffLogged = true;
                CAN_busRecoveredLogged = false;
            }
            twai_initiate_recovery();
        }
        if (alerts & TWAI_ALERT_BUS_RECOVERED) {
            if (!CAN_busRecoveredLogged) {
                ESP_LOGI(TAG, "CAN BUS RECOVERED, restarting driver");
                CAN_busRecoveredLogged = true;
                CAN_busOffLogged = false;
            }
            twai_start();
        }
    }

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
                case CAN_ID_LB_CHARGER_CMD:
                    // BMS -> Charger komutu (DOĞRULANDI decode);
                    // AKS yalnızca DİNLER, hiçbir zaman göndermez.
                    handleCharger1806E5F4(msg);
                    break;
                // Aşağıdakiler DOĞRULANMADI — alan hipotezleri için bkz.
                // Documents/CAN_Message_Table.md. Stub yalnızca ham hex dump
                // basar; TelemetryData'ya ve karar mantığına bağlanmaz.
                case CAN_ID_LB_BMS_E001:  // Sıcaklıklar (DOĞRULANDI)
                    handleLbBmsE001(msg);
                    break;
                case CAN_ID_LB_BMS_E002:  // sabit limit/config adayı; E004 ile multiplex
                case CAN_ID_LB_BMS_E003:
                case CAN_ID_LB_BMS_E004:  // sabit limit/config adayı; E002 ile multiplex
                case CAN_ID_LB_BMS_E005:  // sabit limit/config adayı
                case CAN_ID_LB_BMS_E032:  // gözlemlenen oturumda hep sıfır — reserved/heartbeat adayı
                case CAN_ID_LB_BMS_E033:  // gözlemlenen oturumda hep sıfır — reserved/heartbeat adayı
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
                    // Gözlemlenen oturumlarda payload hep sıfır — anlamı
                    // bilinmiyor (reserved/heartbeat adayı), stub'da kalır.
                    handleLbBmsStub(msg, msg.identifier);
                    break;
                default:
                    ESP_LOGD(TAG, "Unknown STD CAN ID: 0x%03lX", msg.identifier);
                    break;
            }
        }
    }

    updateMotorStatusValidity();
    updateBmsValidity();
    updateChargerValidity();
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

    // Pack voltajı güvenlik eşiği — yalnızca DOĞRULANMIŞ sinyal (packV) ile.
    // Saf kontrol CanParse'ta; eşikler 24S LiFePO4 spec'inden (SystemConfig.h).
    const CanParse::BmsPackVoltageFault CAN_packVoltFault =
        CanParse::checkPackVoltageFault(parsed.TEL_bmsPackVoltageDeciV,
                                        BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V,
                                        BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V);
    const uint8_t CAN_newPackFaultFlags =
        (CAN_packVoltFault == CanParse::BmsPackVoltageFault::UNDERVOLTAGE
             ? 0x01
             : 0) |
        (CAN_packVoltFault == CanParse::BmsPackVoltageFault::OVERVOLTAGE
             ? 0x02
             : 0);

    uint8_t CAN_previousPackFaultFlags = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // DOĞRULANDI: packV
    s_telemetryData.TEL_bmsPackVoltageDeciV = parsed.TEL_bmsPackVoltageDeciV;

    // DOĞRULANDI: Akım ve SoC değerleri TelemetryData'ya aktarılıyor
    s_telemetryData.TEL_bmsCurrentCentiA = parsed.TEL_bmsCurrentCentiA;
    s_telemetryData.TEL_bmsSocHundredths = parsed.TEL_bmsSocHundredths;

    CAN_lastBmsE000Tick = xTaskGetTickCount();
    CAN_hasSeen_BmsE000 = true;
    CAN_bmsE000Valid = true;
    CAN_bmsTimeoutLogged = false;
    s_telemetryData.TEL_bmsDataValid = CAN_bmsE000Valid;
    s_telemetryData.TEL_bmsTimeoutActive = false;

    CAN_previousPackFaultFlags = CAN_bmsPackFaultFlags;
    CAN_bmsPackFaultFlags = CAN_newPackFaultFlags;

    xSemaphoreGive(s_mutex);

    if (CAN_newPackFaultFlags != 0 &&
        CAN_newPackFaultFlags != CAN_previousPackFaultFlags) {
        ESP_LOGE(TAG,
                 "BMS pack voltage %s: %u deciV (%.1f V) — esikler "
                 "[%u..%u] deciV, FAULT bildiriliyor",
                 (CAN_newPackFaultFlags & 0x01) ? "UNDERVOLTAGE" : "OVERVOLTAGE",
                 parsed.TEL_bmsPackVoltageDeciV,
                 parsed.TEL_bmsPackVoltageDeciV * 0.1f,
                 (unsigned)BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V,
                 (unsigned)BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V);
    }

    // Motor errorFlags ile aynı mekanizma: değişimde CAN_Event::FAULT_DETECTED
    // yayınlanır (main.cpp -> VcuLogic::postEvent(FAULT_DETECTED) -> FAULT).
    notifyFaultIfNeeded(CAN_previousPackFaultFlags, CAN_newPackFaultFlags,
                        "BMS packV");

    ESP_LOGD(TAG, "LB-E000: packV=%u deciV (%.1f V)",
             parsed.TEL_bmsPackVoltageDeciV,
             parsed.TEL_bmsPackVoltageDeciV * 0.1f);
}

void CanManager::handleLbBmsE001(const twai_message_t& msg) {
    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "LB BMS E001 received before mutex initialization");
        return;
    }

    TelemetryData parsed{};
    if (!CanParse::parseLbBmsE001(msg, parsed)) {
        ESP_LOGW(TAG, "LB BMS E001 DLC too short: %d", msg.data_length_code);
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_telemetryData.TEL_bmsTempHighestC = parsed.TEL_bmsTempHighestC;
    s_telemetryData.TEL_bmsTempLowestC = parsed.TEL_bmsTempLowestC;
    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "LB-E001: temp1=%d C, temp2=%d C",
             parsed.TEL_bmsTempHighestC,
             parsed.TEL_bmsTempLowestC);
}

// 0x1806E5F4 — Charger komut frame'i (BMS -> Charger, DOĞRULANDI decode).
// AKS bu frame'i yalnızca DİNLER; hiçbir TX yolu yoktur. Setpoint'ler karar
// mantığına bağlanmaz, yalnızca gözlem/telemetri amaçlı saklanır.
void CanManager::handleCharger1806E5F4(const twai_message_t& msg) {
    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "Charger frame received before mutex initialization");
        return;
    }

    ChargerCommand parsed{};
    if (!CanParse::parseCharger1806E5F4(msg, parsed)) {
        ESP_LOGW(TAG, "Charger 1806E5F4 DLC too short: %d",
                 msg.data_length_code);
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_chargerCommand = parsed;
    CAN_lastChargerTick = xTaskGetTickCount();
    CAN_hasSeenCharger = true;
    CAN_chargerValid = true;
    CAN_chargerStaleLogged = false;
    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "Charger-1806E5F4: Vset=%u deciV (%.1f V), Iset=%u deciA (%.1f A)",
             parsed.chargeVoltageSetpointDeciV,
             parsed.chargeVoltageSetpointDeciV * 0.1f,
             parsed.chargeCurrentSetpointDeciA,
             parsed.chargeCurrentSetpointDeciA * 0.1f);
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
        // KARAR (ekip-karari cozuldu): Post-reception BMS timeout, motor
        // timeout ile ayni yoldan eskale edilir — TEL_bmsTimeoutActive
        // set edilir, VcuLogic hasCriticalCondition() IDLE disindaysa
        // FAULT'a gecirir (allOff). Pre-reception (hic E000 gorulmemis)
        // durumda bayrak set EDILMEZ ve TEL_bmsDataValid false kalir; arac
        // BMS'siz baslarken IDLE'da kalir. IDLE->READY gecisi artik
        // VcuLogic::isReadyEntryPermitted() ile korunuyor (TEL_bmsDataValid
        // sart), dolayisiyla BMS verisi hic gelmemisken START HV bus'i
        // enerjilendiremez — bu kontaktor kapama yolu gercekten taze veri gerektirir.
        if (CAN_hasSeen_BmsE000) {
            s_telemetryData.TEL_bmsTimeoutActive = true;
            if (!CAN_bmsTimeoutLogged) {
                CAN_shouldLogTimeout = true;
                CAN_bmsTimeoutLogged = true;
            }
        }
    }

    xSemaphoreGive(s_mutex);

    if (CAN_shouldLogTimeout) {
        ESP_LOGE(TAG,
                 "BMS status timeout after %d ms — IDLE disinda kritik fault "
                 "(TEL_bmsTimeoutActive)",
                 CAN_BMS_STATUS_TIMEOUT_MS);
    }
}

void CanManager::updateChargerValidity() {
    if (s_mutex == nullptr)
        return;

    const TickType_t CAN_nowTick = xTaskGetTickCount();
    bool CAN_shouldLogStale = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // Timeout mantığı E000 ile aynı saf yardımcıyı kullanır; fark, sonucun
    // eskalasyonudur: charger akışı OPSİYONEL olduğundan (araç sürüşteyken
    // charger bağlı olmayabilir) bayatlama yalnızca CAN_chargerValid'i
    // düşürür — CAN_Event/FAULT ÜRETİLMEZ ve TEL_bmsDataValid ETKİLENMEZ.
    if (CanParse::isBmsStatusTimedOut(CAN_hasSeenCharger, CAN_chargerValid,
                                      CAN_nowTick, CAN_lastChargerTick,
                                      pdMS_TO_TICKS(CAN_CHARGER_TIMEOUT_MS))) {
        CAN_chargerValid = false;
        CAN_shouldLogStale = !CAN_chargerStaleLogged;
        CAN_chargerStaleLogged = true;
    }

    xSemaphoreGive(s_mutex);

    if (CAN_shouldLogStale) {
        ESP_LOGD(TAG, "Charger frame stale after %d ms (opsiyonel akış — FAULT üretmez)",
                 CAN_CHARGER_TIMEOUT_MS);
    }
}

bool CanManager::getChargerCommand(ChargerCommand& out) const {
    if (s_mutex == nullptr) {
        out = s_chargerCommand;
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    out = s_chargerCommand;
    const bool CAN_isFresh = CAN_chargerValid;
    xSemaphoreGive(s_mutex);
    return CAN_isFresh;
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