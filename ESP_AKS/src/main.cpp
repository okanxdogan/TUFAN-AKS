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
#include "OfflineBuffer.h"
#include "RelayManager.h"
#include "SystemConfig.h"
#include "Telemetry.h"
#include "VcuLogic.h"

// 24 hücreli BMS gösterge altyapısı (Gerçek veri ile)
#include "BmsModel.h"
#include "BmsAlgo.h"
#include "BmsNextionPacket.h"
#include "HMIHelpers.h"

static constexpr const char *TAG = "APP_MAIN";

// Stack high-water-mark logging interval (ticks)
static constexpr uint32_t STACK_LOG_INTERVAL_MS = 30000;

QueueHandle_t TEL_sensorDataQueue = nullptr;



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

  if (!can.begin()) {
    ESP_LOGE(TAG, "Failed to initialize CAN bus");
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
    return;
  }

  // Phase 1.2 placeholder:
  // keep propulsion torque at zero until the pedal / brake conversion model
  // is defined and validated with vehicle controls.
  //uint16_t CAN_torqueCmd = 0;
  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    // 1. Read incoming messages and dispatch them
    can.processRxMessages();

    /* // 2. Send torque command if in DRIVE state
    if (VcuLogic::getState() == VcuLogic::VcuState::DRIVE) {
      // TODO: Get actual torque value from control logic
      can.sendTorqueCommand(CAN_torqueCmd);
    } else {
      // Not in drive — send zero torque for safety
      can.sendTorqueCommand(0);
    }
    */

    // 3. Push the latest telemetry snapshot to the shared queue
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
  if (!HMI_display.begin()) {
    ESP_LOGE(TAG, "DisplayHMI init failed — HMI task terminating");
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
    return;
  }

  uint8_t HMI_incomingCommand = 0;

  while (true) {
    esp_task_wdt_reset();

    HMI_DisplayData HMI_screenData = {};

    if (TEL_sensorDataQueue != nullptr) {
        TelemetryData TEL_data = {};
        if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
            HMI_screenData.HMI_currentSpeed = TEL_data.TEL_motorRpm;
            HMI_screenData.HMI_currentBattery =
                static_cast<uint8_t>(TEL_data.TEL_bmsSocHundredths / 100);
            HMI_screenData.HMI_motorRpm = TEL_data.TEL_motorRpm;
         //   HMI_screenData.HMI_motorTorqueFeedback =
       //         TEL_data.TEL_motorTorqueFeedback;
            HMI_screenData.HMI_motorErrorFlags = TEL_data.TEL_motorErrorFlags;
            HMI_screenData.HMI_motorDataValid = TEL_data.TEL_motorDataValid;
            HMI_screenData.HMI_motorTimeoutActive =
                TEL_data.TEL_motorTimeoutActive;
            HMI_screenData.HMI_bmsTemperatureC =
                TEL_data.TEL_bmsTempHighestC;
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
        if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
            if (TEL_data.TEL_bmsDataValid) {
                BmsPackData BMS_raw = {};
                BMS_raw.isValid = true;
                // TEL_bmsCurrentCentiMa: raw * 0.01 = mA -> mA için /100.
                BMS_raw.packCurrentMa = TEL_data.TEL_bmsCurrentCentiMa / 100;
                
                // Gerçek Solion BMS 24 hücre verisini ayrı ayrı göndermez, sadece max/min gönderir.
                // Ekranda (Nextion) 24 hücre barının tümünün hata vermemesi ve sahte (rastgele) veri 
                // üretmemek adına tüm hücrelere ortalama gerilimi atıyoruz.
                uint16_t avgCellMv = (TEL_data.TEL_bmsPackVoltageDeciV * 100) / BMS_CELL_COUNT;
                for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                    BMS_raw.cellVoltageMv[i] = avgCellMv;
                    BMS_raw.cellTempC[i] = TEL_data.TEL_bmsTempHighestC;
                }

                // İlk iki hücreye gerçek max ve min değerini yazarak algoritmanın
                // uyarı seviyesini doğru hesaplamasını sağlıyoruz.
                BMS_raw.cellVoltageMv[0] = TEL_data.TEL_bmsCellVoltageMaxDeciMv / 10;
                BMS_raw.cellVoltageMv[1] = TEL_data.TEL_bmsCellVoltageMinDeciMv / 10;

                BmsComputed BMS_comp = computePack(BMS_raw);

                // Algoritma hesapladıktan sonra ekrana gönderilecek max/min değerlerini
                // her ihtimale karşı doğrudan gerçek CAN verisinden eziyoruz.
                BMS_comp.cellMaxMv = TEL_data.TEL_bmsCellVoltageMaxDeciMv / 10;
                BMS_comp.cellMinMv = TEL_data.TEL_bmsCellVoltageMinDeciMv / 10;

                buildBmsNextionCommands(BMS_comp, BMS_raw, BMS_emitNextionCommand, nullptr);
            }
        }
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
// LoRa UART task — reads UKS commands and posts events to VcuLogic
// ---------------------------------------------------------------------------
void vTask_LoRa_UKS(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  Telemetry LO_telemetry;
  int LO_auxLevel = 0;

  gpio_config_t LO_modePinsConfig = {};
  LO_modePinsConfig.pin_bit_mask =
      (1ULL << LORA_M0_PIN) | (1ULL << LORA_M1_PIN);
  LO_modePinsConfig.mode = GPIO_MODE_OUTPUT;
  LO_modePinsConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  LO_modePinsConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  LO_modePinsConfig.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&LO_modePinsConfig));

  gpio_config_t LO_auxPinConfig = {};
  LO_auxPinConfig.pin_bit_mask = (1ULL << LORA_AUX_PIN);
  LO_auxPinConfig.mode = GPIO_MODE_INPUT;
  LO_auxPinConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  LO_auxPinConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  LO_auxPinConfig.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&LO_auxPinConfig));

  // UART init — config modunda da 9600 8N1 kullanıldığından önce başlatılıyor
  uart_config_t LO_uartConfig = {
      .baud_rate = LORA_UART_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };
  uart_param_config(LORA_UART_NUM, &LO_uartConfig);
  uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
  uart_driver_install(LORA_UART_NUM, 256, 256, 0, nullptr, 0);

  // --- E22 boot konfigürasyonu: oku + gerekirse yaz + doğrula ---
  gpio_set_level(LORA_M0_PIN, LORA_MODE_CONFIG_M0_LEVEL);
  gpio_set_level(LORA_M1_PIN, LORA_MODE_CONFIG_M1_LEVEL);
  ESP_LOGI(TAG, "E22: config moduna giriliyor (M0=%d M1=%d)",
           LORA_MODE_CONFIG_M0_LEVEL, LORA_MODE_CONFIG_M1_LEVEL);

  {
    // Config modunda AUX HIGH bekle
    const TickType_t LO_modeT0 = xTaskGetTickCount();
    bool LO_auxCfgReady = false;
    while ((xTaskGetTickCount() - LO_modeT0) <
           pdMS_TO_TICKS(LORA_AUX_MODE_TIMEOUT_MS)) {
      if (gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL) {
        LO_auxCfgReady = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!LO_auxCfgReady) {
      ESP_LOGE(TAG, "E22 AUX timeout (%d ms): config moduna girilemedi, "
                    "normal moda devam ediliyor",
               LORA_AUX_MODE_TIMEOUT_MS);
    } else {
      // 1. Mevcut bloğu oku (0xC1 <start> <len>) — bench teyidi için hex dump
      uart_flush(LORA_UART_NUM);
      uint8_t LO_readCmd[3];
      const size_t LO_readCmdLen =
          e22_buildReadAllCommand(LO_readCmd, sizeof(LO_readCmd));
      uart_write_bytes(LORA_UART_NUM, (const char *)LO_readCmd, LO_readCmdLen);

      uint8_t LO_readResp[3 + E22_REG_BLOCK_LEN] = {};
      const int LO_readLen =
          uart_read_bytes(LORA_UART_NUM, LO_readResp, sizeof(LO_readResp),
                           pdMS_TO_TICKS(LORA_CFG_READ_TIMEOUT_MS));
      E22_logHexDump("E22 mevcut config yaniti", LO_readResp,
                      LO_readLen > 0 ? LO_readLen : 0);

      bool LO_needsWrite = true;
      if (LO_readLen > 0 &&
          e22_isErrorResponse(LO_readResp, (size_t)LO_readLen)) {
        ESP_LOGE(TAG, "E22 okuma reddedildi (FF FF FF) — yazma denenecek");
      } else {
        E22RegValues LO_current = {};
        if (e22_parseRegResponse(LO_readResp, (size_t)LO_readLen,
                                  LO_current)) {
          if (e22_regsEqual(LO_current, E22_CONTRACT_REGS)) {
            ESP_LOGI(TAG, "E22 config zaten sozlesmeyle uyumlu, yazma "
                          "atlaniyor");
            LO_needsWrite = false;
          } else {
            ESP_LOGW(TAG, "E22 mevcut config sozlesmeden farkli — yaziliyor");
          }
        } else {
          ESP_LOGE(TAG, "E22 okuma yaniti eksik/hatali (%d byte) — yazma "
                        "denenecek",
                   LO_readLen);
        }
      }

      // 2. Fark varsa (ya da okunamadıysa) kalıcı yazma komutu gönder
      if (LO_needsWrite) {
        uart_flush(LORA_UART_NUM);
        uint8_t LO_writeCmd[3 + E22_REG_BLOCK_LEN];
        const size_t LO_writeCmdLen = e22_buildWriteCommand(
            E22_CONTRACT_REGS, LO_writeCmd, sizeof(LO_writeCmd));
        uart_write_bytes(LORA_UART_NUM, (const char *)LO_writeCmd,
                          LO_writeCmdLen);

        // Flash yazımının bitmesini bekle: AUX LOW→HIGH geçişi (~200ms tipik)
        vTaskDelay(pdMS_TO_TICKS(50));  // modülün LOW'a geçmesine zaman ver
        const TickType_t LO_flashT0 = xTaskGetTickCount();
        bool LO_flashDone = false;
        while ((xTaskGetTickCount() - LO_flashT0) <
               pdMS_TO_TICKS(LORA_AUX_CFG_TIMEOUT_MS)) {
          if (gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL) {
            LO_flashDone = true;
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (!LO_flashDone) {
          ESP_LOGE(TAG, "E22 flash AUX timeout (%d ms): config yazildi ancak "
                        "dogrulanamadi, devam ediliyor",
                   LORA_AUX_CFG_TIMEOUT_MS);
        } else {
          // 3. Yazma sonrası onay: E22, C0 değil C1 formatıyla yanıt verir
          uint8_t LO_confirm[3 + E22_REG_BLOCK_LEN] = {};
          const int LO_confirmLen = uart_read_bytes(
              LORA_UART_NUM, LO_confirm, sizeof(LO_confirm),
              pdMS_TO_TICKS(LORA_CFG_READ_TIMEOUT_MS));
          E22_logHexDump("E22 yazma onay yaniti", LO_confirm,
                          LO_confirmLen > 0 ? LO_confirmLen : 0);

          if (LO_confirmLen > 0 &&
              e22_isErrorResponse(LO_confirm, (size_t)LO_confirmLen)) {
            ESP_LOGE(TAG, "E22 config yazma reddedildi (FF FF FF) — modul "
                          "eski config ile devam ediyor olabilir, gorevde "
                          "kaliniyor");
          } else {
            E22RegValues LO_written = {};
            if (e22_parseRegResponse(LO_confirm, (size_t)LO_confirmLen,
                                      LO_written) &&
                e22_regsEqual(LO_written, E22_CONTRACT_REGS)) {
              ESP_LOGI(TAG,
                       "E22 config dogrulandi: NETID=0x%02X REG0=0x%02X "
                       "REG1=0x%02X REG2=0x%02X REG3=0x%02X",
                       LO_written.netid, LO_written.reg0, LO_written.reg1,
                       LO_written.reg2, LO_written.reg3);
            } else {
              ESP_LOGE(TAG, "E22 config yazma onayi uyusmuyor (%d byte) — "
                            "gorevde kaliniyor",
                       LO_confirmLen);
            }
          }
        }
      }
    }
  }

  // Normal transparan moda dön; AUX HIGH gelene kadar TX/RX başlatma
  gpio_set_level(LORA_M0_PIN, LORA_MODE_NORMAL_M0_LEVEL);
  gpio_set_level(LORA_M1_PIN, LORA_MODE_NORMAL_M1_LEVEL);
  {
    const TickType_t LO_normT0 = xTaskGetTickCount();
    while ((xTaskGetTickCount() - LO_normT0) <
           pdMS_TO_TICKS(LORA_AUX_MODE_TIMEOUT_MS)) {
      if (gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL) break;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  LO_auxLevel = gpio_get_level(LORA_AUX_PIN);
  ESP_LOGI(TAG, "E22: normal mod (M0=%d M1=%d AUX=%d)",
           LORA_MODE_NORMAL_M0_LEVEL, LORA_MODE_NORMAL_M1_LEVEL, LO_auxLevel);

  LO_telemetry.begin();

  // TEKNOFEST rule: communication is strictly one-way (vehicle → UKS).
  // UKS sends nothing to AKS — no E-Stop, no heartbeat, no commands.
  // LoRa RX is not processed. TX uses AUX pin to decide send vs buffer.

  uint32_t lastStackLog = 0;
  TickType_t LO_lastTelemetryTick = 0;
  bool LO_auxNotReadyLogged = false;

  while (true) {
    esp_task_wdt_reset();

    const TickType_t LO_nowTick = xTaskGetTickCount();
    if ((LO_nowTick - LO_lastTelemetryTick) >=
        pdMS_TO_TICKS(LORA_TX_PERIOD_MS)) {
      LO_lastTelemetryTick = LO_nowTick;

      if (TEL_sensorDataQueue != nullptr) {
        TelemetryData LO_live = {};
        if (xQueuePeek(TEL_sensorDataQueue, &LO_live, 0) == pdTRUE) {
          if (gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL) {
            // AUX ready — first drain any buffered packets, then send live
            TelemetryData LO_replay = {};
            while (ob_pop(LO_replay)) {
              LO_telemetry.sendStatus(LO_replay);
              esp_task_wdt_reset();
              vTaskDelay(pdMS_TO_TICKS(50));
            }
            LO_telemetry.sendStatus(LO_live);
            LO_auxNotReadyLogged = false;
          } else {
            // AUX busy — buffer the packet for later
            ob_push(LO_live);
            if (!LO_auxNotReadyLogged) {
              ESP_LOGW(TAG, "LoRa AUX not ready, buffering telemetry");
              LO_auxNotReadyLogged = true;
            }
          }
        }
      }
    }

    logStackUsage("LoRa_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(LORA_TX_PERIOD_MS));
  }
}

// ---------------------------------------------------------------------------
// Main application entry point
// ---------------------------------------------------------------------------
#ifndef E22_DIAGNOSTIC_MODE
extern "C" void app_main() {
  // --- Hardware initialization (before any tasks) ---

  // 1. Initialize relay hardware (SPI + MCP23S17)
  if (!RelayManager::instance().begin()) {
    ESP_LOGE(TAG, "RelayManager init failed — HALTING");
    return;
  }

  // 2. Initialize VCU state machine (event queue, safety allOff, INIT→IDLE)
  VcuLogic::init();

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
