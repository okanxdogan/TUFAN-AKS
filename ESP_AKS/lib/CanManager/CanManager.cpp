#include "CanManager.h"
#include "BmsFreshness.h"         // G12: E000+E001 birleşik tazelik (saf)
#include "MotorFaultDebounce.h"  // G9: motorErrorFaultConfirmed (saf debounce)
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

    // G6: RX kuyruğunu derinleştir (varsayılan 5 → 32) ve kuyruk-dolu
    // alarmını etkinleştir; böylece BMS burst'lerinde ALARMSIZ frame düşmez.
    g_config.rx_queue_len = CAN_RX_QUEUE_LEN;
    g_config.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED |
                              TWAI_ALERT_RX_QUEUE_FULL;
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

// Motor sürücüsü torque komutu. Motor sürücüsü henüz araca entegre DEĞİL
// (MOTOR_DRIVER_PRESENT=0): bu fazda GERÇEK FRAME GÖNDERİLMEZ. E-STOP/FAULT
// güvenli kapanış sırası (VcuLogic) bu fonksiyonu torque(0) ile çağırır;
// bayrak 0 iken yalnızca bir kez uyarı loglanır (E-STOP yolunda spam yok) ve
// false döner (frame üretilmedi). Bkz. Documents/MOTOR_ENTEGRASYON_NOTU.md.
bool CanManager::sendTorqueCommand(uint16_t torqueValue) {
    if (!isInitialized)
        return false;

#if MOTOR_DRIVER_PRESENT
    // TODO(motor entegrasyonu): GERÇEK torque frame'i burada kurulacak.
    // CAN ID ve frame formatı motor sürücü spec'i gelince tanımlanacak
    // (UYDURMA YOK). Fonksiyon imzası, çağrı noktaları ve E-STOP/FAULT
    // sıralaması ŞİMDİ hazır; yalnızca frame içeriği ve twai_transmit çağrısı
    // eksik:
    //   twai_message_t msg = {};
    //   msg.identifier = CAN_ID_TORQUE_CMD;              // ID doğrulanacak
    //   msg.data_length_code = /* spec */;               // format doğrulanacak
    //   ...
    //   return twai_transmit(&msg, pdMS_TO_TICKS(10)) == ESP_OK;
    (void)torqueValue;
    ESP_LOGW(TAG, "sendTorqueCommand: MOTOR_DRIVER_PRESENT=1 ama frame TODO");
    return false;
#else
    (void)torqueValue;
    if (!CAN_torqueSkipLogged) {
        ESP_LOGW(TAG,
                 "torque cmd atlandi (motor surucusu yok — MOTOR_DRIVER_PRESENT=0)");
        CAN_torqueSkipLogged = true;
    }
    return false;  // frame ÜRETİLMEDİ
#endif
}

// G6 test notu: Bu fonksiyonun drain-döngüsü + RTR filtresi native'de test
// EDİLMİYOR. CanManager platformio.ini'de native `lib_ignore` altındadır ve
// idf_stubs twai_message_t yalnız `flags` alanına sahiptir (rtr/extd union'ı
// yok); twai_receive/driver API fake'i de yoktur. Bu davranışı doğrulamak,
// tam bir TWAI sürücü fake'i + scriptable RX kuyruğu gerektirir ki bu, Faz 3
// CanManager orkestrasyon testi KAPSAMINDADIR (bu değişiklikte bilinçli olarak
// yapılmadı). Değişiklik esp32dev derlemesi ile doğrulanır.
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
        if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
            // Kuyruk taştı → donanım frame düşürdü. Sayaç tut, oran-sınırlı
            // özet logla (her olayda değil, en fazla 1 WARN / interval).
            CAN_rxQueueFullCount++;
            TickType_t CAN_now = xTaskGetTickCount();
            if (CAN_now - CAN_lastRxQueueFullLogTick >=
                pdMS_TO_TICKS(CAN_RX_STATS_LOG_INTERVAL_MS)) {
                ESP_LOGW(TAG,
                         "RX queue full — frame düştü (toplam olay=%lu, "
                         "atlanan remote=%lu)",
                         (unsigned long)CAN_rxQueueFullCount,
                         (unsigned long)CAN_rxRemoteFrameCount);
                CAN_lastRxQueueFullLogTick = CAN_now;
            }
        }
    }

    twai_message_t msg;
    // G6: Kuyruğu bu tick'te boşalana kadar işle; üst sınır CAN_RX_DRAIN_MAX
    // (task açlığı / sonsuz döngü emniyeti). rx_queue_len=CAN_RX_QUEUE_LEN
    // olduğundan tek tick'te tüm kuyruk tahliye edilebilir.
    for (int i = 0; i < CAN_RX_DRAIN_MAX; i++) {
        esp_err_t err = twai_receive(&msg, 0);  // non-blocking
        if (err == ESP_ERR_TIMEOUT)
            break;  // no more messages
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "RX error: %s", esp_err_to_name(err));
            break;
        }

        // G6: Remote frame (RTR) — data alanı TANIMSIZ; DLC≥4 olsa bile parse
        // ETME. Say ve atla. (Beklenen DLC minimum kontrolü ise mesaja özel
        // olarak zaten her CanParse::parse* fonksiyonunun başında yapılır —
        // örn. parseLbBmsE000 DLC<8'i, parseMotorStatus DLC<4'ü reddeder.)
        if (msg.rtr) {
            CAN_rxRemoteFrameCount++;
            continue;
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

    uint8_t CAN_previousConfirmedFlags = 0;
    uint8_t CAN_confirmedErrorFlags = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // Önceki ONAYLANMIŞ (debounce sonrası) errorFlags — edge-trigger için.
    CAN_previousConfirmedFlags = s_telemetryData.TEL_motorErrorFlags;
    s_motorStatus = parsed;
    CAN_lastMotorStatusTick = xTaskGetTickCount();
    CAN_hasSeenMotorStatus = true;
    CAN_motorTimeoutLogged = false;

    // G9: geçici (tek/çift frame) errorFlags kontaktör açtırmasın — N ardışık
    // frame onayı (bkz. MotorFaultDebounce.h + MOTOR_ERROR_DEBOUNCE_FRAMES).
    // Onaylanana kadar TEL_motorErrorFlags 0 kalır → VcuLogic FAULT'a GEÇMEZ ve
    // notifyFaultIfNeeded event üretmez; temiz frame gelince sayaç sıfırlanır.
    const bool CAN_motorFaultConfirmed = motorErrorFaultConfirmed(
        parsed.errorFlags, CAN_motorErrorConsecutive, MOTOR_ERROR_DEBOUNCE_FRAMES);
    CAN_confirmedErrorFlags = CAN_motorFaultConfirmed ? parsed.errorFlags : 0;

    // Motor rpm CAN'da işaretli (int16_t) gelir; geri yön dönüşü negatif
    // olabilir. Telemetri/HMI/LoRa sözleşmesi ise rpm'i işaretsiz büyüklük
    // (0..65535, sanity ≤ TEL_RPM_MAX) bekler — bkz. tools/e2e/contract.py.
    // Negatifi doğrudan uint16_t'ye atamak sarmalanıp (~64k) UKS paketini
    // reddettirirdi; bu yüzden mutlak değeri (büyüklüğü) alıyoruz.
    s_telemetryData.TEL_motorRpm = static_cast<uint16_t>(
        s_motorStatus.rpm < 0 ? -static_cast<int32_t>(s_motorStatus.rpm)
                              : static_cast<int32_t>(s_motorStatus.rpm));
    s_telemetryData.TEL_motorVoltageDeciV = s_motorStatus.motorVoltageDeciV;
    s_telemetryData.TEL_motorErrorFlags = CAN_confirmedErrorFlags;
    s_telemetryData.TEL_motorDataValid = s_motorStatus.isValid;
    s_telemetryData.TEL_motorTimeoutActive = false;

    xSemaphoreGive(s_mutex);

    notifyFaultIfNeeded(CAN_previousConfirmedFlags, CAN_confirmedErrorFlags,
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
    // G12: TEL_bmsDataValid / TEL_bmsTimeoutActive artık burada TEK BAŞINA
    // set EDİLMEZ — updateBmsValidity E000 ile E001 tazeliğini BİRLEŞTİRİR
    // (E000 akarken E001 kesilirse bayat sıcaklık maskelenmesin).

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
    CAN_lastBmsE001Tick = xTaskGetTickCount();  // G12: E001 freshness izleme
    CAN_hasSeen_BmsE001 = true;
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

    // G12: BMS verisi packV (E000) VE sıcaklık (E001) iki ayrı ID'den beslenir;
    // biri akıp diğeri kesilirse bayat alan maskelenmesin diye freshness'i ID
    // bazına birleştir (saf bms_evaluate_freshness). TEL_bmsDataValid ancak İKİSİ
    // de taze ise true; görülmüş bir ID bayatladıysa TEL_bmsTimeoutActive set
    // edilir (motor timeout ile aynı yol: VcuLogic hasCriticalCondition() IDLE
    // dışındaysa FAULT'a geçirir; pre-reception hiç görülmemiş durum tolere
    // edilir, IDLE->READY zaten isReadyEntryPermitted TEL_bmsDataValid şartıyla
    // korunur).
    const BmsFreshnessResult CAN_bmsFresh = bms_evaluate_freshness(
        CAN_hasSeen_BmsE000, (uint32_t)CAN_lastBmsE000Tick, CAN_hasSeen_BmsE001,
        (uint32_t)CAN_lastBmsE001Tick, (uint32_t)CAN_nowTick,
        (uint32_t)CAN_timeoutTicks);

    s_telemetryData.TEL_bmsDataValid = CAN_bmsFresh.dataValid;
    s_telemetryData.TEL_bmsTimeoutActive = CAN_bmsFresh.timeoutActive;

    if (CAN_bmsFresh.timeoutActive) {
        if (!CAN_bmsTimeoutLogged) {
            CAN_shouldLogTimeout = true;
            CAN_bmsTimeoutLogged = true;
        }
    } else {
        // Yeniden taze (veya hiç bayatlamamış) → sonraki bayatlamada tekrar logla.
        CAN_bmsTimeoutLogged = false;
    }

    xSemaphoreGive(s_mutex);

    if (CAN_shouldLogTimeout) {
        ESP_LOGE(TAG,
                 "BMS status timeout after %d ms (E000/E001 freshness) — IDLE "
                 "disinda kritik fault (TEL_bmsTimeoutActive)",
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