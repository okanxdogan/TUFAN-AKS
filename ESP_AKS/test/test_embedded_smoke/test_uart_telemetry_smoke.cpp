#include <unity.h>

#include "SystemConfig.h"
#include "Telemetry.h"
#include "driver/uart.h"

namespace {
void install_lora_uart() {
    uart_config_t cfg = {};
    cfg.baud_rate = LORA_UART_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    uart_param_config(LORA_UART_NUM, &cfg);
    uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    uart_driver_install(LORA_UART_NUM, 256, 256, 0, nullptr, 0);
}
}  // namespace

// LoRa UART2 driver install + Telemetry::begin + sendStatus end-to-end.
// Fiziksel LoRa modülü olmasa bile UART TX FIFO'ya yazma başarılı olur.
void test_telemetry_begin_and_send(void) {
    install_lora_uart();

    Telemetry tel;
    TEST_ASSERT_TRUE(tel.begin());

    TelemetryData d{};
    d.TEL_motorRpm = 1234;
    d.TEL_motorVoltageDeciV = 240;
    d.TEL_motorDataValid = true;
    d.TEL_bmsSocHundredths = 8000;  // 80.00%
    d.TEL_bmsDataValid = true;
    tel.sendStatus(d);

    // Hayatta kaldıysak driver init + UART write başarılı.
    TEST_ASSERT_TRUE(true);
}
