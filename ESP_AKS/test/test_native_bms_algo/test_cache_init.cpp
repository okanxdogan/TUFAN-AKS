#include <unity.h>

#include <cstdlib>
#include <cstring>

#include "BmsNextionPacket.h"
#include "fake_nextion_emit.h"

void test_cache_init_emits_on_first_nonforced_call(void) {
    fake_nextion_reset();

    BmsPackData raw{};
    raw.isValid = true;
    raw.cellVoltageMv[23] = 2500; // Expected to produce j23.val=0

    BmsComputed comp{};
    comp.balanceFlag[23] = false; // Expected to produce bal23.val=0

    // Fresh cache constructed using default ctor (should initialize all elements to 255)
    BmsNextionCache cache{};
    
    // Call buildBmsNextionCommands WITHOUT forceFullRefresh (forceFullRefresh = false)
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache, false, true, 2000);

    // If cache initialization was broken (e.g. elements 1..23 zero-initialized),
    // and input maps to 0, it wouldn't emit a change for cell 23.
    // However, since we initialized the cache array elements to 255, 
    // it will see a difference between 255 and 0, and emit the command.

    const char* cmd_j = fake_nextion_find("j23.val=0");
    TEST_ASSERT_NOT_NULL(cmd_j);

    const char* cmd_bal = fake_nextion_find("bal23.val=0");
    TEST_ASSERT_NOT_NULL(cmd_bal);
}
