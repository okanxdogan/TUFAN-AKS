#include <unity.h>

#include "VcuLogic.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::isResetInterlockSatisfied;
using VcuLogic::VcuState;

// ---------------------------------------------------------------------------
// Reset interlock — FAULT/EMERGENCY_STOP'tan IDLE'ya geçiş için ön koşul.
// Hem motor/bms error flag temiz olmalı, hem de hasCriticalCondition false
// olmalı. Aksi halde reset reddedilir.
// ---------------------------------------------------------------------------
void test_reset_interlock_clean_baseline_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::EMERGENCY_STOP));
}

void test_reset_interlock_motor_error_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorErrorFlags = 0x02;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_bms_error_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsSystemState = 4;  // FAULT
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_unverified_bms_system_state_does_not_block(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsSystemState = 4;  // FAULT shaped output
    d.TEL_bmsDataValid = false;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// Sıcaklık artık karar mantığına BAĞLI (≥70 °C kritik): kritik sıcaklık
// sürerken reset REDDEDİLMELİ; yalnız WARN bandındaki (55–69 °C) sıcaklık
// kritik olmadığı için reset'i bloklamaz.
void test_reset_interlock_critical_temp_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 75;  // ≥70 °C — kritik, FAULT'tan çıkışı engeller
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_warning_temp_does_not_block(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 60;  // WARN bandı — kritik değil, reset serbest
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// DOĞRULANMAMIŞ sinyal (akım) karar mantığına bağlı olmadığı için reset'i
// BLOKLAMAMALI (Ek B güven kuralı).
void test_reset_interlock_unverified_current_does_not_block(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -2000;  // 20 A — sinyal DOĞRULANMADI — karar dışı
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_voltage_low_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 590;  // < 600 dV critical (24S LiFePO4 spec min)
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_voltage_high_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 900;  // > 876 dV critical (spec maks)
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// FAULT içerisinde motor timeout aktifse: state IDLE değil (FAULT), bu yüzden
// hasCriticalCondition true → reset reddedilir.
void test_reset_interlock_motor_timeout_in_fault_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// BMS timeout da motor timeout ile aynı şekilde reset'i bloklar.
void test_reset_interlock_bms_timeout_in_fault_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTimeoutActive = true;
    d.TEL_bmsDataValid = false;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// Sadece warning seviyesindeki bir koşul reset'i bloklamamalı.
void test_reset_interlock_warning_level_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 720;  // warn eşiğinde ama critical değil
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}
