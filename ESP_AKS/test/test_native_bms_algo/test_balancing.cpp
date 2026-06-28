#include <unity.h>

#include "BmsAlgo.h"
#include "bms_test_fixtures.h"

using bms_fixtures::makeUniformPack;

namespace {
// Aktif (true) dengeleme bayraklarını say.
int countBalanceFlags(const BmsComputed& c) {
    int n = 0;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i)
        if (c.balanceFlag[i]) ++n;
    return n;
}
}  // namespace

// 24 eşit hücre => delta 0 => hiç dengeleme bayrağı olmamalı.
void test_balance_uniform_no_flags(void) {
    BmsComputed c = computePack(makeUniformPack(3700));
    TEST_ASSERT_EQUAL_UINT16(0, c.cellDeltaMv);
    TEST_ASSERT_EQUAL_INT(0, countBalanceFlags(c));
}

// Bir hücre +60 mV (delta 60 > 50) => yalnız o hücrede bayrak.
void test_balance_single_high_cell_flagged(void) {
    BmsPackData p = makeUniformPack(3700);
    p.cellVoltageMv[7] = 3760;  // +60 mV
    BmsComputed c = computePack(p);

    TEST_ASSERT_EQUAL_UINT16(60, c.cellDeltaMv);
    TEST_ASSERT_EQUAL_UINT8(7, c.cellMaxIndex);
    TEST_ASSERT_TRUE(c.balanceFlag[7]);
    TEST_ASSERT_EQUAL_INT(1, countBalanceFlags(c));  // sadece 7. hücre
}

// SINIR: delta tam 50 mV => dengeleme YOK (eşik dahil edilmez).
void test_balance_delta_exactly_50_no_flag(void) {
    BmsPackData p = makeUniformPack(3700);
    p.cellVoltageMv[3] = 3750;  // +50 mV => delta == 50
    BmsComputed c = computePack(p);

    TEST_ASSERT_EQUAL_UINT16(50, c.cellDeltaMv);
    TEST_ASSERT_EQUAL_INT(0, countBalanceFlags(c));
}

// SINIR: delta 51 mV => dengeleme VAR (yalnız en yüksek hücre).
void test_balance_delta_51_flag_present(void) {
    BmsPackData p = makeUniformPack(3700);
    p.cellVoltageMv[3] = 3751;  // +51 mV => delta == 51
    BmsComputed c = computePack(p);

    TEST_ASSERT_EQUAL_UINT16(51, c.cellDeltaMv);
    TEST_ASSERT_TRUE(c.balanceFlag[3]);
    TEST_ASSERT_EQUAL_INT(1, countBalanceFlags(c));
}

// Marj içindeki iki "en yüksek" hücre birlikte dengelenir.
// İki hücre 3760, biri 3759 (marj 5 mV içinde) => üçü de bayraklı; diğerleri 3700 değil.
void test_balance_top_margin_groups_cells(void) {
    BmsPackData p = makeUniformPack(3700);
    p.cellVoltageMv[1] = 3760;  // en yüksek
    p.cellVoltageMv[2] = 3759;  // marj içinde (3760-5=3755 >= 3759)
    p.cellVoltageMv[5] = 3756;  // marj içinde
    BmsComputed c = computePack(p);

    TEST_ASSERT_EQUAL_UINT16(60, c.cellDeltaMv);  // 3760 - 3700
    TEST_ASSERT_TRUE(c.balanceFlag[1]);
    TEST_ASSERT_TRUE(c.balanceFlag[2]);
    TEST_ASSERT_TRUE(c.balanceFlag[5]);
    TEST_ASSERT_EQUAL_INT(3, countBalanceFlags(c));
    TEST_ASSERT_FALSE(c.balanceFlag[0]);  // 3700, marj dışında
}
