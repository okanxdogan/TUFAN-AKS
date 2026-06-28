#include <unity.h>

#include "BmsAlgo.h"
#include "bms_test_fixtures.h"

using bms_fixtures::makeUniformPack;

// Nominal (3700 mV, 25 °C) => OK.
void test_warning_nominal_is_ok(void) {
    BmsComputed c = computePack(makeUniformPack(3700, 25));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_OK, c.warningLevel);
}

// Bir hücre undervoltage (< 3000 mV) => CRITICAL.
void test_warning_undervoltage_critical(void) {
    BmsPackData p = makeUniformPack(3700, 25);
    p.cellVoltageMv[10] = 2900;  // < BMS_CELL_UNDERVOLT_CRIT_MV (3000)
    BmsComputed c = computePack(p);
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);
}

// Bir hücre overtemp (> 60 °C) => CRITICAL.
void test_warning_overtemp_critical(void) {
    BmsPackData p = makeUniformPack(3700, 25);
    p.cellTempC[4] = 65;  // > BMS_TEMP_OVERTEMP_CRIT_C (60)
    BmsComputed c = computePack(p);
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);
}

// Ara undervoltage (< 3200 ama >= 3000) => WARNING.
void test_warning_undervoltage_warn_band(void) {
    BmsPackData p = makeUniformPack(3700, 25);
    p.cellVoltageMv[0] = 3100;  // < WARN(3200), >= CRIT(3000)
    BmsComputed c = computePack(p);
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

// Ara overtemp (> 50 ama <= 60) => WARNING.
void test_warning_overtemp_warn_band(void) {
    BmsPackData p = makeUniformPack(3700, 25);
    p.cellTempC[0] = 55;  // > WARN(50), <= CRIT(60)
    BmsComputed c = computePack(p);
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

// CRITICAL, WARNING'i ezer: hem warn-band overtemp hem crit undervoltage.
void test_warning_critical_overrides_warning(void) {
    BmsPackData p = makeUniformPack(3700, 25);
    p.cellTempC[0] = 55;          // WARNING bandı
    p.cellVoltageMv[1] = 2950;    // CRITICAL
    BmsComputed c = computePack(p);
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);
}

// isValid=false => güvenli CRITICAL çıktı, sıfır varsayılanlar, denge yok.
void test_invalid_input_safe_critical(void) {
    BmsPackData p = makeUniformPack(3700, 25);
    p.isValid = false;
    BmsComputed c = computePack(p);

    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);
    TEST_ASSERT_EQUAL_UINT8(0, c.socPercent);
    TEST_ASSERT_EQUAL_UINT32(0, c.packVoltageMv);
    TEST_ASSERT_EQUAL_UINT16(0, c.cellDeltaMv);
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        TEST_ASSERT_FALSE(c.balanceFlag[i]);
    }
}
