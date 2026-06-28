#include <unity.h>

#include "bms_edge_helpers.h"

// Edge case grubu: HÜCRE UÇ DEĞERLERİ (over/under volt, delta, indeks sınırı)
// Sadece BmsModel.h (BmsPackData) + test-local doğrulayıcılara bağlıdır.

using namespace bmsedge;

// --- Tek hücre çok yüksek (4200+ mV) — delta hesabı sınırı ----------------

void test_single_cell_overvolt_delta(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[10] = 4250;  // tek hücre aşırı dolu
    TEST_ASSERT_EQUAL_UINT8(10, maxCellIndex(p));
    TEST_ASSERT_EQUAL_INT32(4250 - 3650, cellDeltaMv(p));  // 600 mV
}

void test_cell_at_absolute_max_4200(void) {
    BmsPackData p = makeUniform(4200, 25, 0, true);  // hepsi tavanda
    // Tüm hücreler eşit => delta 0 olmalı, ama paket toplamı taşma sınırında.
    TEST_ASSERT_EQUAL_INT32(0, cellDeltaMv(p));
    TEST_ASSERT_EQUAL_INT32(kPackSumMaxMv, sumPackVoltageMv(p));  // 100800
}

void test_overvolt_cell_above_plausible(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[0] = 4300;  // makul üst sınırın üstünde
    TEST_ASSERT_TRUE(p.cellVoltageMv[0] > kCellMaxPlausibleMv);
}

// --- Tek hücre çok düşük (2500 mV) — undervoltage --------------------------

void test_single_cell_undervolt_2500(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[3] = 2500;
    TEST_ASSERT_EQUAL_UINT8(3, minCellIndex(p));
    TEST_ASSERT_EQUAL_INT32(3650 - 2500, cellDeltaMv(p));  // 1150 mV
    // 2500 makul alt SINIRINA eşit; "< 2500" kesin küçükten dolayı FALSE.
    TEST_ASSERT_FALSE(hasUndervolt(p));
}

void test_cell_below_undervolt_threshold(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[3] = 2499;  // eşiğin hemen altı
    TEST_ASSERT_TRUE(hasUndervolt(p));
}

// --- İndeks sınırları: en yüksek/en düşük hücre index 0 veya 23'te --------

void test_max_cell_at_index_0(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[0] = 4100;
    TEST_ASSERT_EQUAL_UINT8(0, maxCellIndex(p));
}

void test_max_cell_at_index_23(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[BMS_CELL_COUNT - 1] = 4100;
    TEST_ASSERT_EQUAL_UINT8(23, maxCellIndex(p));
}

void test_min_cell_at_index_0(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[0] = 3000;
    TEST_ASSERT_EQUAL_UINT8(0, minCellIndex(p));
}

void test_min_cell_at_index_23(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellVoltageMv[BMS_CELL_COUNT - 1] = 3000;
    TEST_ASSERT_EQUAL_UINT8(23, minCellIndex(p));
}

void test_uniform_pack_min_max_first_index_tie(void) {
    // Hepsi eşitse hem min hem max ilk indekse (0) düşmeli — kararlı davranış.
    BmsPackData p = makeUniform(3650, 25, 0, true);
    TEST_ASSERT_EQUAL_UINT8(0, maxCellIndex(p));
    TEST_ASSERT_EQUAL_UINT8(0, minCellIndex(p));
    TEST_ASSERT_EQUAL_INT32(0, cellDeltaMv(p));
}
