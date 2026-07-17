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
      HMI_resetPending(false),
      HMI_resetWarnLoggedOnce(false),
      HMI_lastResetWarnTick(0),
      HMI_resetCount(0),
      HMI_lastResyncTick(0),
      HMI_nextResyncField(0),
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
    HMI_sendBkcmd0();
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

// bkcmd=0 Nextion'da KALICI DEĞİLDİR — ekran reset'inde Editor varsayılanına
// döner; hem begin()'de hem reset kurtarmasında gönderilir.
void DisplayHMI::HMI_sendBkcmd0() {
    const char *HMI_bkcmd = "bkcmd=0";
    uart_write_bytes(HMI_UART_NUM, HMI_bkcmd, 7);
    HMI_sendEndBytes();
}

void DisplayHMI::forceFullRefresh() { HMI_hasCachedScreen = false; }

bool DisplayHMI::consumeResetFlag() {
    const bool HMI_was = HMI_resetPending;
    HMI_resetPending = false;
    return HMI_was;
}

// Startup event (00 00 00 FF FF FF) yakalandı: Nextion brown-out/reset attı,
// tüm component'ler Editor varsayılanına döndü. Kurtarma:
//   (a) bkcmd=0'ı yeniden gönder (reset ile kaybolur),
//   (b) DisplayHMI cache'ini geçersiz kıl → bir sonraki updateScreen tüm
//       skalar alanları yeniden basar (boot'taki ilk çağrıyla birebir aynı yol),
//   (c) HMI_resetPending → HMI_Task consumeResetFlag() ile BmsNextionCache'i
//       sıfırlar; hücre barları maxBytes bütçesiyle döngülere yayılarak dolar.
// Burada drain/delay YAPILMAZ — readTouchCommand'ın byte akışı bozulmamalı.
void DisplayHMI::HMI_handleNextionReset() {
    ++HMI_resetCount;
    HMI_sendBkcmd0();
    forceFullRefresh();
    HMI_resetPending = true;

    // Oran-sınırlı WARN (CAN_RX_STATS_LOG_INTERVAL_MS deseni): ekran güç
    // hattında sürekli brown-out varsa log spam yapılmaz, toplam sayaçla
    // en fazla 1 WARN / HMI_RESET_WARN_LOG_INTERVAL_MS basılır.
    const uint32_t HMI_now = xTaskGetTickCount();
    if (!HMI_resetWarnLoggedOnce ||
        (HMI_now - HMI_lastResetWarnTick) >=
            pdMS_TO_TICKS(HMI_RESET_WARN_LOG_INTERVAL_MS)) {
        ESP_LOGW(TAG,
                 "Nextion reset algilandi (toplam %lu), ekran yeniden "
                 "dolduruluyor",
                 (unsigned long)HMI_resetCount);
        HMI_lastResetWarnTick = HMI_now;
        HMI_resetWarnLoggedOnce = true;
    }
}

void DisplayHMI::updateScreen(const HMI_DisplayData& HMI_data) {
    if (!HMI_isInitialized) return;

    const bool HMI_forceRefresh = !HMI_hasCachedScreen;

    // Round-robin resync emniyet katmanı (bkz. ResyncPolicy.h): Startup
    // event'i brown-out sırasında RX hattında kaybolursa reset dedektörü kör
    // kalır — bu yüzden her HMI_RESYNC_INTERVAL_MS'te bir SIRADAKİ TEK alan
    // cache'e bakılmaksızın zorla gönderilir (burst yok, bütçe aşımı yok).
    const int HMI_resyncField = hmi_resync_due_field(
        xTaskGetTickCount(), HMI_lastResyncTick, HMI_nextResyncField,
        HMI_RESYNC_FIELD_COUNT, pdMS_TO_TICKS(HMI_RESYNC_INTERVAL_MS));
    const auto HMI_force = [&](HMI_ResyncField HMI_field) {
        return HMI_forceRefresh || HMI_resyncField == (int)HMI_field;
    };

    char HMI_currentErrorText[16];
    char HMI_lastErrorText[16];

    HMI_formatErrorText(HMI_data.HMI_motorErrorFlags, HMI_currentErrorText,
                        sizeof(HMI_currentErrorText));
    HMI_formatErrorText(HMI_lastScreenData.HMI_motorErrorFlags,
                        HMI_lastErrorText, sizeof(HMI_lastErrorText));

    HMI_sendNumericIfChanged("speed", HMI_data.HMI_currentSpeed,
                             HMI_lastScreenData.HMI_currentSpeed,
                             HMI_force(HMI_RESYNC_SPEED));
    HMI_sendNumericIfChanged("bat", HMI_data.HMI_currentBattery,
                             HMI_lastScreenData.HMI_currentBattery,
                             HMI_force(HMI_RESYNC_BAT));
    HMI_sendNumericIfChanged("rpm", HMI_data.HMI_motorRpm,
                             HMI_lastScreenData.HMI_motorRpm,
                             HMI_force(HMI_RESYNC_RPM));
    HMI_sendNumericIfChanged("torque", HMI_data.HMI_motorTorqueFeedback,
                             HMI_lastScreenData.HMI_motorTorqueFeedback,
                             HMI_force(HMI_RESYNC_TORQUE));
    HMI_sendNumericIfChanged("temp", HMI_data.HMI_bmsTemperatureC,
                             HMI_lastScreenData.HMI_bmsTemperatureC,
                             HMI_force(HMI_RESYNC_TEMP));
    // packv Nextion'da 1 ondalıklı, packa 2 ondalıklı float (xfloat) —
    // ".val" packv için gerçek_değer×10, packa için gerçek_değer×100
    // olacak şekilde ölçeklenir (bkz. HMIHelpers.h).
    HMI_sendNumericIfChanged(
        "packv", HMI_packVoltageToXfloat(HMI_data.HMI_bmsPackVoltageDeciV),
        HMI_packVoltageToXfloat(HMI_lastScreenData.HMI_bmsPackVoltageDeciV),
        HMI_force(HMI_RESYNC_PACKV));
    HMI_sendNumericIfChanged(
        "packa", HMI_packCurrentToXfloat(HMI_data.HMI_bmsPackCurrentCentiA),
        HMI_packCurrentToXfloat(HMI_lastScreenData.HMI_bmsPackCurrentCentiA),
        HMI_force(HMI_RESYNC_PACKA));

    HMI_sendTextIfChanged("state", HMI_getStateText(HMI_data.HMI_vcuState),
                          HMI_getStateText(HMI_lastScreenData.HMI_vcuState),
                          HMI_force(HMI_RESYNC_STATE));
    HMI_sendTextIfChanged("motorErr", HMI_currentErrorText, HMI_lastErrorText,
                          HMI_force(HMI_RESYNC_MOTOR_ERR));
    HMI_sendTextIfChanged("valid",
                          HMI_getValidityText(HMI_data.HMI_motorDataValid,
                                              HMI_data.HMI_motorTimeoutActive),
                          HMI_getValidityText(
                              HMI_lastScreenData.HMI_motorDataValid,
                              HMI_lastScreenData.HMI_motorTimeoutActive),
                          HMI_force(HMI_RESYNC_VALID));
    HMI_sendTextIfChanged("contactor",
                          HMI_getContactorText(HMI_data.HMI_contactorClosed),
                          HMI_getContactorText(
                              HMI_lastScreenData.HMI_contactorClosed),
                          HMI_force(HMI_RESYNC_CONTACTOR));

    HMI_lastScreenData = HMI_data;
    HMI_hasCachedScreen = true;
}

bool DisplayHMI::readTouchCommand(uint8_t& HMI_command) {
    if (!HMI_isInitialized) return false;

    // --- HMI Command Frame Format ---
    // The HMI must send commands as a 3-byte frame to ensure integrity:
    // [HEADER] [COMMAND] [CHECKSUM]
    // HEADER   = 0x5A
    // COMMAND  = HMI_CMD_... (e.g. 0x01 for START)
    // CHECKSUM = ~COMMAND (bitwise NOT of COMMAND)
    // 
    // Example START frame: 0x5A 0x01 0xFE
    
    static int rxState = 0;
    static uint8_t pendingCmd = 0;

    uint8_t rxByte;
    // Timeout pdMS_TO_TICKS(10) ile en az 1 byte bekler (eski davranis),
    // ardindan bufferdaki kalan bytelari 0 timeout ile ceker.
    int rxBytes = uart_read_bytes(HMI_UART_NUM, &rxByte, 1, pdMS_TO_TICKS(10));
    if (rxBytes <= 0) return false;

    do {
        // Nextion reset dedektörü ham byte akışını PARALEL gözler — aşağıdaki
        // 0x5A çerçeve mantığından tamamen bağımsızdır (Startup event'inin
        // 0x00/0xFF byte'ları zaten çerçeve parser'ında atlanıyor).
        if (HMI_resetDetect.HMI_feedByte(rxByte)) {
            HMI_handleNextionReset();
        }

        if (rxByte == 0xFF || rxByte == 0x00) {
            // Nextion invalid/ack artiklari - guvenle atla ve state resetle
            rxState = 0;
            continue;
        }

        if (rxState == 0) {
            if (rxByte == 0x5A) {
                rxState = 1;
            }
        } else if (rxState == 1) {
            pendingCmd = rxByte;
            rxState = 2;
        } else if (rxState == 2) {
            uint8_t expectedChecksum = (uint8_t)(~pendingCmd);
            rxState = 0; // reset state for next command
            if (rxByte == expectedChecksum) {
                HMI_command = pendingCmd;
                return true;
            } else {
                ESP_LOGW(TAG, "HMI command checksum mismatch: cmd=0x%02X, csum=0x%02X, expected=0x%02X",
                         pendingCmd, rxByte, expectedChecksum);
            }
        }
    } while (uart_read_bytes(HMI_UART_NUM, &rxByte, 1, 0) == 1);

    return false;
}
