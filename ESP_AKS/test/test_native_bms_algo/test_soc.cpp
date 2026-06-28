#include <unity.h>

#include "BmsAlgo.h"
#include "bms_test_fixtures.h"

using bms_fixtures::makeUniformPack;

// 3000 mV (EMPTY) => %0.
void test_soc_empty_is_zero(void) {
    BmsComputed c = computePack(makeUniformPack(BMS_SOC_EMPTY_MV));
    TEST_ASSERT_EQUAL_UINT8(0, c.socPercent);
}

// 4200 mV (FULL) => %100.
void test_soc_full_is_hundred(void) {
    BmsComputed c = computePack(makeUniformPack(BMS_SOC_FULL_MV));
    TEST_ASSERT_EQUAL_UINT8(100, c.socPercent);
}

// 3600 mV => aralığın tam ortası => %50.
// (3600-3000)*100/(4200-3000) = 600*100/1200 = 50
void test_soc_midpoint_3600mv(void) {
    BmsComputed c = computePack(makeUniformPack(3600));
    TEST_ASSERT_EQUAL_UINT8(50, c.socPercent);
}

// Çeyrek nokta: 3300 mV => (300*100)/1200 = 25%.
void test_soc_quarter_3300mv(void) {
    BmsComputed c = computePack(makeUniformPack(3300));
    TEST_ASSERT_EQUAL_UINT8(25, c.socPercent);
}

// EMPTY altında clamp => %0 (sarmalama yok).
void test_soc_below_empty_clamps_zero(void) {
    BmsComputed c = computePack(makeUniformPack(2500));
    TEST_ASSERT_EQUAL_UINT8(0, c.socPercent);
}

// FULL üstünde clamp => %100.
void test_soc_above_full_clamps_hundred(void) {
    BmsComputed c = computePack(makeUniformPack(4300));
    TEST_ASSERT_EQUAL_UINT8(100, c.socPercent);
}
