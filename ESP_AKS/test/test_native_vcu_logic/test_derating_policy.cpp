#include <unity.h>

#include "DeratingPolicy.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;

// ===========================================================================
// AÇIK İŞ B12 iskeleti — DeratingPolicy::computeTorqueAllowPercent.
// Basit 3 kademe: NOMINAL(100) / WARNING(50) / APPROACHING_CRITICAL(20).
// "Yaklaşma" = WARN->CRITICAL aralığının %90'ının tüketilmesi (bkz.
// DeratingPolicy.h yorumu — ham eşiğin %90'ı DEĞİL).
// ===========================================================================

// --- bmsValid=false: notr (100), karar verilemez ---------------------------
void test_derating_neutral_when_bms_data_invalid(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_bmsTempHighestC = 90;  // acikca CRITICAL-otesi bir sicaklik olsa bile

    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Temiz veri (hicbir WARN yok): 100 -------------------------------------
void test_derating_nominal_when_all_signals_clean(void) {
    TelemetryData d = makeTelemetryDataValid();
    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Sicaklik: WARN esiginin 1 altinda -> hala 100 -------------------------
void test_derating_temp_just_below_warn_is_nominal(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 54;  // WARN=55'in 1 altinda
    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Sicaklik: TAM WARN esiginde -> 50 -------------------------------------
void test_derating_temp_at_warn_threshold_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 55;  // BMS_WARN_MAX_TEMP_C
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Sicaklik: %90-CRITICAL sinirinin 1 altinda -> hala 50 -----------------
// WARN=55, CRIT=70, span=15, approachAt = 55 + (15*90)/100 = 55+13 = 68.
void test_derating_temp_just_below_approach_boundary_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 67;  // approachAt(68)'in 1 altinda
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Sicaklik: TAM %90-CRITICAL sinirinda -> 20 ----------------------------
void test_derating_temp_at_approach_boundary_is_approaching_critical_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 68;  // approachAt
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Sicaklik: CRITICAL esiginin kendisinde de hala 20 (fonksiyon CRITICAL'i
// FAULT'a cevirmez, yalnizca kademe hesaplar; run() zaten bu noktaya
// CRITICAL iken hic ulasmaz cunku FAULT kontrolu daha once return eder) ----
void test_derating_temp_at_critical_threshold_is_still_approaching_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 70;  // BMS_CRITICAL_MAX_TEMP_C
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Sarj akimi: WARN=1100, CRIT=1300, span=200, approachAt=1100+180=1280 --
void test_derating_charge_current_below_warn_is_nominal(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 1090;
    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_charge_current_at_warn_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 1100;
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_charge_current_at_approach_boundary_is_approaching_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 1280;
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Desarj akimi (negatif): WARN=900, CRIT=1500, span=600, approachAt =
// 900+(600*90)/100 = 900+540 = 1440 (magnitude) -----------------------------
void test_derating_discharge_current_below_warn_is_nominal(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -890;
    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_discharge_current_at_warn_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -900;
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_discharge_current_at_approach_boundary_is_approaching_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -1440;
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Pack dusuk gerilim (MIN yonlu): WARN=720, CRIT=600, span=120,
// approachAt = 720 - (120*90)/100 = 720-108 = 612 ---------------------------
void test_derating_pack_undervoltage_above_warn_is_nominal(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 721;
    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_pack_undervoltage_at_warn_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 720;
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_pack_undervoltage_at_approach_boundary_is_approaching_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 612;
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Pack asiri gerilim (MAX yonlu): WARN=852, CRIT=876, span=24,
// approachAt = 852 + (24*90)/100 = 852+21 = 873 -----------------------------
void test_derating_pack_overvoltage_at_warn_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 852;
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_pack_overvoltage_at_approach_boundary_is_approaching_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 873;
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Hucre voltaji yalniz TEL_cellVoltageDataValid iken degerlendirilir ----
void test_derating_ignores_cell_voltage_when_not_fresh(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = false;
    d.TEL_bmsCellVoltageMinDeciMv = 20000;  // acikca CRITICAL-altinda bir deger (deci-mV)
    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

// GÜVENLİK-EŞİĞİ DÜZELTMESİ (2026-07-13): asagidaki degerler artik deci-mV
// olcegindedir (BMS_CELL_UNDERVOLT_WARN_DECI_MV=28000, _CRIT_DECI_MV=25000,
// bkz. BmsAlgo.h) — TEL_bmsCellVoltageMin/MaxDeciMv alaniyla AYNI olcek.
// Onceden burada mV-olcekli (2800/2500) degerler kullaniliyordu ve kod da
// mV-olcekli esiklerle karsilastiriyordu; DeratingPolicy.h artik deci-mV
// esikleri (BMS_CELL_*_DECI_MV) kullandigi icin test degerleri de gercekci
// deci-mV araligina (bkz. test_native_can_parsing/test_cell_voltage_parse.cpp
// ~33505/34311 gibi degerler) tasindi.
//
// Hucre dusuk gerilim: WARN=28000, CRIT=25000, span=3000,
// approachAt = 28000 - (3000*90)/100 = 28000-2700 = 25300.
void test_derating_cell_undervoltage_at_warn_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMaxDeciMv = 33000;  // 3300.0 mV — nominal, esiklerin altinda — izolasyon
    d.TEL_bmsCellVoltageMinDeciMv = 28000;
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_cell_undervoltage_at_approach_boundary_is_approaching_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMaxDeciMv = 33000;  // nominal — izolasyon
    d.TEL_bmsCellVoltageMinDeciMv = 25300;
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// Hucre asiri gerilim: WARN=35500, CRIT=36500, span=1000,
// approachAt = 35500 + (1000*90)/100 = 35500+900 = 36400.
void test_derating_cell_overvoltage_at_warn_is_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMinDeciMv = 33000;  // nominal — izolasyon
    d.TEL_bmsCellVoltageMaxDeciMv = 35500;
    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_cell_overvoltage_at_approach_boundary_is_approaching_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMinDeciMv = 33000;  // nominal — izolasyon
    d.TEL_bmsCellVoltageMaxDeciMv = 36400;
    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

// REGRESYON: gercekci bir hucre voltajiyla (3300-3350 mV, DUZELTME ONCESI
// eski mV-olcekli esikle HER ZAMAN overvolt WARN tetiklerdi) artik NOMINAL.
void test_derating_cell_voltage_realistic_nominal_is_nominal(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMinDeciMv = 33000;  // 3300.0 mV
    d.TEL_bmsCellVoltageMaxDeciMv = 33500;  // 3350.0 mV
    TEST_ASSERT_EQUAL_UINT8(100, DeratingPolicy::computeTorqueAllowPercent(d));
}

// --- Coklu-WARN kombinasyonu: en kisitlayici (en dusuk yuzde) kazanir ------
void test_derating_multiple_warnings_worst_case_wins(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 55;          // WARNING tier (50)
    d.TEL_bmsCurrentCentiA = 1280;       // APPROACHING_CRITICAL tier (20)
    // Pack voltaji ve hucre voltaji temiz kalir.

    TEST_ASSERT_EQUAL_UINT8(20, DeratingPolicy::computeTorqueAllowPercent(d));
}

void test_derating_two_warning_tier_signals_stay_at_warning_tier(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 55;     // WARNING tier (50)
    d.TEL_bmsCurrentCentiA = 1100;  // WARNING tier (50)

    TEST_ASSERT_EQUAL_UINT8(50, DeratingPolicy::computeTorqueAllowPercent(d));
}
