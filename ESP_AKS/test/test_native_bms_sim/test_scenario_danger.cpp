#include <unity.h>

#include "SimCellDataSource.h"

// --- Senaryo C (Tehlike) ---
// C-1 (undervoltage): bir hücre <= 2800 mV olmalı.
// C-2 (overtemp):     bir hücre >= 70 °C olmalı.

void test_danger_undervoltage_has_low_cell(void) {
    SimCellDataSource SIM(SimScenario::DANGER_UNDERVOLT);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);
    TEST_ASSERT_TRUE(pack.isValid);

    bool SIM_foundLow = false;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (pack.cellVoltageMv[i] <= 2800) {
            SIM_foundLow = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(SIM_foundLow,
                             "En az bir hücre <= 2800 mV olmali");
}

void test_danger_undervoltage_others_not_low(void) {
    // Sadece tek bir hücre çökmeli; geri kalanlar sağlıklı kalmalı.
    SimCellDataSource SIM(SimScenario::DANGER_UNDERVOLT);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);

    int SIM_lowCount = 0;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (pack.cellVoltageMv[i] <= 2800) ++SIM_lowCount;
    }
    TEST_ASSERT_EQUAL_INT(1, SIM_lowCount);
}

void test_danger_overtemp_has_hot_cell(void) {
    SimCellDataSource SIM(SimScenario::DANGER_OVERTEMP);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);
    TEST_ASSERT_TRUE(pack.isValid);

    bool SIM_foundHot = false;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (pack.cellTempC[i] >= 70) {
            SIM_foundHot = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(SIM_foundHot,
                             "En az bir hücre >= 70 °C olmali");
}

void test_danger_overtemp_voltages_remain_normal(void) {
    // Overtemp senaryosunda gerilimler normal bandda kalmalı.
    SimCellDataSource SIM(SimScenario::DANGER_OVERTEMP);
    SIM.begin();
    BmsPackData pack{};
    SIM.read(pack);

    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        TEST_ASSERT_GREATER_OR_EQUAL_UINT16(3000, pack.cellVoltageMv[i]);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(4000, pack.cellVoltageMv[i]);
    }
}
