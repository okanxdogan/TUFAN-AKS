#include <unity.h>

#include "BmsAlgo.h"

namespace {
// Tüm 24 hücreyi aynı gerilime sabitler — ortalama = o gerilim, böylece
// socFromAvgMv() doğrudan tek bir girdi değeriyle test edilebilir.
BmsPackData makeUniformPack(uint16_t cellMv) {
    BmsPackData d{};
    d.isValid = true;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        d.cellVoltageMv[i] = cellMv;
        d.cellTempC[i] = 25;  // nominal — WARN(50)/CRIT(60) eşiklerinin altında
    }
    return d;
}
}  // namespace

// BMS_SOC_EMPTY_MV = 2500 (LiFePO4 spec min, 2.50 V) -> %0
void test_soc_at_empty_is_zero_percent(void) {
    BmsComputed c = computePack(makeUniformPack(2500));
    TEST_ASSERT_EQUAL_UINT8(0, c.socPercent);
}

// BMS_SOC_FULL_MV = 3650 (LiFePO4 spec maks, 3.65 V) -> %100
void test_soc_at_full_is_hundred_percent(void) {
    BmsComputed c = computePack(makeUniformPack(3650));
    TEST_ASSERT_EQUAL_UINT8(100, c.socPercent);
}

// Orta nokta: 2500 + (3650-2500)/2 = 3075 mV -> %50
void test_soc_at_midpoint_is_fifty_percent(void) {
    BmsComputed c = computePack(makeUniformPack(3075));
    TEST_ASSERT_EQUAL_UINT8(50, c.socPercent);
}

void test_soc_below_empty_clamps_to_zero(void) {
    BmsComputed c = computePack(makeUniformPack(2400));
    TEST_ASSERT_EQUAL_UINT8(0, c.socPercent);
}

void test_soc_above_full_clamps_to_hundred(void) {
    BmsComputed c = computePack(makeUniformPack(3700));
    TEST_ASSERT_EQUAL_UINT8(100, c.socPercent);
}
