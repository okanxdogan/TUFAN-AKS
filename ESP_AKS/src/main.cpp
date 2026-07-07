#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <cstdio>

#include "DisplayHMI.h"
#include "CanManager.h"
#include "E22Config.h"
#include "LinkMonitor.h"
#include "LoraLink.h"
#include "LoraRxHandler.h"
#include "OfflineBuffer.h"
#include "UplinkScheduler.h"
#include "RelayManager.h"
#include "SystemConfig.h"
#include "Telemetry.h"
#include "TelemetrySanitize.h"
#include "VcuLogic.h"
#include "VehicleParams.h"

// 24 hücreli BMS gösterge altyapısı (Gerçek veri ile)
#include "BmsModel.h"
#include "BmsAlgo.h"
#include "BmsNextionPacket.h"
#include "HMIHelpers.h"

static constexpr const char *TAG = "APP_MAIN";

// Stack high-water-mark logging interval (ticks)
static constexpr uint32_t STACK_LOG_INTERVAL_MS = 30000;

QueueHandle_t TEL_sensorDataQueue = nullptr;

// --- LoRa link monitörü (M1 refactor) ---
// s_linkDown / s_lastHeartbeatMs / s_unknownRxByteCount / s_lastUnknownRxByteWarnMs
// statikleri UplinkScheduler'a taşındı (tipler DEĞİŞMEDİ — P11'in işi). Cross-
// task LoRa_IsLinkDown() API'si, task'in kurduğu scheduler örneğine bu file-
// scope pointer üzerinden erişir (task ömür boyu çalışır → pointer geçerli).
static UplinkScheduler* s_uplink = nullptr;

extern "C" bool LoRa_IsLinkDown(void) {
    return s_uplink != nullptr && s_uplink->isLinkDown();
}

static HMI_VcuState HMI_mapVcuState(VcuLogic::VcuState HMI_state) {
  switch (HMI_state) {
  case VcuLogic::VcuState::INIT:
    return HMI_VcuState::INIT;
  case VcuLogic::VcuState::IDLE:
    return HMI_VcuState::IDLE;
  case VcuLogic::VcuState::READY:
    return HMI_VcuState::READY;
  case VcuLogic::VcuState::DRIVE:
    return HMI_VcuState::DRIVE;
  case VcuLogic::VcuState::EMERGENCY_STOP:
    return HMI_VcuState::EMERGENCY_STOP;
  case VcuLogic::VcuState::FAULT:
    return HMI_VcuState::FAULT;
  default:
    return HMI_VcuState::FAULT;
  }
}

// 24-hücre BMS Nextion komutlarını HMI UART'ına yazan emit callback'i.
static void BMS_emitNextionCommand(const char* BMS_cmd, size_t BMS_len,
                                   void* BMS_ctx) {
  (void)BMS_ctx;
  uart_write_bytes(HMI_UART_NUM, BMS_cmd, BMS_len);
  HMI_sendEndBytes();
}

static bool HMI_areAllContactorsClosed() {
  for (uint8_t REL_channel = 0; REL_channel < RELAY_TOTAL_CHANNELS;
       ++REL_channel) {
    if (!RelayManager::instance().getRelayState(REL_channel)) {
      return false;
    }
  }
  return true;
}

static void CAN_handleEvent(CAN_Event CAN_event, void* CAN_context) {
  (void)CAN_context;

  switch (CAN_event) {
  case CAN_Event::FAULT_DETECTED:
    ESP_LOGE(TAG, "CAN fault event received");
    VcuLogic::postEvent(VcuLogic::VcuEvent::FAULT_DETECTED);
    break;
  default:
    break;
  }
}

// VcuLogic torque sink → CanManager köprüsü. `can` CAN task'ine ait olduğundan
// (task-yerel), sink çağrıldığında geçerli örneğe bu file-scope pointer ile
// erişilir; CAN task başlamadan/örnek kurulmadan çağrılırsa güvenle atlanır.
// G2 iskeleti: MOTOR_DRIVER_PRESENT=0 iken sendTorqueCommand gerçek frame
// üretmez. TODO(motor entegrasyonu): bayrak 1 olduğunda torque'un CAN task'i
// DIŞINDAN (VCU task'inden) gönderilmesinin thread-safety'si ele alınmalı
// (bkz. Documents/MOTOR_ENTEGRASYON_NOTU.md).
static CanManager* s_canForTorque = nullptr;
static void CAN_torqueSink(uint16_t torque) {
  if (s_canForTorque != nullptr)
    s_canForTorque->sendTorqueCommand(torque);
}

// M2: VcuLogic → RelayManager köprüsü (kompozisyon kökü adapteri). VcuLogic
// artık somut RelayManager singleton'ına değil IRelayActuator arayüzüne bağlı;
// bu ince adapter gerçek RelayManager çağrılarını arayüze uydurur. Böylece
// RelayManager saf donanım sürücüsü kalır (VcuLogic'ten haberi olmaz) ve
// enjeksiyon burada, tek yerde yapılır.
namespace {
class RelayActuatorAdapter : public IRelayActuator {
 public:
  void closeAllContactors() override { RelayManager::instance().allOn(); }
  void openAllContactors(bool silent) override {
    RelayManager::instance().allOff(silent);
  }
  void verifyIfDue(uint32_t nowMs) override {
    RelayManager::instance().verifyIfDue(nowMs);
  }
  bool hasActuatorFault() const override {
    return RelayManager::instance().hasActuatorFault();
  }
  void clearActuatorFault() override {
    RelayManager::instance().clearActuatorFault();
  }
};
RelayActuatorAdapter g_relayActuator;
}  // namespace

// ---------------------------------------------------------------------------
// Helper: log stack high-water-mark periodically
// ---------------------------------------------------------------------------
static void logStackUsage(const char *taskName, uint32_t &lastLogTick) {
  uint32_t now = xTaskGetTickCount();
  if ((now - lastLogTick) >= pdMS_TO_TICKS(STACK_LOG_INTERVAL_MS)) {
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGD(taskName, "Stack high water mark: %u words remaining", hwm);
    lastLogTick = now;
  }
}

// ---------------------------------------------------------------------------
// CAN communication task
// ---------------------------------------------------------------------------
void vTask_CAN_Comm(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  CanManager can(CAN_TX_PIN, CAN_RX_PIN);
  can.setEventCallback(CAN_handleEvent, nullptr);
  s_canForTorque = &can;  // VcuLogic torque sink'inin köprüleyeceği örnek

  if (!can.begin()) {
    ESP_LOGE(TAG, "Failed to initialize CAN bus");
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
    return;
  }

  // G2 GERÇEĞİ: Motor sürücüsü entegre değil (MOTOR_DRIVER_PRESENT=0). Bu
  // fazda torque komutu GÖNDERİLMİYOR; kontaktörler yük altında açılıyor
  // OLABİLİR. Saha riski: ark/kontak kaynaması. Entegrasyonda
  // sendTorqueCommand(0) dizisi (VcuLogic E-STOP/FAULT güvenli kapanış sırası
  // üzerinden) aktive edilecek. Bkz. Documents/MOTOR_ENTEGRASYON_NOTU.md.
  // NOT: Torque komutu artık bu döngüde DEĞİL, VcuLogic handleEmergencyStop/
  // handleFault içinden sink hook ile çağrılıyor (doğru güvenlik sırası için).
  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    // 1. Read incoming messages and dispatch them
    can.processRxMessages();

    // 2. Push the latest telemetry snapshot to the shared queue
    if (TEL_sensorDataQueue != nullptr) {
      TelemetryData TEL_data = can.getTelemetryData();
      TEL_data.TEL_speedKmhX10  = rpmToSpeedKmhX10(TEL_data.TEL_motorRpm);
      TEL_data.TEL_timestampMs  = (uint32_t)(esp_timer_get_time() / 1000ULL);
      xQueueOverwrite(TEL_sensorDataQueue, &TEL_data);
    }

    logStackUsage("CAN_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz
  }
}

// ---------------------------------------------------------------------------
// VCU logic task — runs the main state machine
// ---------------------------------------------------------------------------
void vTask_VCU_Logic(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    if (TEL_sensorDataQueue != nullptr) {
      TelemetryData TEL_data = {};
      if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
        VcuLogic::setTelemetryData(TEL_data);
      }
    }

    VcuLogic::run();

    logStackUsage("VCU_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz loop
  }
}

// ---------------------------------------------------------------------------
// HMI task — updates display with current state and sensor data
// ---------------------------------------------------------------------------
void vTask_HMI_Display(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  uint32_t lastStackLog = 0;

  DisplayHMI HMI_display;
  bool hmi_ok = false;
  int retry_count = 0;
  while (!hmi_ok && retry_count < 5) {
      if (HMI_display.begin()) {
          hmi_ok = true;
      } else {
          if (retry_count == 0) {
              ESP_LOGW(TAG, "DisplayHMI baslatilamadi, tekrar deneniyor...");
          }
          retry_count++;
          vTaskDelay(pdMS_TO_TICKS(1000));
          esp_task_wdt_reset();
      }
  }

  if (!hmi_ok) {
      ESP_LOGE(TAG, "DisplayHMI 5 denemede baslatilamadi, HMI task uykuya geciyor (degraded mode)");
      while (true) {
          vTaskDelay(pdMS_TO_TICKS(1000));
          esp_task_wdt_reset();
      }
  }

  uint8_t HMI_incomingCommand = 0;

  while (true) {
    esp_task_wdt_reset();

    HMI_DisplayData HMI_screenData = {};
    // Kuyruk boşken de "veri yok" ("--") görünmeli — sahte %0/0°C değil.
    HMI_screenData.HMI_currentBattery = HMI_BATTERY_NO_DATA;
    HMI_screenData.HMI_bmsTemperatureC = HMI_TEMP_NO_DATA;

    if (TEL_sensorDataQueue != nullptr) {
        TelemetryData TEL_data = {};
        if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
            HMI_screenData.HMI_currentSpeed = TEL_data.TEL_speedKmhX10 / 10;
            // SOC/sıcaklık kaynak sinyalleri DOĞRULANMADI (hiç parse
            // edilmiyor, hep 0) — sürücüye sahte "%0 batarya / 0°C"
            // göstermemek için sentinel gönderilir. Kaynak sinyal
            // DOĞRULANDIĞINDA HMI_*_SOURCE_VERIFIED true yapılıp bu geçici
            // yol kaldırılacak (bkz. HMIHelpers.h "Veri yok gösterimi").
            HMI_screenData.HMI_currentBattery = HMI_batteryDisplayValue(
                HMI_SOC_SOURCE_VERIFIED, TEL_data.TEL_bmsDataValid,
                TEL_data.TEL_bmsSocHundredths);
            HMI_screenData.HMI_motorRpm = TEL_data.TEL_motorRpm;
         //   HMI_screenData.HMI_motorTorqueFeedback =
       //         TEL_data.TEL_motorTorqueFeedback;
            HMI_screenData.HMI_motorErrorFlags = TEL_data.TEL_motorErrorFlags;
            HMI_screenData.HMI_motorDataValid = TEL_data.TEL_motorDataValid;
            HMI_screenData.HMI_motorTimeoutActive =
                TEL_data.TEL_motorTimeoutActive;
            HMI_screenData.HMI_bmsTemperatureC = HMI_temperatureDisplayValue(
                HMI_TEMP_SOURCE_VERIFIED, TEL_data.TEL_bmsDataValid,
                TEL_data.TEL_bmsTempHighestC);
            HMI_screenData.HMI_bmsPackVoltageDeciV =
                TEL_data.TEL_bmsPackVoltageDeciV;
        }
    }

    HMI_screenData.HMI_vcuState = HMI_mapVcuState(VcuLogic::getState());
    HMI_screenData.HMI_contactorClosed = HMI_areAllContactorsClosed();

    HMI_display.updateScreen(HMI_screenData);

    // 24-hücre BMS verisini Nextion'a gönder
    if (TEL_sensorDataQueue != nullptr) {
        TelemetryData TEL_data = {};
        bool hasData = (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE);
        bool bmsValid = hasData && TEL_data.TEL_bmsDataValid;

        BmsPackData BMS_raw = {};
        BMS_raw.isValid = bmsValid;

        BmsComputed BMS_comp = {};

        if (bmsValid) {
            // TEL_bmsCurrentCentiA birimi centi-Amper (0.01 A) → BmsPackData
            // .packCurrentMa mA (0.001 A) bekler: 1 centi-A = 10 mA, yani ×10.
            // TODO(HMI): packCurrentMa şu an hiçbir yerde tüketilmiyor (Nextion'a
            // gitmiyor); ekranda gösterilecekse beklenen birim Nextion tarafından
            // teyit edilecek.
            BMS_raw.packCurrentMa = TEL_data.TEL_bmsCurrentCentiA * 10;
            
            // Lithium Balance c-BMS'ten 24 hücre verisi henüz ayrı ayrı çözülmedi, sadece packV doğrulandı.
            // Ekranda (Nextion) 24 hücre barının tümünün hata vermemesi ve sahte (rastgele) veri 
            // üretmemek adına tüm hücrelere ortalama gerilimi atıyoruz.
            uint16_t avgCellMv = (TEL_data.TEL_bmsPackVoltageDeciV * 100) / BMS_CELL_COUNT;
            for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                BMS_raw.cellVoltageMv[i] = avgCellMv;
                BMS_raw.cellTempC[i] = TEL_data.TEL_bmsTempHighestC;
            }

            // TODO(dogrulama): Lithium Balance E001-E033 tersine mühendisliği
            // (reverse engineering) henüz tamamlanmadı (açık iş).
            // Gerçek max/min hücre gerilimi doğrulanana kadar algoritma uyarı seviyesi
            // avgCellMv üzerinden (sağlıklı varsayılarak) çalışır.
            BMS_comp = computePack(BMS_raw);

            // Ekranda ise sahte 0 mV yerine sentinel ("--") gösterilir.
            if (!HMI_CELL_VOLTAGE_SOURCE_VERIFIED) {
                BMS_comp.cellMaxMv = HMI_CELL_VOLTAGE_NO_DATA;
                BMS_comp.cellMinMv = HMI_CELL_VOLTAGE_NO_DATA;
            } else {
                BMS_comp.cellMaxMv = TEL_data.TEL_bmsCellVoltageMaxDeciMv / 10;
                BMS_comp.cellMinMv = TEL_data.TEL_bmsCellVoltageMinDeciMv / 10;
            }
        } else {
            // Geçersiz/bayat veri durumu: Ekranda son değerlerin donup kalmaması
            // için "--" (sentinel) ve boş bar gönderilir. Uyarı durumu CRITICAL yapılır.
            for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                BMS_raw.cellVoltageMv[i] = HMI_CELL_VOLTAGE_NO_DATA;
                BMS_comp.balanceFlag[i] = false;
            }
            BMS_comp.cellMaxMv = HMI_CELL_VOLTAGE_NO_DATA;
            BMS_comp.cellMinMv = HMI_CELL_VOLTAGE_NO_DATA;
            BMS_comp.warningLevel = BMS_WARN_CRITICAL;
        }

        static BmsNextionCache BMS_hmiCache = {};
        static uint32_t BMS_lastCellUpdateMs = 0;
        static bool BMS_firstRun = true;
        
        uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
        bool updateCells = false;
        bool forceRefresh = false;

        if (BMS_firstRun) {
            forceRefresh = true;
            updateCells = true;
            BMS_firstRun = false;
            BMS_lastCellUpdateMs = nowMs;
        } else if (nowMs - BMS_lastCellUpdateMs >= 1000) {
            updateCells = true;
            BMS_lastCellUpdateMs = nowMs;
        }

        buildBmsNextionCommands(BMS_comp, BMS_raw, BMS_emitNextionCommand, nullptr,
                                BMS_hmiCache, forceRefresh, updateCells);
    }

    if (HMI_display.readTouchCommand(HMI_incomingCommand)) {
        switch (HMI_incomingCommand) {
            case HMI_CMD_START:
                ESP_LOGI(TAG, "HMI command: START request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::START_REQUEST);
                break;
            case HMI_CMD_RESET:
                ESP_LOGI(TAG, "HMI command: RESET request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::RESET);
                break;
            case HMI_CMD_EMERGENCY_STOP:
                ESP_LOGE(TAG, "HMI command: EMERGENCY STOP triggered");
                VcuLogic::postEvent(VcuLogic::VcuEvent::EMERGENCY_STOP);
                break;
            case HMI_CMD_DRIVE_ENABLE:
                ESP_LOGI(TAG, "HMI command: DRIVE ENABLE request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::DRIVE_ENABLE);
                break;
            default:
                ESP_LOGW(TAG, "Unknown HMI command received: %d", HMI_incomingCommand);
                break;
        }
    }

    logStackUsage("HMI_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz refresh
  }
}

// Bench teyidi için register bloğunu satır satır loglar. UKS ile birebir
// eşleşmesi zorunlu format: "E22REG,0x%02X,0x%02X\r\n" (adres, değer).
// NOT: E22REG satırları kasıtlı olarak çıplak printf ile basılır (ESP_LOGI
// DEĞİL) — UKS aynı satırı çıplak printf ile basıyor, bench'te iki çıktı
// satır satır diff'leniyor; ESP_LOGI'nin "I (ts) TAG:" öneki ve çift satır
// sonu diff'i bozar. Başlık ayırıcı satırı diff kapsamı dışında, ESP_LOGI
// kalır.
static void E22_logHexDump(const char *label, const uint8_t *buf, int len) {
  ESP_LOGI(TAG, "--- %s ---", label);
  if (len < (int)(3 + E22_REG_BLOCK_LEN)) {
    return;
  }
  for (uint8_t i = 0; i < E22_REG_BLOCK_LEN; i++) {
    printf("E22REG,0x%02X,0x%02X\r\n", E22_REG_BLOCK_START + i, buf[3 + i]);
  }
}

// ---------------------------------------------------------------------------
// M1 refactor: ILoraHal'in gerçek ESP-IDF implementasyonu. LoraLink'in donanım
// primitiflerini (GPIO/UART/zaman/watchdog) buraya bağlar; native tarafta bu
// dosya derlenmez (build_src_filter = -<*>). E22REG hex dump format string'i
// (contract-drift bekçisi) bilinçli olarak burada — main.cpp'de — kalır.
// ---------------------------------------------------------------------------
class EspLoraHal : public ILoraHal {
 public:
  // GPIO mode/aux pinleri + UART (retry) kurulumu — eski task preamble'ı.
  void begin() {
    gpio_config_t modePinsConfig = {};
    modePinsConfig.pin_bit_mask = (1ULL << LORA_M0_PIN) | (1ULL << LORA_M1_PIN);
    modePinsConfig.mode = GPIO_MODE_OUTPUT;
    modePinsConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    modePinsConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    modePinsConfig.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&modePinsConfig));

    gpio_config_t auxPinConfig = {};
    auxPinConfig.pin_bit_mask = (1ULL << LORA_AUX_PIN);
    auxPinConfig.mode = GPIO_MODE_INPUT;
    auxPinConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    auxPinConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    auxPinConfig.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&auxPinConfig));

    // UART init — config modunda da 9600 8N1 kullanıldığından önce başlatılıyor
    uart_config_t uartConfig = {
        .baud_rate = LORA_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    bool uart_ok = false;
    bool uart_error_logged = false;
    while (!uart_ok) {
      esp_err_t err1 = uart_param_config(LORA_UART_NUM, &uartConfig);
      esp_err_t err2 = uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN,
                                    UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
      esp_err_t err3 = uart_driver_install(LORA_UART_NUM, 256, 256, 0, nullptr, 0);
      if (err1 == ESP_OK && err2 == ESP_OK && err3 == ESP_OK) {
        uart_ok = true;
        if (uart_error_logged) {
          ESP_LOGI(TAG, "LoRa UART basariyla baslatildi");
        }
      } else {
        if (!uart_error_logged) {
          ESP_LOGE(TAG, "LoRa UART init failed (err1=%d, err2=%d, err3=%d), "
                        "tekrar deneniyor...", err1, err2, err3);
          uart_error_logged = true;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_task_wdt_reset();
      }
    }
  }

  void setModePins(int m0Level, int m1Level) override {
    gpio_set_level(LORA_M0_PIN, m0Level);
    gpio_set_level(LORA_M1_PIN, m1Level);
  }
  bool isAuxReady() override {
    return gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL;
  }
  int auxRawLevel() override { return gpio_get_level(LORA_AUX_PIN); }
  void uartFlush() override { uart_flush(LORA_UART_NUM); }
  int uartWrite(const uint8_t *data, size_t len) override {
    return uart_write_bytes(LORA_UART_NUM, (const char *)data, len);
  }
  int uartRead(uint8_t *buf, size_t len, uint32_t timeoutMs) override {
    return uart_read_bytes(LORA_UART_NUM, buf, len, pdMS_TO_TICKS(timeoutMs));
  }
  void delayMs(uint32_t ms) override { vTaskDelay(pdMS_TO_TICKS(ms)); }
  uint64_t nowMs() override { return (uint64_t)(esp_timer_get_time() / 1000LL); }
  void feedWatchdog() override { esp_task_wdt_reset(); }
  void hexDumpE22(const char *label, const uint8_t *buf, int len) override {
    E22_logHexDump(label, buf, len);
  }
};

// UplinkScheduler'ın gönderim callback'i: AUX kapısı + sanitize + gerçek TX.
// Eski task içindeki AUX-not-ready tek-sefer logu ve replay sonrası watchdog
// beslemesi burada korunur.
struct LoRa_TxCtx {
  Telemetry *tel;
  EspLoraHal *hal;
  bool *auxNotReadyLogged;
};
static bool LoRa_txSend(const TelemetryData &pkt, bool isReplay, void *ctxv) {
  LoRa_TxCtx *c = static_cast<LoRa_TxCtx *>(ctxv);
  if (!c->hal->isAuxReady()) {
    if (!*c->auxNotReadyLogged) {
      ESP_LOGW(TAG, "LoRa AUX not ready, telemetry TX skipped");
      *c->auxNotReadyLogged = true;
    }
    return false;  // paket buffer'da KALIR (replay) / atlanır (canlı)
  }
  c->tel->sendStatus(TelemetrySanitize::sanitizeForUplink(pkt));
  *c->auxNotReadyLogged = false;
  if (isReplay) {
    c->hal->feedWatchdog();  // eski: her başarılı replay sonrası esp_task_wdt_reset
  }
  return true;
}

// ---------------------------------------------------------------------------
// LoRa UART task — ince orkestrasyon: LoraLink (donanım) + UplinkScheduler (saf)
// ---------------------------------------------------------------------------
void vTask_LoRa_UKS(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  // Donanım kurulumu + E22 boot-config el sıkışması (LoraLink).
  Telemetry LO_telemetry;
  EspLoraHal LO_hal;
  LO_hal.begin();
  LoraLink(LO_hal).configureE22();
  LO_telemetry.begin();

  // Saf uplink beyni (link FSM + offline örnekleme + replay); sabitler
  // eskisiyle birebir, taşınan statikler (s_linkDown vb.) artık içinde.
  UplinkScheduler LO_sched(LINK_TIMEOUT_MS, BOOT_LINK_GRACE_MS,
                           OFFLINE_SAMPLE_PERIOD_MS, REPLAY_BURST_PER_TICK,
                           LORA_UNKNOWN_BYTE_WARN_INTERVAL_MS);
  s_uplink = &LO_sched;  // cross-task LoRa_IsLinkDown() köprüsü

  // Boot-grace (9.2.e / 9.4.b.vi, S3): boot'tan sonra hiç heartbeat gelmezse
  // link DOWN → buffer'a yazım hemen başlar (veri kaybolmaz).
  const uint64_t LO_bootMs = LO_hal.nowMs();

  bool LO_auxNotReadyLogged = false;
  LoRa_TxCtx LO_txCtx{&LO_telemetry, &LO_hal, &LO_auxNotReadyLogged};

  uint8_t LO_rxBuffer[4];
  uint32_t lastStackLog = 0;
  TickType_t LO_lastTelemetryTick = 0;

  while (true) {
    esp_task_wdt_reset();

    // 1. RX (9.2.a: uzaktan komut yok — heartbeat dışı her byte "bilinmeyen").
    int LO_rxLength = LO_hal.uartRead(LO_rxBuffer, sizeof(LO_rxBuffer),
                                      LORA_RX_TIMEOUT_MS);
    for (int LO_i = 0; LO_i < LO_rxLength; LO_i++) {
      if (LO_sched.onRxByte(LO_rxBuffer[LO_i], LO_hal.nowMs()) ==
          UplinkScheduler::RxResult::UNKNOWN_WARN) {
        ESP_LOGW(TAG,
                 "LoRa RX: bilinmeyen byte 0x%02X (toplam %lu) — RF gurultu "
                 "olabilir",
                 LO_rxBuffer[LO_i], (unsigned long)LO_sched.unknownByteCount());
      }
    }

    // 2. Link FSM — DOWN/UP geçiş logunu burada bas.
    const UplinkScheduler::LinkTransition LO_tr =
        LO_sched.updateLink(LO_hal.nowMs(), LO_bootMs);
    if (LO_tr.becameDown) {
      ESP_LOGW("LINK", "UKS heartbeat timeout — link DOWN");
    } else if (LO_tr.becameUp) {
      if (LO_tr.hadSamples) {
        ESP_LOGI("LINK",
                 "Heartbeat alindi — link UP: %d paket, ts araligi [%lu..%lu] "
                 "replay ediliyor",
                 LO_tr.bufferedCount, (unsigned long)LO_tr.firstTs,
                 (unsigned long)LO_tr.lastTs);
      } else {
        ESP_LOGI("LINK", "Heartbeat alindi — link UP: 0 paket replay edilecek");
      }
    }

    // 3. TX tik'i (LORA_TX_PERIOD_MS): link UP → replay+canlı; DOWN → örnekle.
    const TickType_t LO_nowTick = xTaskGetTickCount();
    if ((LO_nowTick - LO_lastTelemetryTick) >=
        pdMS_TO_TICKS(LORA_TX_PERIOD_MS)) {
      LO_lastTelemetryTick = LO_nowTick;
      if (TEL_sensorDataQueue != nullptr) {
        if (!LO_sched.isLinkDown()) {
          TelemetryData LO_live = {};
          const bool LO_haveLive =
              (xQueuePeek(TEL_sensorDataQueue, &LO_live, 0) == pdTRUE);
          LO_sched.onTxTickLinkUp(LO_haveLive, LO_live, &LoRa_txSend, &LO_txCtx);
        } else {
          TelemetryData LO_buffered = {};
          if (xQueuePeek(TEL_sensorDataQueue, &LO_buffered, 0) == pdTRUE) {
            LO_sched.offlineSample(LO_hal.nowMs(), LO_buffered);
          }
        }
      }
    }

    logStackUsage("LoRa_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------------
// Main application entry point
// ---------------------------------------------------------------------------
#ifndef E22_DIAGNOSTIC_MODE
extern "C" void app_main() {
#if !VEHICLE_PARAMS_CONFIRMED
  // 9.2.c.i / 9.4.b.iii / 9.2.f: WHEEL_DIAMETER_M / GEAR_RATIO /
  // MOTOR_RPM_IS_WHEEL_RPM henüz gerçek değerlerle teyit edilmedi —
  // hız ve enerji verisi bu haliyle YANLIŞ. Derleme #warning'i
  // VehicleParams.h'de; bu boot logu sahada/bench'te de görünür kalır.
  ESP_LOGE(TAG, "ARAC PARAMETRELERI TEYITSIZ — hiz/enerji verisi gecersiz "
                "(VehicleParams.h)");
#endif

  // AÇIK İŞ (2026-07-03 merge, Solion SK -> Lithium Balance c-BMS donanım
  // geçişi): CanParse::parseLbBmsE000 yalnızca packV alanını çözüyor;
  // tempH/tempL/sysState/current/soc alanları hiçbir CAN ID'den parse
  // EDİLMİYOR (TelemetryData value-init default'unda kalıyor).
  // TelemetrySanitize::sanitizeSystemState(0) bunu FAULT(4) yapar — yani
  // UKS ekranında BMS her zaman FAULT görünür, gerçek bir arıza olmasa
  // bile. Ayrıca BMS_WARN_MAX_TEMP_C / BMS_CRITICAL_MAX_TEMP_C eşikleri
  // (SystemConfig.h) hiç tetiklenmez (temp hep 0 okunur). Bkz.
  // TEKNIK_KONTROL_PROVASI.md "AÇIK İŞ" maddesi (9.2.c.ii).
  ESP_LOGW(TAG, "BMS: LB parse eksik — tempH/soc/sysState vb. placeholder, "
                "sysState=FAULT gorunur (bkz. CanParse.cpp Lithium Balance "
                "stub'lari)");

  // --- Hardware initialization (before any tasks) ---

  // 1. Initialize relay hardware (SPI + MCP23S17)
  if (!RelayManager::instance().begin()) {
    ESP_LOGE(TAG, "RelayManager init failed — HALTING");
    return;
  }

  // 2. Initialize VCU state machine (event queue, safety allOff, INIT→IDLE).
  //    Aktüatör katmanı RelayManager adapteri ile enjekte edilir (M2).
  VcuLogic::init(g_relayActuator);
  // E-STOP/FAULT güvenli kapanış sırasındaki sıfır-tork isteğini CanManager'a
  // yönlendir (G2 iskeleti; flag 0 iken gerçek frame üretmez).
  VcuLogic::setTorqueSink(CAN_torqueSink);

  // 3. Create sensor data queue for inter-task communication
  TEL_sensorDataQueue = xQueueCreate(1, sizeof(TelemetryData));
  if (TEL_sensorDataQueue == nullptr) {
    ESP_LOGE(TAG, "Failed to create TEL_sensorDataQueue");
    return;
  }

  ESP_LOGI(TAG, "All subsystems initialized — starting tasks");

  // --- FreeRTOS task creation ---
  xTaskCreatePinnedToCore(vTask_CAN_Comm, "CAN_Task", 4096, nullptr, 5, nullptr,
                          0);
  xTaskCreatePinnedToCore(vTask_HMI_Display, "HMI_Task", 4096, nullptr, 2,
                          nullptr, 0);
  xTaskCreatePinnedToCore(vTask_VCU_Logic, "VCU_Task", 4096, nullptr, 10,
                          nullptr, 1);
  xTaskCreatePinnedToCore(vTask_LoRa_UKS, "LoRa_Task", 3072, nullptr, 8,
                          nullptr, 0);
}
#endif  // E22_DIAGNOSTIC_MODE
