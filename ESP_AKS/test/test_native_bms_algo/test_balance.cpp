#include <unity.h>

#include "BmsAlgo.h"

namespace {
// Hücre[0]'ı diğer 23 hücrenin (3200 mV) üzerine tam `deltaMv` kadar
// çıkarır — cellDeltaMv birebir deltaMv olur (LiFePO4 geçişinde de
// BMS_BALANCE_THRESHOLD_MV/TOP_MARGIN_MV DEĞİŞMEDİ, bu test yalnızca
// mevcut davranışın kimyadan bağımsız kaldığını kilitler).
BmsPackData makePackWithDelta(uint16_t deltaMv) {
    BmsPackData d{};
    d.isValid = true;
    constexpr uint16_t baseMv = 3200;
    d.cellVoltageMv[0] = static_cast<uint16_t>(baseMv + deltaMv);
    for (uint8_t i = 1; i < BMS_CELL_COUNT; ++i) {
        d.cellVoltageMv[i] = baseMv;
        d.cellTempC[i] = 25;
    }
    d.cellTempC[0] = 25;
    return d;
}
}  // namespace

// delta == BMS_BALANCE_THRESHOLD_MV (50 mV) -> dengeleme YOK (strictly greater gerekir)
void test_balance_at_threshold_no_balancing(void) {
    BmsComputed c = computePack(makePackWithDelta(50));
    TEST_ASSERT_FALSE(c.balanceFlag[0]);
}

// delta == 51 mV -> dengeleme VAR, en yüksek hücre (index 0) işaretlenir
void test_balance_above_threshold_triggers_balancing(void) {
    BmsComputed c = computePack(makePackWithDelta(51));
    TEST_ASSERT_TRUE(c.balanceFlag[0]);
}
