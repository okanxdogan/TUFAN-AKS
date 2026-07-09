#include <unity.h>

#include "BmsAlgo.h"

// Sınır semantiği computePack()'ten (BmsAlgo.cpp): strictly < / > — eşik
// değerinin KENDİSİ henüz CRITICAL/WARNING tetiklemez, bir sonraki adım
// tetikler. Bu dosya bu semantiği LiFePO4 sabitleriyle (BmsAlgo.h) kilitler.

namespace {
// Hücre[0]'ı hedef değere, diğer 23 hücreyi güvenli nominal (3200 mV) bandına
// sabitler; böylece hücre[0] açıkça min VEYA max olur ve diğer taraftan
// (undervolt/overvolt) yanlışlıkla tetiklenme riski olmaz. Sıcaklık nominal
// (25°C), WARN(50)/CRIT(60) eşiklerinin altında.
BmsPackData makePackWithCell0At(uint16_t cell0Mv) {
    BmsPackData d{};
    d.isValid = true;
    d.cellVoltageMv[0] = cell0Mv;
    for (uint8_t i = 1; i < BMS_CELL_COUNT; ++i) {
        d.cellVoltageMv[i] = 3200;
        d.cellTempC[i] = 25;
    }
    d.cellTempC[0] = 25;
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
