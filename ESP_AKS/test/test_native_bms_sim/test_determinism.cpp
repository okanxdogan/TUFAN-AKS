#include <unity.h>

#include <cstring>

#include "RealCellDataSource.h"
#include "SimCellDataSource.h"

// --- Determinizm ---
// Aynı tohum + aynı senaryo + aynı sayıda read() => birebir aynı çıktı.

void test_same_seed_same_output(void) {
    SimCellDataSource A(SimScenario::NORMAL);
    SimCellDataSource B(SimScenario::NORMAL);
    A.setSeed(0x1234ABCD);
    B.setSeed(0x1234ABCD);

    for (int r = 0; r < 10; ++r) {
        BmsPackData pa{};
        BmsPackData pb{};
        A.read(pa);
        B.read(pb);
        for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
            TEST_ASSERT_EQUAL_UINT16(pa.cellVoltageMv[i], pb.cellVoltageMv[i]);
            TEST_ASSERT_EQUAL_INT16(pa.cellTempC[i], pb.cellTempC[i]);
        }
        TEST_ASSERT_EQUAL_INT32(pa.packCurrentMa, pb.packCurrentMa);
    }
}

void test_reset_replays_same_sequence(void) {
    SimCellDataSource SIM(SimScenario::NORMAL);
    SIM.setSeed(0x0BADF00D);

    BmsPackData first{};
    SIM.read(first);

    // Birkaç okuma daha ilerlet, sonra reset ile başa dön.
    BmsPackData scratch{};
    SIM.read(scratch);
    SIM.read(scratch);

    SIM.reset();
    BmsPackData replay{};
    SIM.read(replay);

    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        TEST_ASSERT_EQUAL_UINT16(first.cellVoltageMv[i], replay.cellVoltageMv[i]);
    }
}

void test_different_seed_changes_output(void) {
    SimCellDataSource A(SimScenario::NORMAL);
    SimCellDataSource B(SimScenario::NORMAL);
    A.setSeed(0x11111111);
    B.setSeed(0x22222222);

    BmsPackData pa{};
    BmsPackData pb{};
    A.read(pa);
    B.read(pb);

    // En az bir hücrede farklılık beklenir (band içinde de olsa).
    bool SIM_anyDiff = false;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (pa.cellVoltageMv[i] != pb.cellVoltageMv[i]) {
            SIM_anyDiff = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(SIM_anyDiff);
}

// --- HAL ayniligi: RealCellDataSource ayni struct'i besler/okur ---
void test_real_source_ingest_roundtrip(void) {
    RealCellDataSource REAL;
    REAL.begin();

    BmsPackData empty{};
    TEST_ASSERT_FALSE(REAL.read(empty));  // ingest yok => taze veri yok

    BmsPackData snap{};
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        snap.cellVoltageMv[i] = static_cast<uint16_t>(3700 + i);
        snap.cellTempC[i] = 28;
    }
    snap.packCurrentMa = -4200;
    snap.isValid = true;
    REAL.ingest(snap);

    BmsPackData out{};
    TEST_ASSERT_TRUE(REAL.read(out));
    TEST_ASSERT_TRUE(out.isValid);
    TEST_ASSERT_EQUAL_UINT16(3700, out.cellVoltageMv[0]);
    TEST_ASSERT_EQUAL_INT32(-4200, out.packCurrentMa);

    REAL.invalidate();
    BmsPackData out2{};
    TEST_ASSERT_FALSE(REAL.read(out2));
}
