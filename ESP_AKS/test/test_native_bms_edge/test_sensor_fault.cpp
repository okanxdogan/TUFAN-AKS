#include <unity.h>

#include "bms_edge_helpers.h"

// Edge case grubu: SENSÖR ARIZASI / GEÇERSİZ VERİ
// Sadece BmsModel.h (BmsPackData) + test-local doğrulayıcılara bağlıdır.

using namespace bmsedge;

// --- Tüm hücreler 0 mV (sensör kopması / boş paket) -----------------------

void test_all_cells_zero_detected(void) {
    BmsPackData p = makeUniform(0, 0, 0, true);
    TEST_ASSERT_TRUE(allCellsZero(p));
    // Toplam gerilim sıfır olmalı (taşma yok, alt sınır).
    TEST_ASSERT_EQUAL_INT32(0, sumPackVoltageMv(p));
    // Delta da 0 — hiçbir hücre diğerinden farklı değil.
    TEST_ASSERT_EQUAL_INT32(0, cellDeltaMv(p));
}

void test_all_cells_zero_but_marked_valid_is_implausible(void) {
    // isValid=true olsa bile, 0 mV paket fiziksel olarak imkansız; tüketici
    // bunu "ölü sensör" olarak ayrı bir sezgiselle yakalayabilmeli.
    BmsPackData p = makeUniform(0, 25, 0, true);
    TEST_ASSERT_TRUE(allCellsZero(p));
    // Undervolt sezgiseli de bunu yakalamalı (0 < 2500).
    TEST_ASSERT_TRUE(hasUndervolt(p));
}

void test_nominal_pack_not_all_zero(void) {
    BmsPackData p = makeNominal();
    TEST_ASSERT_FALSE(allCellsZero(p));
}

// --- isValid=false paket — tüketici güvenli tarafta mı --------------------

void test_invalid_pack_not_consumable(void) {
    BmsPackData p = makeNominal();
    p.isValid = false;
    TEST_ASSERT_FALSE(isConsumable(p));
}

void test_valid_pack_is_consumable(void) {
    BmsPackData p = makeNominal();
    TEST_ASSERT_TRUE(isConsumable(p));
}

void test_invalid_flag_independent_of_content(void) {
    // İçerik mükemmel görünse bile isValid=false ise paket TÜKETİLMEMELİ.
    // (Stale/timeout durumunun sözleşme-tarafı temsili.)
    BmsPackData p = makeUniform(3700, 25, 1000, false);
    TEST_ASSERT_FALSE(isConsumable(p));
    // Yine de ham içerik okunabilir olmalı (struct erişimi bozulmamış).
    TEST_ASSERT_EQUAL_UINT16(3700, p.cellVoltageMv[0]);
}
