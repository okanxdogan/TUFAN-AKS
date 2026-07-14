#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <atomic>
#include <cstdio>
#include <cmath>

#include "DisplayHMI.h"
#include "CanManager.h"
#include "E22Config.h"
#include "LinkMonitor.h"
#include "LoraLink.h"
#include "LoraRxHandler.h"
#include "OfflineBuffer.h"
#include "UartInitRetry.h"
#include "UplinkScheduler.h"
#include "RelayManager.h"
#include "SystemConfig.h"
#include "SysStateDerive.h"
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

// G11: UART init N kez başarısız olunca set edilir; LoRa task telemetrisiz moda
// geçer (araç durmaz). G11-b: KALICI DEĞİLDİR — vTask_LoRa_UKS
// LORA_INIT_RETRY_INTERVAL_MS'te bir begin()'i yeniden dener; retry başarılı
// olursa bu bayrak TEKRAR false'a döner (bkz. vTask_LoRa_UKS retry döngüsü).
// Cross-task okunur (LoRa_IsLinkDown deseni) → atomic. NOT: şu an bu bayrağı
// OKUYAN bir HMI/VcuLogic çağrı noktası YOK (grep ile doğrulandı, 2026-07-13)
// — API gelecekte telemetri kaybını arayüzde göstermek isteyen bir tüketici
// için hazır tutuluyor (bkz. CanManager::getChargerCommand'daki "bilinçli
// çağıransız API" deseniyle aynı gerekçe); "ölü kod" diye SİLİNMEMELİ.
static std::atomic<bool> s_loraTelemetryDisabled{false};

extern "C" bool LoRa_IsTelemetryDisabled(void) {
    return s_loraTelemetryDisabled.load(std::memory_order_relaxed);
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
  void allOn() override { RelayManager::instance().allOn(); }
  void allOff(bool silent) override { RelayManager::instance().allOff(silent); }
  void setRelay(uint8_t channel, bool state) override {
    RelayManager::instance().setRelay(channel, state);
  }
  void verifyIfDue(uint32_t nowMs) override {
    RelayManager::instance().verifyIfDue(nowMs);
  }
  bool verifyOutputs() override {
    return RelayManager::instance().verifyOutputs();
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
    // A) ESP_LOGD default'ta HİÇ basılmıyordu → stack koruması kördü. INFO'ya
    // çekildi; 30 sn periyot spam yapmaz. Değer WORD cinsindendir (ESP32'de
    // 1 word = 4 B) — "marj < 512 B" ~= "< 128 word" demektir.
    ESP_LOGI(taskName, "Stack high water mark: %u words remaining (%u B)", hwm,
             (unsigned)(hwm * sizeof(StackType_t)));
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

  // EMA (Üstel Hareketli Ortalama) filtresi değişkenleri (İbrenin akıcı hareket etmesi için)
  float HMI_smoothedRpm = 0.0f;
  float HMI_smoothedSpeed = 0.0f;
  const float HMI_EMA_ALPHA = 0.15f; // Bu değer (0.0 ile 1.0 arası) küçüldükçe ibre daha yavaş ve yumuşak kalkar.

  while (true) {
    esp_task_wdt_reset();

    HMI_DisplayData HMI_screenData = {};
    // Kuyruk boşken de "veri yok" ("--") görünmeli — sahte %0/0°C değil.
    HMI_screenData.HMI_currentBattery = HMI_BATTERY_NO_DATA;
    HMI_screenData.HMI_bmsTemperatureC = HMI_TEMP_NO_DATA;

    if (TEL_sensorDataQueue != nullptr) {
        TelemetryData TEL_data = {};
        if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
            // TEL_motorRpm işaretsiz büyüklük (uint16_t) — geri yön dönüşü
            // zaten CanManager'da mutlak değere çevrildi, doğrudan kullanılır.
            float targetRpm = (float)TEL_data.TEL_motorRpm;
            // Gerçek hızı km/h cinsinden hesaplıyoruz (Teknofest Şartnamesi)
            float targetSpeed = (float)TEL_data.TEL_speedKmhX10 / 10.0f;
            
            // İbreler için EMA hesaplaması: (Yeni Değer * Alpha) + (Eski Değer * (1 - Alpha))
            HMI_smoothedRpm = (HMI_EMA_ALPHA * targetRpm) + ((1.0f - HMI_EMA_ALPHA) * HMI_smoothedRpm);
            HMI_smoothedSpeed = (HMI_EMA_ALPHA * targetSpeed) + ((1.0f - HMI_EMA_ALPHA) * HMI_smoothedSpeed);

            HMI_screenData.HMI_currentSpeed = static_cast<uint16_t>(HMI_smoothedSpeed);
            // SOC ve sıcaklık kaynak sinyalleri DOĞRULANDI:
            //   SoC → 0xE000 byte[4:5], Temp → 0xE001 byte[6:7].
            // HMI_SOC_SOURCE_VERIFIED=true ve HMI_TEMP_SOURCE_VERIFIED=true.
            // TEL_bmsDataValid=false ise sentinel gönderilir ("--").
            HMI_screenData.HMI_currentBattery = HMI_batteryDisplayValue(
                HMI_SOC_SOURCE_VERIFIED, TEL_data.TEL_bmsDataValid,
                TEL_data.TEL_bmsSocHundredths);
            HMI_screenData.HMI_motorRpm = static_cast<uint16_t>(HMI_smoothedRpm);
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
            HMI_screenData.HMI_bmsPackCurrentCentiA =
                TEL_data.TEL_bmsCurrentCentiA;
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

            // G8/M4 FIX: Hücre gerilimi kaynağı DOĞRULANDI
            // (HMI_CELL_VOLTAGE_SOURCE_VERIFIED=true, bkz. HMIHelpers.h) —
            // E015-E020, gerçek 24 hücre verisi. Eski kod (kaynak DOĞRULANMADAN
            // önce) pack/24 ortalamasını 24 hücrenin HEPSİNE yazıp en yüksek
            // sıcaklığı tüm hücrelere kopyalıyordu; bu FABRİKASYON gerçek bir
            // hücre dengesizliğini (tek hücre 2.3 V'a düşse bile) maskeleyip
            // ekranı SAĞLIKLI gösteriyordu. Fabrikasyon KALDIRILDI, yerine
            // gerçek veri yazılıyor (aşağıda). `cellDataValid` (=
            // `TEL_cellVoltageDataValid`) yalnız bu anlık görüntüde 24
            // hücrenin TAMAMI henüz taze/tam DEĞİLSE (boot sonrası tüm CAN
            // ID'leri henüz gelmedi / freshness timeout) false olur; o
            // durumda per-hücre alanlara sentinel yazılır (aşağıdaki else
            // dalı) → ekranda hücreler "--", barlar boş, dengeleme yok, ve
            // computePack dengeleme/uyarıyı "veri yok" (NO_DATA) döndürür.
            BMS_raw.cellDataValid = TEL_data.TEL_cellVoltageDataValid;
            if (BMS_raw.cellDataValid) {
                for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                    BMS_raw.cellVoltageMv[i] = TEL_data.TEL_bmsCellVoltages[i];
                    BMS_raw.cellTempC[i] = 0;
                }
            } else {
                for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                    BMS_raw.cellVoltageMv[i] = HMI_CELL_VOLTAGE_NO_DATA;
                    BMS_raw.cellTempC[i] = 0;
                }
            }

            BMS_comp = computePack(BMS_raw);
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
  // G11: UART bounded-retry. Kurulursa true; N denemede kurulamazsa false
  // döner (çağıran telemetrisiz moda geçer, reboot YOK).
  //
  // RETRY-SAFE (G11-b): bu fonksiyon TEKRAR ÇAĞRILABİLİR — vTask_LoRa_UKS
  // ilk çağrı false dönerse LORA_INIT_RETRY_INTERVAL_MS'te bir begin()'i
  // YENİDEN çağırır (bkz. çağıran taraf, Documents/LoRa_Link_Analysis.md
  // "G11-b"). Gerekçe: `uart_driver_install`, sürücü zaten kuruluyken
  // (ESP_ERR_INVALID_STATE ile) çağrılırsa SONSUZA dek başarısız kalır —
  // aşağıdaki döngünün HER iterasyonu (ilk çağrının kendi iç denemeleri
  // KADAR, bir SONRAKİ dış çağrının İLK iterasyonu DAHİL) `uart_is_driver_
  // installed()` ile önce kontrol edip gerekirse `uart_driver_delete()` ile
  // temizler; bu kontrol döngünün EN BAŞINDA olduğu için hangi err1/err2/
  // err3 kombinasyonuyla önceki çağrı yarım kaldıysa kalsın, yeni bir
  // begin() çağrısı da SIFIRDAN temiz kurulum dener. BU KONTROLÜ (satırdaki
  // `if (uart_is_driver_installed(...))` bloğunu) KALDIRMAYIN — kaldırılırsa
  // G11-b retry döngüsü ikinci denemeden itibaren kalıcı olarak
  // ESP_ERR_INVALID_STATE'e takılır. (GPIO `ESP_ERROR_CHECK` çağrıları bu
  // döngünün DIŞINDA, her begin() çağrısında bir kez çalışır; aynı pin
  // bitmask'iyle tekrar `gpio_config` çağırmak ESP-IDF'te normal/idempotent
  // bir işlemdir, retry'e özgü bir tuzak değildir.)
  bool begin() {
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
    bool uart_error_logged = false;
    int uart_failedAttempts = 0;
    while (true) {
      // G11: Retry TUZAĞI düzeltmesi — driver zaten kuruluysa (önceki yarım
      // kurulum) yeniden install ESP_ERR_INVALID_STATE ile SONSUZA dek
      // başarısız kalırdı. Önce sil, sonra temiz kur.
      if (uart_is_driver_installed(LORA_UART_NUM)) {
        uart_driver_delete(LORA_UART_NUM);
      }
      esp_err_t err1 = uart_param_config(LORA_UART_NUM, &uartConfig);
      esp_err_t err2 = uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN,
                                    UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
      esp_err_t err3 = uart_driver_install(LORA_UART_NUM, 256, 256, 0, nullptr, 0);
      if (err1 == ESP_OK && err2 == ESP_OK && err3 == ESP_OK) {
        if (uart_error_logged) {
          ESP_LOGI(TAG, "LoRa UART basariyla baslatildi (%d. denemede)",
                   uart_failedAttempts + 1);
        }
        return true;
      }

      ++uart_failedAttempts;
      if (!uart_error_logged) {
        ESP_LOGE(TAG, "LoRa UART init failed (err1=%d, err2=%d, err3=%d), "
                      "tekrar deneniyor...", err1, err2, err3);
        uart_error_logged = true;
      }
      // G11: N denemeden sonra SONSUZ döngü YERİNE vazgeç → telemetrisiz mod.
      if (uart_init_retry_decision(uart_failedAttempts,
                                   LORA_UART_MAX_INIT_ATTEMPTS) ==
          UartInitDecision::GIVE_UP_DISABLED) {
        ESP_LOGE(TAG,
                 "LoRa UART %d denemede kurulamadi — TELEMETRI DEVRE DISI "
                 "(arac calismaya devam eder; reboot yok)",
                 uart_failedAttempts);
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      esp_task_wdt_reset();
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
// G10: TX ring buffer dolunca uart_write_bytes'ın BLOKLAMASIYLA ertelenen
// gönderim sayacı (replay drenajını yavaşlatıp RX/heartbeat'i geciktiren blok
// yerine tick'i erteliyoruz).
static uint32_t s_loraTxDeferredCount = 0;
static bool LoRa_txSend(const TelemetryData &pkt, bool isReplay, void *ctxv) {
  LoRa_TxCtx *c = static_cast<LoRa_TxCtx *>(ctxv);
  if (!c->hal->isAuxReady()) {
    if (!*c->auxNotReadyLogged) {
      ESP_LOGW(TAG, "LoRa AUX not ready, telemetry TX skipped");
      *c->auxNotReadyLogged = true;
    }
    return false;  // paket buffer'da KALIR (replay) / atlanır (canlı)
  }
  // G10: TX ring'de bir tam frame'lik boş alan yoksa BLOKLAMA — bu tick'in
  // gönderimini ertele (replay buffer'da kalır / canlı atlanır), say ve bir
  // sonraki tikte tekrar dene. Böylece heartbeat okuyan RX yolu gecikmez.
  size_t LO_txFree = 0;
  if (uart_get_tx_buffer_free_size(LORA_UART_NUM, &LO_txFree) != ESP_OK ||
      LO_txFree < (size_t)LORA_TEL_FRAME_MAX_BYTES) {
    if ((s_loraTxDeferredCount++ % 50u) == 0u) {
      ESP_LOGW(TAG, "LoRa TX ring dolu — gonderim ertelendi (toplam %lu)",
               (unsigned long)s_loraTxDeferredCount);
    }
    return false;
  }
  // HİPOTEZ (bkz. SysStateDerive.h, Documents/CAN_Message_Table.md "0x0000E003"):
  // sanitize'DAN ÖNCE — sanitizeSystemState(0) çalışırsa 0'ı zaten FAULT(4)
  // yapar, türetilmiş 1/2/3 değeri o noktadan sonra uygulanırsa etkisiz kalır.
  // Yalnızca LoRa TX paketleme kopyası (pktForUplink) değişir; VcuLogic'in
  // okuduğu paylaşılan TelemetryData (pkt / TEL_sensorDataQueue) DOKUNULMADAN
  // kalır (EK B güven kuralı — bu türetilmiş değer VCU karar mantığına
  // BAĞLANMAZ).
  TelemetryData pktForUplink = pkt;
  SysStateDerive::applyIfEnabled(pktForUplink);
  c->tel->sendStatus(TelemetrySanitize::sanitizeForUplink(pktForUplink));
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
  // G11: UART kurulamazsa (N deneme, EspLoraHal::begin() içi) telemetrisiz
  // moda geç — aracı DURDURMA (FAULT yok), esp_restart YOK. Task canlı kalır
  // (wdt beslenir) ama RX/TX yapmaz; durum LoRa_IsTelemetryDisabled() ile
  // sorgulanabilir hale getirilir (şu an okuyan bir HMI/VcuLogic çağrı
  // noktası YOK — bkz. s_loraTelemetryDisabled tanım yorumu).
  //
  // G11-b: bu mod ARTIK KALICI DEĞİL (ne de "bir kez true olup kalan" bir
  // durum) — retry başarılı olduğunda aşağıda s_loraTelemetryDisabled tekrar
  // false'a çekilir. Geçici bir UART/donanım aksaklığı
  // (ör. gevşek kablo, geçici gürültü) sahada reboot beklemeden kendi kendine
  // düzelebilsin diye LORA_INIT_RETRY_INTERVAL_MS (30 sn) sabit aralıkla
  // begin() yeniden denenir (lora_task_retry_due, bkz. UartInitRetry.h).
  // Watchdog bekleme sırasında da beslenir. Araç bu süre boyunca zaten
  // etkilenmiyordu (telemetri kaybı FAULT tetiklemez) — bu değişiklik yalnız
  // telemetrinin KENDİ KENDİNE toparlanmasını ekliyor.
  uint64_t LO_lastInitAttemptMs = LO_hal.nowMs();
  bool LO_wasDisabled = false;
  while (!LO_hal.begin()) {
    s_loraTelemetryDisabled.store(true, std::memory_order_release);
    if (!LO_wasDisabled) {
      ESP_LOGE(TAG,
               "LoRa telemetri devre disi — %lu sn'de bir yeniden denenecek "
               "(RX/TX yok, arac etkilenmez)",
               (unsigned long)(LORA_INIT_RETRY_INTERVAL_MS / 1000U));
      LO_wasDisabled = true;
    }
    LO_lastInitAttemptMs = LO_hal.nowMs();
    while (!lora_task_retry_due(LO_hal.nowMs(), LO_lastInitAttemptMs,
                                LORA_INIT_RETRY_INTERVAL_MS)) {
      esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
  if (LO_wasDisabled) {
    ESP_LOGI(TAG, "LoRa telemetri KURTARILDI — retry basarili, normal "
                  "akisa donuluyor");
    s_loraTelemetryDisabled.store(false, std::memory_order_release);
  }

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
  // A) Log görünürlüğü: ESP-IDF'in ÇALIŞMA-ZAMANI log seviyesini INFO'ya ayarla
  // (doğru IDF mekanizması — Arduino'ya özgü CORE_DEBUG_LEVEL kaldırıldı). IDF
  // varsayılan derleme seviyesi de INFO'dur; bu çağrı niyeti açık kılar ve HWM
  // (ESP_LOGI) dahil INFO ve üstü logların basıldığını garanti eder.
  esp_log_level_set("*", ESP_LOG_INFO);

#if !VEHICLE_PARAMS_CONFIRMED
  // 9.2.c.i / 9.4.b.iii / 9.2.f: WHEEL_DIAMETER_M / GEAR_RATIO /
  // MOTOR_RPM_IS_WHEEL_RPM henüz gerçek değerlerle teyit edilmedi —
  // hız ve enerji verisi bu haliyle YANLIŞ. Derleme #warning'i
  // VehicleParams.h'de; bu boot logu sahada/bench'te de görünür kalır.
  ESP_LOGE(TAG, "ARAC PARAMETRELERI TEYITSIZ — hiz/enerji verisi gecersiz "
                "(VehicleParams.h)");
#endif

  // BMS durumu: 0xE000 ve 0xE001 DOĞRULANDI (packV, current, SoC, temp,
  // hücre min/max/avg). 24 hücrenin tekil voltajları (E015-E020) da
  // DOĞRULANDI. Açık iş: TEL_bmsSystemState hiçbir CAN ID'den parse
  // EDİLMİYOR — TelemetrySanitize::sanitizeSystemState(0) bunu FAULT(4)
  // yapar → UKS ekranında BMS her zaman FAULT görünür. Hücre sıcaklığı
  // (E032-E033) alan anlamı hâlâ BİLİNMİYOR (stub).
  // Bkz. Documents/CAN_Message_Table.md.
  ESP_LOGW(TAG, "BMS: sysState henuz parse edilmiyor (E002-E006 stub). "
                "Hucre voltajlari (E015-E020) DOGRULANDI ve aktif.");

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
  // M6: Öncelikler SystemConfig.h'de TASK_PRIORITY_* olarak. CAN (güvenlik-
  // kritik alım) > LoRa (telemetri): telemetri, güvenlik-kritik CAN alımını
  // preempt edemez. Eskiden CAN=5 < LoRa=8 idi; düzeltildi (CAN=8, LoRa=5).
  xTaskCreatePinnedToCore(vTask_CAN_Comm, "CAN_Task", 4096, nullptr,
                          TASK_PRIORITY_CAN, nullptr, 0);
  xTaskCreatePinnedToCore(vTask_HMI_Display, "HMI_Task", 4096, nullptr,
                          TASK_PRIORITY_HMI, nullptr, 0);
  xTaskCreatePinnedToCore(vTask_VCU_Logic, "VCU_Task", 4096, nullptr,
                          TASK_PRIORITY_VCU, nullptr, 1);
  // LoRa stack 3072 B: HWM ölçümü artık ESP_LOGI ile GÖRÜNÜR (logStackUsage).
  // Marj < 512 B (≈128 word "remaining") ise stack'i artır; ölçüm görünmeden
  // körlemesine büyütme.
  xTaskCreatePinnedToCore(vTask_LoRa_UKS, "LoRa_Task", 3072, nullptr,
                          TASK_PRIORITY_LORA, nullptr, 0);
}
#endif  // E22_DIAGNOSTIC_MODE
