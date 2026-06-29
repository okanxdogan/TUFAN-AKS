#include "DisplayHMI.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

static constexpr const char *TAG = "DisplayHMI";

DisplayHMI::DisplayHMI()
    : HMI_isInitialized(false),
      HMI_hasCachedScreen(false),
      HMI_lastScreenData({}) {}

bool DisplayHMI::begin() {
    if (HMI_isInitialized) return true;

    uart_config_t HMI_uartConfig = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT
    };

    if (uart_param_config(HMI_UART_NUM, &HMI_uartConfig) != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed");
        return false;
    }

    if (uart_set_pin(HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed (TX=%d, RX=%d)", HMI_TX_PIN, HMI_RX_PIN);
        return false;
    }

    if (uart_driver_install(HMI_UART_NUM, 256, 256, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed");
        return false;
    }

    HMI_isInitialized = true;

    // Nextion açılış mesajlarını temizle
    HMI_drainRxBuffer();

    // Nextion acknowledge yanıtlarını kapat (bkcmd=0)
    // Aksi halde her komut sonrası gelen 0x01/0x02/0x03 yanıtları
    // readTouchCommand tarafından sahte komut olarak yorumlanır
    const char *HMI_bkcmd = "bkcmd=0";
    uart_write_bytes(HMI_UART_NUM, HMI_bkcmd, 7);
    HMI_sendEndBytes();
    vTaskDelay(pdMS_TO_TICKS(50));  // Nextion'ın işlemesi için bekle
    HMI_drainRxBuffer();            // bkcmd komutunun kendi acknowledge'ını temizle

    ESP_LOGI(TAG, "Initialized on UART%d (TX=IO%d, RX=IO%d)", HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN);
    return true;
}

void DisplayHMI::HMI_drainRxBuffer() {
    uint8_t HMI_drainBuf[32];
    while (uart_read_bytes(HMI_UART_NUM, HMI_drainBuf, sizeof(HMI_drainBuf), 0) > 0) {
        // Nextion acknowledge/error yanıtlarını temizle
    }
}

void DisplayHMI::updateScreen(const HMI_DisplayData& HMI_data) {
    if (!HMI_isInitialized) return;

    const bool HMI_forceRefresh = !HMI_hasCachedScreen;
    char HMI_currentErrorText[16];
    char HMI_lastErrorText[16];

    HMI_formatErrorText(HMI_data.HMI_motorErrorFlags, HMI_currentErrorText,
                        sizeof(HMI_currentErrorText));
    HMI_formatErrorText(HMI_lastScreenData.HMI_motorErrorFlags,
                        HMI_lastErrorText, sizeof(HMI_lastErrorText));

    HMI_sendNumericIfChanged("speed", HMI_data.HMI_currentSpeed,
                             HMI_lastScreenData.HMI_currentSpeed,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("bat", HMI_data.HMI_currentBattery,
                             HMI_lastScreenData.HMI_currentBattery,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("rpm", HMI_data.HMI_motorRpm,
                             HMI_lastScreenData.HMI_motorRpm,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("torque", HMI_data.HMI_motorTorqueFeedback,
                             HMI_lastScreenData.HMI_motorTorqueFeedback,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("temp", HMI_data.HMI_bmsTemperatureC,
                             HMI_lastScreenData.HMI_bmsTemperatureC,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("packv", HMI_data.HMI_bmsPackVoltageDeciV,
                             HMI_lastScreenData.HMI_bmsPackVoltageDeciV,
                             HMI_forceRefresh);
    // Gerçek zamanlı max/min hücre gerilimi (mV). Şimdilik UYKUDA: demo,
    // cellmax/cellmin'i sim veriyle gösteriyor. Gerçek zamanlıya geçişte
    // BMS_USE_REALTIME_MINMAX tanımlanır VE demo'nun sim cellmax/cellmin emit'i
    // (BmsNextionPacket.cpp) kaldırılır — böylece çakışma olmaz.
#ifdef BMS_USE_REALTIME_MINMAX
    HMI_sendNumericIfChanged("cellmax", HMI_data.HMI_bmsCellVoltageMaxMv,
                             HMI_lastScreenData.HMI_bmsCellVoltageMaxMv,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("cellmin", HMI_data.HMI_bmsCellVoltageMinMv,
                             HMI_lastScreenData.HMI_bmsCellVoltageMinMv,
                             HMI_forceRefresh);
#endif

    HMI_sendTextIfChanged("state", HMI_getStateText(HMI_data.HMI_vcuState),
                          HMI_getStateText(HMI_lastScreenData.HMI_vcuState),
                          HMI_forceRefresh);
    HMI_sendTextIfChanged("motorErr", HMI_currentErrorText, HMI_lastErrorText,
                          HMI_forceRefresh);
    HMI_sendTextIfChanged("valid",
                          HMI_getValidityText(HMI_data.HMI_motorDataValid,
                                              HMI_data.HMI_motorTimeoutActive),
                          HMI_getValidityText(
                              HMI_lastScreenData.HMI_motorDataValid,
                              HMI_lastScreenData.HMI_motorTimeoutActive),
                          HMI_forceRefresh);
    HMI_sendTextIfChanged("contactor",
                          HMI_getContactorText(HMI_data.HMI_contactorClosed),
                          HMI_getContactorText(
                              HMI_lastScreenData.HMI_contactorClosed),
                          HMI_forceRefresh);

    HMI_lastScreenData = HMI_data;
    HMI_hasCachedScreen = true;
}

bool DisplayHMI::readTouchCommand(uint8_t& HMI_command) {
    if (!HMI_isInitialized) return false;

    uint8_t HMI_rxBuf[1];
    int HMI_rxBytes = uart_read_bytes(HMI_UART_NUM, HMI_rxBuf, 1, pdMS_TO_TICKS(10));

    if (HMI_rxBytes <= 0) return false;

    // Gürültü byte'larını filtrele:
    // 0xFF = Nextion end-byte (buffer'da kalan artık)
    // 0x00 = Nextion Invalid Instruction yanıtı
    // bkcmd=0 sayesinde 0x01-0x05 ack yanıtları gelmez,
    // dolayısıyla bu değerler güvenle komut olarak yorumlanabilir
    if (HMI_rxBuf[0] == 0xFF || HMI_rxBuf[0] == 0x00) return false;

    HMI_command = HMI_rxBuf[0];
    return true;
}
