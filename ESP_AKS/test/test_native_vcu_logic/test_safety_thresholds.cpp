#include <unity.h>

#include "VcuLogic.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::hasCriticalCondition;
using VcuLogic::hasWarningCondition;
using VcuLogic::isCurrentCritical;
using VcuLogic::isCurrentWarning;
using VcuLogic::VcuState;

// ---------------------------------------------------------------------------
// Birim: centi-Amper (0.01 A) — parser çıktısıyla aynı ölçek (G5 sonrası).
// isCurrentCritical — şarj tarafı (eşik: BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A
// = 1300 = 13.0 A; saha kalibrasyonu, 9.9 A nominal şarjın üstünde marj)
// ---------------------------------------------------------------------------
void test_isCurrentCritical_charge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentCritical(1290));  // 12.9 A — altında
}

void test_isCurrentCritical_charge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(1300));   // 13.0 A — eşikte
}

void test_isCurrentCritical_charge_above_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(1400));   // 14.0 A — üstünde
}

void test_isCurrentCritical_zero_is_safe(void) {
    TEST_ASSERT_FALSE(isCurrentCritical(0));
}

// ---------------------------------------------------------------------------
// isCurrentCritical — deşarj tarafı (eşik: -1500 centi-A = -15.0 A)
// ---------------------------------------------------------------------------
void test_isCurrentCritical_discharge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentCritical(-1490));  // 14.9 A — altında
}

void test_isCurrentCritical_discharge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(-1500));   // 15.0 A — eşikte
}

void test_isCurrentCritical_discharge_above_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(-2000));   // 20.0 A — üstünde
}

// ---------------------------------------------------------------------------
// isCurrentWarning — şarj / deşarj (eşikler: 1100 / -900 centi-A = 11.0 A / -9.0 A)
// ---------------------------------------------------------------------------
void test_isCurrentWarning_charge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentWarning(1090)); // 10.9 A
}

void test_isCurrentWarning_charge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentWarning(1100));  // 11.0 A — eşikte
}

void test_isCurrentWarning_discharge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentWarning(-890)); // 8.9 A
}

void test_isCurrentWarning_discharge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentWarning(-900));  // 9.0 A — eşikte
}

// ---------------------------------------------------------------------------
// Sıcaklık eşikleri (politika: ≥55 °C UYARI, ≥70 °C FAULT; >= semantiği).
// Kaynak sinyal DOĞRULANDI (0xE001 → TEL_bmsTempHighestC) ve
// hasWarning/hasCriticalCondition'a BAĞLI — sınır değerleri kilitle.
// ---------------------------------------------------------------------------
void test_temp_below_warn_is_clean(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 54;  // WARN eşiğinin 1 °C altı — temiz
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_temp_at_warn_threshold_is_warning(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 55;  // eşikte — WARN tetikler (>=), kritik değil
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_temp_below_crit_is_warning_only(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 69;  // CRIT eşiğinin 1 °C altı — hâlâ yalnız WARN
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_temp_at_crit_threshold_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 70;  // eşikte — FAULT tetikler (>=)
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Akım artık karar mantığına BAĞLI (sinyal DOĞRULANDI — saha gözlemi:
// şarjda +9.9 A, deşarjda −0.1…−1.5 A; işaret + şarj / − deşarj).
// Eşikler: şarj WARN 11 A / CRIT 13 A, deşarj WARN 9 A / CRIT 15 A.
// ---------------------------------------------------------------------------
// REGRESYON: sahada gözlenen nominal 9.9 A şarj akımı (990 centi-A) hiçbir
// koşul tetiklememeli — normal şarj FAULT'a/uyarıya yol açmamalı.
void test_nominal_charge_current_no_fault(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 990;  // 9.9 A şarj — saha nominali
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_charge_current_at_warn_is_warning_only(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 1100;  // 11.0 A şarj — WARN eşiğinde
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_charge_current_at_crit_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 1300;  // 13.0 A şarj — CRIT eşiğinde
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_discharge_current_at_crit_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -2000;  // 20 A deşarj — CRIT (15 A) üstünde
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Pack voltajı alt sınır (24S LiFePO4: warn ≤720, crit ≤600 deci-V)
// ---------------------------------------------------------------------------
void test_warning_voltage_above_warn_low(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 730;
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_warning_voltage_at_warn_low(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 720;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_voltage_at_crit_low(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 600;  // 60.0 V — spec min
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Pack voltajı üst sınır (24S LiFePO4: warn ≥852, crit ≥876 deci-V)
// ---------------------------------------------------------------------------
void test_warning_voltage_below_warn_high(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 840;
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_warning_voltage_at_warn_high(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 852;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_voltage_at_crit_high(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 876;  // 87.6 V — spec maks
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Hücre voltajı (0xE001 min/max — TEL_bmsCellVoltageMin/MaxDeciMv, deci-mV
// ölçekli, yalnızca TEL_cellVoltageDataValid iken değerlendirilir).
// GÜVENLİK-EŞİĞİ DÜZELTMESİ (2026-07-13): önceden burada mV-ölçekli eşikler
// (BMS_CELL_*_MV) kullanılıyordu — gerçekçi bir hücre voltajıyla (deci-mV
// ~28000-40000) overvolt dalı HER ZAMAN, undervolt dalı ise NEREDEYSE HİÇ
// tetiklenmiyordu (saha belirtisi: BMS canlıyken araç READY'ye giremiyordu).
// Artık deci-mV eşiklerle (BMS_CELL_*_DECI_MV, bkz. BmsAlgo.h) karşılaştırılır
// — aşağıdaki testler hem eski hatayı hem doğru davranışı kilitler.
// ---------------------------------------------------------------------------
void test_cell_voltage_realistic_nominal_no_condition(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMinDeciMv = 33000;  // 3300.0 mV — nominal LiFePO4 hücre
    d.TEL_bmsCellVoltageMaxDeciMv = 33500;  // 3350.0 mV — nominal
    // DÜZELTME ÖNCESİ: 33500 > eski mV eşiği (3550) olduğundan bu her zaman
    // overvolt WARN tetiklerdi — düzeltmeden önce bu test BAŞARISIZ olurdu.
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_cell_undervoltage_warn_threshold_deci_mv(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    // Hücre voltajı semantiği strictly < / > (BmsAlgo.h'deki gibi) — eşiğin
    // KENDİSİ tetiklemez, bir altı tetikler.
    d.TEL_bmsCellVoltageMinDeciMv = 27999;  // 2799.9 mV — WARN (2800) eşiğinin 0.1 mV altı
    d.TEL_bmsCellVoltageMaxDeciMv = 33000;  // temiz
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_cell_undervoltage_critical_realistic(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMinDeciMv = 24900;  // 2490.0 mV — CRIT (2500 mV) altında
    d.TEL_bmsCellVoltageMaxDeciMv = 33000;  // temiz
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_cell_overvoltage_warn_threshold_deci_mv(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMinDeciMv = 33000;  // temiz
    // Hücre voltajı semantiği strictly < / > — eşiğin KENDİSİ tetiklemez.
    d.TEL_bmsCellVoltageMaxDeciMv = 35501;  // 3550.1 mV — WARN (3550) eşiğinin 0.1 mV üstü
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_cell_overvoltage_critical_realistic(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_cellVoltageDataValid = true;
    d.TEL_bmsCellVoltageMinDeciMv = 33000;  // temiz
    d.TEL_bmsCellVoltageMaxDeciMv = 36600;  // 3660.0 mV — CRIT (3650 mV) üstünde
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// BMS FAULT state her zaman critical tetikler.
// ---------------------------------------------------------------------------
void test_critical_motor_error_flag_set(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorErrorFlags = 0x01;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_bms_error_flag_set(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsSystemState = 4;   // FAULT
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Motor timeout — IDLE'de yok sayılır, diğer durumlarda critical.
// ---------------------------------------------------------------------------
void test_motor_timeout_in_idle_is_safe(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::IDLE));
}

void test_motor_timeout_in_ready_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_motor_timeout_in_drive_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::DRIVE));
}

// ---------------------------------------------------------------------------
// BMS timeout (post-reception E000 freshness kaybı) — motor timeout ile aynı
// politika: IDLE'da yok sayılır, diğer durumlarda critical.
// ---------------------------------------------------------------------------
void test_bms_timeout_in_idle_is_safe(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTimeoutActive = true;
    d.TEL_bmsDataValid = false;  // timeout'ta CanManager ikisini birlikte set eder
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::IDLE));
}

void test_bms_timeout_in_ready_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTimeoutActive = true;
    d.TEL_bmsDataValid = false;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_bms_timeout_in_drive_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTimeoutActive = true;
    d.TEL_bmsDataValid = false;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::DRIVE));
}

// ---------------------------------------------------------------------------
// BMS data invalid — eşik kontrolü yapılmaz, motor error hâlâ critical.
// ---------------------------------------------------------------------------
void test_warning_bms_invalid_skips_thresholds(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_bmsTempHighestC = 99;
    d.TEL_bmsPackVoltageDeciV = 500;
    d.TEL_bmsCurrentCentiA = -2500;
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_bms_invalid_with_motor_error_still_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_motorErrorFlags = 0x04;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Baseline — temiz veri hiçbir koşul tetiklemez.
// ---------------------------------------------------------------------------
void test_baseline_clean_data_no_conditions(void) {
    TelemetryData d = makeTelemetryDataValid();
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::IDLE));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::DRIVE));
}

