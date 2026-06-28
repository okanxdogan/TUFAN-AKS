#include <unity.h>

#include "SimCellDataSource.h"

// --- Senaryo B (Dengesiz Hücre) ---
// index 6 ~4150 mV, diğer hücreler ~3800 mV; max-min farkı > 50 mV.

void test_unbalanced_cell6_is_high(void) {
    SimCellDataSource SIM(SimScenario::UNBALANCED);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);

    TEST_ASSERT_UINT16_WITHIN(20, 4150, pack.cellVoltageMv[6]);
}

void test_unbalanced_other_cells_nominal(void) {
    SimCellDataSource SIM(SimScenario::UNBALANCED);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);

    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (i == 6) continue;
        TEST_ASSERT_UINT16_WITHIN(20, 3800, pack.cellVoltageMv[i]);
    }
}

void test_unbalanced_spread_exceeds_50mv(void) {
    SimCellDataSource SIM(SimScenario::UNBALANCED);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);

    uint16_t SIM_min = pack.cellVoltageMv[0];
    uint16_t SIM_max = pack.cellVoltageMv[0];
    for (uint8_t i = 1; i < BMS_CELL_COUNT; ++i) {
        if (pack.cellVoltageMv[i] < SIM_min) SIM_min = pack.cellVoltageMv[i];
        if (pack.cellVoltageMv[i] > SIM_max) SIM_max = pack.cellVoltageMv[i];
    }
    TEST_ASSERT_GREATER_THAN_UINT16(50, static_cast<uint16_t>(SIM_max - SIM_min));
}
