#include <unity.h>

#include <cstdlib>
#include <cstring>

#include "BmsNextionPacket.h"
#include "fake_nextion_emit.h"

void test_valid_to_invalid_data_transition(void) {
    fake_nextion_reset();

    BmsPackData validRaw{};
    validRaw.isValid = true;
    for (int i = 0; i < BMS_CELL_COUNT; ++i) {
        validRaw.cellVoltageMv[i] = 3300;
    }

    BmsComputed validComp{};
    validComp.cellMaxMv = 3300;
    validComp.cellMinMv = 3300;
    validComp.warningLevel = 0;
    for (int i = 0; i < BMS_CELL_COUNT; ++i) {
        validComp.balanceFlag[i] = false;
    }

    BmsNextionCache cache{};
    
    // Tick 1: Valid data, infinite budget to warm up cache fully
    buildBmsNextionCommands(validComp, validRaw, fake_nextion_capture, nullptr, cache, true, true, 2000);
    TEST_ASSERT_TRUE_MESSAGE(cache.isWarm, "Cache should be warm after valid data pass");

    // Clear capture buffer
    fake_nextion_reset();

    // Now transition to invalid data
    BmsPackData invalidRaw{};
    invalidRaw.isValid = false;
    for (int i = 0; i < BMS_CELL_COUNT; ++i) {
        invalidRaw.cellVoltageMv[i] = 65535; // HMI_CELL_VOLTAGE_NO_DATA
    }

    BmsComputed invalidComp{};
    invalidComp.cellMaxMv = 65535;
    invalidComp.cellMinMv = 65535;
    invalidComp.warningLevel = 2; // CRITICAL
    for (int i = 0; i < BMS_CELL_COUNT; ++i) {
        invalidComp.balanceFlag[i] = false;
    }

    // Tick 2: Invalid data, no force refresh
    buildBmsNextionCommands(invalidComp, invalidRaw, fake_nextion_capture, nullptr, cache, false, true, 2000);

    // Assert that we emitted sentinels
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("cell0.val=65535"), "cell0.val=65535 not found");
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("j0.val=0"), "j0.val=0 not found (bars should be cleared)");
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("warn.val=2"), "warn.val=2 not found (warning should be CRITICAL)");
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("cellmax.val=65535"), "cellmax.val=65535 not found");
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("cellmin.val=65535"), "cellmin.val=65535 not found");

    // We shouldn't see bal0.val=0 emitted again if it was already 0 and we didn't force full refresh
    TEST_ASSERT_NULL_MESSAGE(fake_nextion_find("bal0.val="), "bal0.val should not be emitted since it did not change");
}
