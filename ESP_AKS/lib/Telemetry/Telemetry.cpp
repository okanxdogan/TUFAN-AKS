#include "Telemetry.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "esp_log.h"

#include <cstdio>

static constexpr const char* TAG = "Telemetry";

Telemetry::Telemetry() : TEL_isInitialized(false), TEL_sequenceCounter(0) {}

bool Telemetry::begin() {
    TEL_isInitialized = true;
    TEL_sequenceCounter = 0;
    return true;
}

void Telemetry::sendStatus(const TelemetryData& TEL_data) {
    if (!TEL_isInitialized)
        return;

    // Format: TEL,ver,seq,rpm,motorVoltDeciV,motorErr,motorValid,motorTimeout,
    //         cellVMax,cellVMin,tempH,tempL,sysState,packV,current,soc,
    //         bmsValid,tsMs,spdX10
    // `current` centi-Amper (0.01 A) birimindedir — UKS /100 ile A'e çevirir
    // (birim sözleşmesi: Telemetry.h::sendStatus yorumu).
    //
    // NOT: 4. alan sözleşmede "torque" (int16, -32768..32767) olarak
    // adlandırılır ama burada TEL_motorVoltageDeciV (motor voltajı, deciV)
    // yazılır — semantik uyumsuzluk bilerek kayıt altına alındı, bkz.
    // Telemetry.h::sendStatus yorumu ve Documents/TORQUE_ALAN_KARAR_NOTU.md.
    // Çağıran taraf bu değeri sendStatus'a geçmeden önce
    // TelemetrySanitize::sanitizeForUplink() ile 32767'ye kırpar; sendStatus
    // burada AYRICA kırpma YAPMAZ (tek ortak sanitize kapısına güvenir).
    char TEL_payload[192];
    const int TEL_payloadLength = snprintf(
        TEL_payload, sizeof(TEL_payload),
        "TEL,%d,%lu,%d,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u,%ld,%u,%u,%lu,%u\r\n",
        LORA_PROTOCOL_VERSION,
        static_cast<unsigned long>(TEL_sequenceCounter),
        static_cast<int>(TEL_data.TEL_motorRpm),
        static_cast<unsigned>(TEL_data.TEL_motorVoltageDeciV),
        TEL_data.TEL_motorErrorFlags,
        TEL_data.TEL_motorDataValid ? 1u : 0u,
        TEL_data.TEL_motorTimeoutActive ? 1u : 0u,
        TEL_data.TEL_bmsCellVoltageMaxDeciMv,
        TEL_data.TEL_bmsCellVoltageMinDeciMv,
        static_cast<int>(TEL_data.TEL_bmsTempHighestC),
        static_cast<int>(TEL_data.TEL_bmsTempLowestC),
        TEL_data.TEL_bmsSystemState,
        TEL_data.TEL_bmsPackVoltageDeciV,
        static_cast<long>(TEL_data.TEL_bmsCurrentCentiA),
        TEL_data.TEL_bmsSocHundredths,
        TEL_data.TEL_bmsDataValid ? 1u : 0u,
        static_cast<unsigned long>(TEL_data.TEL_timestampMs),
        TEL_data.TEL_speedKmhX10);

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

    TEL_sequenceCounter++;
}
