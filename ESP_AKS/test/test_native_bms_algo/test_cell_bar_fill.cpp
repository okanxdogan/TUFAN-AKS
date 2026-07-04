#include <unity.h>

#include <cstdlib>
#include <cstring>

#include "BmsNextionPacket.h"
#include "fake_nextion_emit.h"

// cellBarFill() BmsNextionPacket.cpp içinde anonymous namespace'te (internal
// linkage) tanımlı — bu test dosyasından doğrudan çağrılamaz. Bunun yerine
// public API (buildBmsNextionCommands) üzerinden ürettiği "j0.val=<fill>"
// komutunu gözlemleyerek dolaylı olarak kilitleniyor. Aralık artık
// BMS_SOC_EMPTY_MV..BMS_SOC_FULL_MV (2500..3650 mV, BmsAlgo.h — tek kaynak).

namespace {
uint8_t j0FillFor(uint16_t cell0Mv) {
    BmsPackData raw{};
    raw.isValid = true;
    raw.cellVoltageMv[0] = cell0Mv;

    BmsComputed comp{};  // bu testte içeriği önemsiz — yalnızca j0 bakılıyor

    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr);

    const char* cmd = fake_nextion_find("j0.val=");
    TEST_ASSERT_NOT_NULL(cmd);
    return static_cast<uint8_t>(atoi(cmd + strlen("j0.val=")));
}
}  // namespace

void test_cell_bar_fill_at_empty_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, j0FillFor(2500));
}

void test_cell_bar_fill_at_full_is_hundred(void) {
    TEST_ASSERT_EQUAL_UINT8(100, j0FillFor(3650));
}

void test_cell_bar_fill_at_midpoint_is_fifty(void) {
    TEST_ASSERT_EQUAL_UINT8(50, j0FillFor(3075));
}
