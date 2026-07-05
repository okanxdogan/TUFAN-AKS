#include <unity.h>
#include <cstdlib>
#include <cstring>

#include "BmsNextionPacket.h"
#include "fake_nextion_emit.h"

void test_byte_budget_is_respected(void) {
    fake_nextion_reset();

    BmsPackData raw{};
    raw.isValid = true;
    for (int i = 0; i < BMS_CELL_COUNT; ++i) {
        raw.cellVoltageMv[i] = 3300;
    }

    BmsComputed comp{};
    comp.cellMaxMv = 3300;
    comp.cellMinMv = 3300;
    comp.warningLevel = 0;

    BmsNextionCache cache{};
    
    // Simulate tick 1: forceFullRefresh = true, budget = 90
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache, true, true, 90);

    // Calculate bytes emitted
    size_t totalBytes = 0;
    for (size_t i = 0; i < fake_nextion_command_count(); ++i) {
        totalBytes += strlen(fake_nextion_command_at(i)) + 3; // +3 for the 0xFF 0xFF 0xFF end sequence
    }

    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(90, totalBytes, "Emitted bytes exceeded budget on tick 1");
    TEST_ASSERT_FALSE_MESSAGE(cache.isWarm, "Cache should not be warm after hitting budget");

    // Clear capture and do tick 2
    fake_nextion_reset();
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache, false, false, 90);

    size_t totalBytesTick2 = 0;
    for (size_t i = 0; i < fake_nextion_command_count(); ++i) {
        totalBytesTick2 += strlen(fake_nextion_command_at(i)) + 3;
    }

    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(90, totalBytesTick2, "Emitted bytes exceeded budget on tick 2");
    
    // Exhaust the remaining queue to verify it eventually gets warm
    int iterations = 2; // already did 2
    while (!cache.isWarm && iterations < 20) {
        fake_nextion_reset();
        buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache, false, false, 90);
        
        size_t tickBytes = 0;
        for (size_t i = 0; i < fake_nextion_command_count(); ++i) {
            tickBytes += strlen(fake_nextion_command_at(i)) + 3;
        }
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(90, tickBytes, "Emitted bytes exceeded budget in drain loop");
        iterations++;
    }

    TEST_ASSERT_TRUE_MESSAGE(cache.isWarm, "Cache should become warm after sufficient ticks");
}
