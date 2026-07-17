#include <unity.h>

#include "BmsAlgo.h"

// Sınır semantiği computePack()'ten (BmsAlgo.cpp): HÜCRE VOLTAJI eşikleri
// strictly < / > — eşik değerinin KENDİSİ henüz CRITICAL/WARNING tetiklemez,
// bir sonraki adım tetikler. SICAKLIK eşikleri ise >= — eşik değeri tetikler
// (VCU katmanı isTempWarning/isTempCritical ile aynı politika: ≥55 UYARI,
// ≥70 KRİTİK). Bu dosya her iki semantiği de sabitlerle (BmsAlgo.h) kilitler.

namespace {
// Hücre[0]'ı hedef değere, diğer 23 hücreyi güvenli nominal (3200 mV) bandına
// sabitler; böylece hücre[0] açıkça min VEYA max olur ve diğer taraftan
// (undervolt/overvolt) yanlışlıkla tetiklenme riski olmaz. Paket sıcaklığı
// nominal (25°C), WARN(55)/CRIT(70) eşiklerinin altında.
BmsPackData makePackWithCell0At(uint16_t cell0Mv) {
    BmsPackData d{};
    d.isValid = true;
    d.cellVoltageMv[0] = cell0Mv;
    for (uint8_t i = 1; i < BMS_CELL_COUNT; ++i) {
        d.cellVoltageMv[i] = 3200;
    }
    d.packTempMaxC = 25;
    d.packTempMinC = 25;
    return d;
}

// Tüm hücreler güvenli nominal gerilimde (3200 mV), PAKET sıcaklığı hedef
// değerde — sıcaklık sınırı izole test edilir. Sıcaklık kaynağı artık
// per-hücre dizi DEĞİL, packTempMaxC/MinC (0xE001 paket sıcaklığı — bkz.
// BmsModel.h); computePack sıcaklık kararını yalnız buradan alır.
BmsPackData makePackWithTemp0At(int16_t temp0C) {
    BmsPackData d{};
    d.isValid = true;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        d.cellVoltageMv[i] = 3200;
    }
    d.packTempMaxC = temp0C;
    d.packTempMinC = 25 < temp0C ? 25 : temp0C;
    return d;
}
}  // namespace

// --- Overvolt: BMS_CELL_OVERVOLT_CRIT_MV = 3650 ---

void test_overvolt_at_crit_threshold_is_warning_not_critical(void) {
    BmsComputed c = computePack(makePackWithCell0At(3650));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

void test_overvolt_above_crit_threshold_is_critical(void) {
    BmsComputed c = computePack(makePackWithCell0At(3651));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);
}

// --- Overvolt WARN: BMS_CELL_OVERVOLT_WARN_MV = 3550 ---

void test_overvolt_at_warn_threshold_is_ok(void) {
    BmsComputed c = computePack(makePackWithCell0At(3550));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_OK, c.warningLevel);
}

void test_overvolt_above_warn_threshold_is_warning(void) {
    BmsComputed c = computePack(makePackWithCell0At(3551));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

// --- Undervolt: BMS_CELL_UNDERVOLT_CRIT_MV = 2500 ---

void test_undervolt_at_crit_threshold_is_warning_not_critical(void) {
    BmsComputed c = computePack(makePackWithCell0At(2500));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

void test_undervolt_below_crit_threshold_is_critical(void) {
    BmsComputed c = computePack(makePackWithCell0At(2499));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);
}

// --- Undervolt WARN: BMS_CELL_UNDERVOLT_WARN_MV = 2800 ---

void test_undervolt_at_warn_threshold_is_ok(void) {
    BmsComputed c = computePack(makePackWithCell0At(2800));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_OK, c.warningLevel);
}

void test_undervolt_below_warn_threshold_is_warning(void) {
    BmsComputed c = computePack(makePackWithCell0At(2799));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

// --- Sıcaklık (>= semantiği): BMS_TEMP_OVERTEMP_WARN_C = 55,
// --- BMS_TEMP_OVERTEMP_CRIT_C = 70 — VCU 55/70 politikasıyla eş anlı ---

void test_overtemp_below_warn_threshold_is_ok(void) {
    BmsComputed c = computePack(makePackWithTemp0At(54));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_OK, c.warningLevel);
}

void test_overtemp_at_warn_threshold_is_warning(void) {
    BmsComputed c = computePack(makePackWithTemp0At(55));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

void test_overtemp_below_crit_threshold_is_warning(void) {
    BmsComputed c = computePack(makePackWithTemp0At(69));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_WARNING, c.warningLevel);
}

void test_overtemp_at_crit_threshold_is_critical(void) {
    BmsComputed c = computePack(makePackWithTemp0At(70));
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);
}
