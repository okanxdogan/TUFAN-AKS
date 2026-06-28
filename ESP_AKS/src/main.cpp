#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "DisplayHMI.h"
#include "CanManager.h"
#include "RelayManager.h"
#include "SystemConfig.h"
#include "Telemetry.h"
#include "VcuLogic.h"

// --- 24 hücreli BMS gösterge zinciri ---
// Veri akışı: [Yalancı Veri (Sim)] -> ICellDataSource (HAL) -> computePack()
// -> Nextion komutları -> HMI UART. Gerçek BMS gelince SimCellDataSource yerine
// RealCellDataSource kullanılır; arayüz aynı kaldığından tüketici kod değişmez.
#include "BmsModel.h"
#include "SimCellDataSource.h"
#include "BmsAlgo.h"
#include "BmsNextionPacket.h"
#include "HMIHelpers.h"  // HMI_sendEndBytes

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
// buildBmsNextionCommands her tam komut için bunu çağırır; biz komut gövdesini
// yazıp ardından Nextion end-byte'larını (0xFF 0xFF 0xFF) ekleriz.
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
  uint16_t CAN_torqueCmd = 0;
  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    // 1. Read incoming messages and dispatch them
    can.processRxMessages();

    // 2. Send torque command if in DRIVE state
    if (VcuLogic::getState() == VcuLogic::VcuState::DRIVE) {
      // TODO: Get actual torque value from control logic
      can.sendTorqueCommand(CAN_torqueCmd);
    } else {
      // Not in drive — send zero torque for safety
      can.sendTorqueCommand(0);
    }

    // 3. Push the latest telemetry snapshot to the shared queue
    if (TEL_sensorDataQueue != nullptr) {
      const TelemetryData TEL_data = can.getTelemetryData();
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

  // 24-hücre yalancı veri kaynağı (HAL). Gerçek BMS gelince yalnızca bu satır
  // RealCellDataSource ile değişir; aşağıdaki döngü aynı kalır.
  SimCellDataSource BMS_cellSource(SimScenario::NORMAL);
  BMS_cellSource.begin();

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
            HMI_screenData.HMI_motorTorqueFeedback =
                TEL_data.TEL_motorTorqueFeedback;
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

    // 24-hücre BMS verisini oku, yorumla ve aynı HMI UART'ından Nextion'a
    // gönder. updateScreen ile aynı task içinde olduğundan UART'ta tek yazıcı
    // vardır (eşzamanlı erişim/karışma olmaz).
    BmsPackData BMS_raw = {};
    if (BMS_cellSource.read(BMS_raw) && BMS_raw.isValid) {
      const BmsComputed BMS_comp = computePack(BMS_raw);
      buildBmsNextionCommands(BMS_comp, BMS_raw, BMS_emitNextionCommand,
                              nullptr);
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

  gpio_set_level(LORA_M0_PIN, LORA_MODE_NORMAL_M0_LEVEL);
  gpio_set_level(LORA_M1_PIN, LORA_MODE_NORMAL_M1_LEVEL);

  LO_auxLevel = gpio_get_level(LORA_AUX_PIN);
  ESP_LOGI(TAG, "LoRa mode configured: M0=%d M1=%d AUX=%d",
           LORA_MODE_NORMAL_M0_LEVEL, LORA_MODE_NORMAL_M1_LEVEL, LO_auxLevel);

  // UART init for E32
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
  uart_driver_install(LORA_UART_NUM, 256, 0, 0, nullptr, 0);
  LO_telemetry.begin();

  uint8_t LO_rxBuffer[4];
  uint32_t lastStackLog = 0;
  TickType_t LO_lastTelemetryTick = 0;
  bool LO_auxNotReadyLogged = false;

  while (true) {
    esp_task_wdt_reset();

    int LO_rxLength = uart_read_bytes(LORA_UART_NUM, LO_rxBuffer,
                                      sizeof(LO_rxBuffer),
                                      pdMS_TO_TICKS(LORA_RX_TIMEOUT_MS));
    if (LO_rxLength > 0) {
      switch (LO_rxBuffer[0]) {
      case UKS_CMD_EMERGENCY_STOP:
        VcuLogic::postEvent(VcuLogic::VcuEvent::EMERGENCY_STOP);
        break;
      case UKS_CMD_START:
        VcuLogic::postEvent(VcuLogic::VcuEvent::START_REQUEST);
        break;
      case UKS_CMD_STOP:
        VcuLogic::postEvent(VcuLogic::VcuEvent::RESET);
        break;
      case UKS_CMD_DRIVE_ENABLE:
        ESP_LOGI(TAG, "LoRa command: DRIVE ENABLE request");
        VcuLogic::postEvent(VcuLogic::VcuEvent::DRIVE_ENABLE);
        break;
      default:
        break;
      }
    }

    const TickType_t LO_nowTick = xTaskGetTickCount();
    if ((LO_nowTick - LO_lastTelemetryTick) >=
        pdMS_TO_TICKS(LORA_TX_PERIOD_MS)) {
      if (TEL_sensorDataQueue != nullptr) {
        TelemetryData TEL_data = {};
        if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
          if (gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL) {
            LO_telemetry.sendStatus(TEL_data);
            LO_lastTelemetryTick = LO_nowTick;
            LO_auxNotReadyLogged = false;
          } else {
            if (!LO_auxNotReadyLogged) {
              ESP_LOGW(TAG, "LoRa AUX not ready, telemetry TX skipped");
              LO_auxNotReadyLogged = true;
            }
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
