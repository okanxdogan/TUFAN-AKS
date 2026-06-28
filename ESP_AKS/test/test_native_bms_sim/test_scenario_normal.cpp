#include <unity.h>

#include "RealCellDataSource.h"
#include "SimCellDataSource.h"

// --- Senaryo A (Normal) ---
// Tüm hücreler 3650 mV ± 50 mV bandında olmalı (3600–3700 mV).

void test_normal_all_cells_in_band(void) {
    SimCellDataSource SIM(SimScenario::NORMAL);
    SIM.begin();
    BmsPackData pack{};
    TEST_ASSERT_TRUE(SIM.read(pack));
    TEST_ASSERT_TRUE(pack.isValid);

    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        TEST_ASSERT_GREATER_OR_EQUAL_UINT16(3600, pack.cellVoltageMv[i]);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(3700, pack.cellVoltageMv[i]);
    }
}

void test_normal_band_holds_over_many_reads(void) {
    // Senaryo deterministik dalgalansa da band'dan çıkmamalı.
    SimCellDataSource SIM(SimScenario::NORMAL);
    SIM.begin();
    for (int r = 0; r < 50; ++r) {
        BmsPackData pack{};
        SIM.read(pack);
        for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
            TEST_ASSERT_GREATER_OR_EQUAL_UINT16(3600, pack.cellVoltageMv[i]);
            TEST_ASSERT_LESS_OR_EQUAL_UINT16(3700, pack.cellVoltageMv[i]);
        }
    }
}

void test_normal_temp_and_current_reasonable(void) {
    SimCellDataSource SIM(SimScenario::NORMAL);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);

    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        // ~30 °C ± birkaç derece
        TEST_ASSERT_GREATER_OR_EQUAL_INT16(25, pack.cellTempC[i]);
        TEST_ASSERT_LESS_OR_EQUAL_INT16(35, pack.cellTempC[i]);
    }
    // Deşarj akımı negatif olmalı.
    TEST_ASSERT_LESS_THAN_INT32(0, pack.packCurrentMa);
}
