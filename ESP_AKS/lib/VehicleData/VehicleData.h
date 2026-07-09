#pragma once

#include <cstdint>

// VehicleData.h — TEK veri-sözleşmesi (M3: katman ihlali düzeltmesi).
//
// TelemetryData üç rolü taşır: (1) CAN parse çıktısı, (2) task-arası kuyruk
// yükü, (3) LoRa frame kaynağı. Eskiden bu struct LoRa TX sınıfının header'ı
// olan Telemetry.h içinde yaşıyordu ve Telemetry.h → VehicleParams.h çektiği
// için saf CAN parser'ı (CanParse) derlemek tekerlek çapı sabitine ve LoRa
// katmanına bağımlı oluyordu.
//
// Bu header YALNIZCA saf veri sözleşmesidir: donanım, LoRa, VehicleParams ve
// hız hesabı bağımlılığı YOKTUR. Hız/tur hesabı (VehicleParams'a ihtiyaç duyan
// tek yer) LoRa tarafında Telemetry.h'de kalır. Alan adları DEĞİŞMEDİ.
struct TelemetryData {
    int16_t TEL_motorRpm;
    uint16_t TEL_motorVoltageDeciV;
    uint8_t TEL_motorErrorFlags;
    bool TEL_motorDataValid;
    bool TEL_motorTimeoutActive;

    // Lithium Balance c-BMS — per-hücre alanlar (kaynak CAN ID henüz BİLİNMİYOR)
    uint16_t TEL_bmsCellVoltageMaxDeciMv;  // BİLİNMİYOR — kaynak ID çözülmedi
    uint16_t TEL_bmsCellVoltageMinDeciMv;  // BİLİNMİYOR — kaynak ID çözülmedi
    uint8_t TEL_bmsSystemState;            // BİLİNMİYOR — kaynak ID çözülmedi

    // Lithium Balance c-BMS — CAN ID 0xE000 (DOĞRULANDI, bkz. CAN_Message_Table.md)
    uint16_t TEL_bmsPackVoltageDeciV;  // byte[2:3], raw × 0.1 = V — DOĞRULANDI
    int32_t TEL_bmsCurrentCentiA;     // byte[0:1], int16 signed, raw × 10 = centi-A — DOĞRULANDI
    uint16_t TEL_bmsSocHundredths;     // byte[4:5], uint16, raw × 0.01 = % (SoC 1) — DOĞRULANDI
    uint16_t TEL_bmsSoc2Hundredths;    // byte[6:7], uint16, raw × 0.01 = % (SoC 2) — DOĞRULANDI

    // Lithium Balance c-BMS — CAN ID 0xE001 (kısmi DOĞRULANDI)
    int8_t TEL_bmsTempHighestC;       // max(byte[6], byte[7]), int8 °C — DOĞRULANDI
    int8_t TEL_bmsTempLowestC;        // min(byte[6], byte[7]), int8 °C — DOĞRULANDI

    bool TEL_bmsDataValid;
    // Post-reception E000 freshness kaybı (krş. TEL_motorTimeoutActive).
    // En az bir E000 görüldükten sonra CAN_BMS_STATUS_TIMEOUT_MS içinde yeni
    // frame gelmezse true olur; VcuLogic IDLE dışında kritik fault sayar.
    bool TEL_bmsTimeoutActive;

    uint32_t TEL_timestampMs   = 0;   // ms since boot — stamped when packet is created
    uint16_t TEL_speedKmhX10  = 0;   // vehicle speed ×10 km/h, filled via rpmToSpeedKmhX10()
};
