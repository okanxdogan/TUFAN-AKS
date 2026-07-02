#include "Telemetry.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "esp_log.h"

#include <cstdio>

static constexpr const char* TAG = "Telemetry";

Telemetry::Telemetry() : TEL_isInitialized(false) {}

bool Telemetry::begin() {
    TEL_isInitialized = true;
    return true;
}

void Telemetry::sendStatus(const TelemetryData& TEL_data) {
    if (!TEL_isInitialized)
        return;

    // TEKNOFEST mandatory telemetry format:
    //   zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh
    // Separator: semicolon (;)
    // Line ending: \r\n

    // TODO: RPM->km/h conversion, will be added once wheel diameter is
    // confirmed by the MS team
    const uint16_t TEL_hizKmh = 0;

    // T_bat_C: highest battery temperature in Celsius
    const int TEL_tempBatC = static_cast<int>(TEL_data.TEL_bmsTempHighestC);

    // V_bat_C: pack voltage in decivolts (raw is deciV, TEKNOFEST expects
    // deciV integer representation — e.g. 780 for 78.0V)
    const uint16_t TEL_vBatDeciV = TEL_data.TEL_bmsPackVoltageDeciV;

    // TODO: SOC * capacity (Ah) calculation, will be added once battery
    // capacity is confirmed
    const uint32_t TEL_kalanEnerjiWh = 0;

    char TEL_payload[128];
    const int TEL_payloadLength = snprintf(
        TEL_payload, sizeof(TEL_payload),
        "%lu;%u;%d;%u;%lu\r\n",
        static_cast<unsigned long>(TEL_data.TEL_timestampMs),
        TEL_hizKmh,
        TEL_tempBatC,
        TEL_vBatDeciV,
        static_cast<unsigned long>(TEL_kalanEnerjiWh));

    if (TEL_payloadLength <= 0)
        return;

    const int TEL_txLength =
        (TEL_payloadLength < static_cast<int>(sizeof(TEL_payload)))
            ? TEL_payloadLength
            : static_cast<int>(sizeof(TEL_payload) - 1);

    const int TEL_written =
        uart_write_bytes(LORA_UART_NUM, TEL_payload, TEL_txLength);
    if (TEL_written < 0) {
        ESP_LOGE(TAG, "Telemetry TX failed");
        return;
    }
}
