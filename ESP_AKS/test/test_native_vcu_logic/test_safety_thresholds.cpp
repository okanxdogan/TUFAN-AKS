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
// isCurrentCritical — şarj tarafı (eşik: BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A = 100)
// ---------------------------------------------------------------------------
void test_isCurrentCritical_charge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentCritical(90));   // 0.9 A — altında
}

void test_isCurrentCritical_charge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(100));   // 1.0 A — eşikte
}

void test_isCurrentCritical_charge_above_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(110));   // 1.1 A — üstünde
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
// isCurrentWarning — şarj / deşarj (eşikler: 90 / -900 centi-A = 0.9 A / -9.0 A)
// ---------------------------------------------------------------------------
void test_isCurrentWarning_charge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentWarning(80));   // 0.8 A
}

void test_isCurrentWarning_charge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentWarning(90));    // 0.9 A — eşikte
}

void test_isCurrentWarning_discharge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentWarning(-890)); // 8.9 A
}

void test_isCurrentWarning_discharge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentWarning(-900));  // 9.0 A — eşikte
}

// ---------------------------------------------------------------------------
// DOĞRULANMAMIŞ sinyaller karar mantığına BAĞLI DEĞİL (Ek B güven kuralı):
// sıcaklık ve akım alanları hiçbir CAN ID'den parse edilmediği için
// hasWarning/hasCriticalCondition bunlara bakmaz — aşırı değerler bile
// koşul tetiklememeli. Kaynak sinyal + ölçek doğrulanınca bu testler
// gerçek eşik testlerine dönüştürülecek.
// ---------------------------------------------------------------------------
void test_unverified_temp_not_wired(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 99;  // eşiklerin çok üstünde ama sinyal DOĞRULANMADI
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_unverified_current_not_wired(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -2000;  // 20 A — ama sinyal DOĞRULANMADI
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
    d.TEL_bmsCurrentCentiA = 120;    // 1.2 A şarj
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
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

// NOT: "Akım eşikleri uçtan uca" testleri kaldırıldı — akım sinyali
// DOĞRULANMADIĞI için karar mantığına bağlı değil; kapsam
// test_unverified_current_not_wired ile ters yönde korunuyor.
