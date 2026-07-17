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

    // Lithium Balance c-BMS — per-hücre alanlar (kaynak DOĞRULANDI)
    //
    // DİKKAT — İKİ FARKLI BİRİM AYNI ANDA YAŞIYOR (2026-07-13 güvenlik-eşiği
    // düzeltmesinde teyit edildi):
    //   * TEL_bmsCellVoltages[24]  → GERÇEK mV (CanParse::parseLbBmsE015..E020
    //     ham CAN byte'ını /10'a böler, sonucu doğrudan mV olarak yazar).
    //   * TEL_bmsCellVoltageMax/Min/AvgDeciMv → DECİ-mV (CanParse::
    //     parseLbBmsE001 ham CAN byte'ını HİÇBİR BÖLME OLMADAN doğrudan yazar;
    //     mV'ye çevirmek için ayrıca /10 gerekir — gerçek bir hücre için
    //     ~28000-40000 aralığı, bkz. test_cell_voltage_parse.cpp). Wire
    //     formatı da bu ölçekte (UKS_LoRa_Protocol.md cellVMax/cellVMin ×0.1
    //     mV) — bu alan wire'a doğrudan kopyalanır (Telemetry.cpp), DEĞİŞMEZ.
    // Bu iki alan grubunu KARIŞTIRMAYIN: aynı eşikle (BMS_CELL_*_MV vs
    // BMS_CELL_*_DECI_MV, bkz. BmsAlgo.h) karşılaştırılmamalıdır — BmsAlgo.cpp
    // ilkini (mV, main.cpp'te TEL_bmsCellVoltages[]'ten kopyalanır), VcuLogic.h/
    // DeratingPolicy.h ikincisini (deci-mV) kullanır.
    uint16_t TEL_bmsCellVoltages[24];      // DOĞRULANDI — kaynak: E015(hücre 0-3) E016(4-7) E017(8-11) E018(12-15) E019(16-19) E020(20-23), raw/10 = mV (GERÇEK mV)
    uint16_t TEL_bmsCellVoltageMaxDeciMv;  // DOĞRULANDI — 0xE001 byte[2:3], HAM (deci-mV, /10 YAPILMAZ)
    uint16_t TEL_bmsCellVoltageMinDeciMv;  // DOĞRULANDI — 0xE001 byte[0:1], HAM (deci-mV, /10 YAPILMAZ)
    uint16_t TEL_bmsCellVoltageAvgDeciMv = 0; // DOĞRULANDI — 0xE001 byte[4:5], HAM (deci-mV, /10 YAPILMAZ, ortalama hücre voltajı)
    uint8_t TEL_bmsSystemState;            // BİLİNMİYOR — kaynak ID çözülmedi

    bool TEL_cellVoltageDataValid = false;
    bool TEL_cellVoltageTimeoutActive = false;

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

    // Charger komut akışı (0x1806E5F4) taze mi? CanManager::getTelemetryData
    // CAN_chargerValid'den doldurur (CAN_CHARGER_TIMEOUT_MS freshness dahil).
    // YALNIZCA DAHİLİ kullanım: VcuLogic S1/S2 mod anahtarlaması girdisi
    // (RELAY_ROLES_ASSIGNED=1, şartname 8.2.a.iii). LoRa wire formatına
    // (Telemetry.cpp sendStatus, 19 alan v2) ASLA serialize EDİLMEZ.
    // Charger akışı opsiyoneldir — false, FAULT değildir.
    bool TEL_chargerActive = false;

    uint32_t TEL_timestampMs   = 0;   // ms since boot — stamped when packet is created
    uint16_t TEL_speedKmhX10  = 0;   // vehicle speed ×10 km/h, filled via rpmToSpeedKmhX10()
};
