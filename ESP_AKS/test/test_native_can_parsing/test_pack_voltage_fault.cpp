#include <unity.h>

#include "CanParse.h"

// =========================================================================
// checkPackVoltageFault — saf pack voltajı eşik kontrolü
// Eşikler parametrik; burada 24S LiFePO4 spec değerleri kullanılır
// (SystemConfig.h: CRITICAL_MIN = 600 deciV / CRITICAL_MAX = 876 deciV).
// Semantik: <= min -> UNDERVOLTAGE, >= max -> OVERVOLTAGE (VcuLogic
// hasCriticalCondition ile aynı).
// =========================================================================

namespace {

constexpr uint16_t kCritMinDeciV = 600;  // 60.0 V
constexpr uint16_t kCritMaxDeciV = 876;  // 87.6 V

CanParse::BmsPackVoltageFault check(uint16_t packDeciV) {
    return CanParse::checkPackVoltageFault(packDeciV, kCritMinDeciV,
                                           kCritMaxDeciV);
}

}  // namespace

void test_packv_fault_599_is_undervoltage(void) {
    // 59.9 V — spec min'in altında
    TEST_ASSERT_TRUE(check(599) ==
                     CanParse::BmsPackVoltageFault::UNDERVOLTAGE);
}

void test_packv_fault_600_boundary_is_undervoltage(void) {
    // 60.0 V — eşik dahil (<= semantiği)
    TEST_ASSERT_TRUE(check(600) ==
                     CanParse::BmsPackVoltageFault::UNDERVOLTAGE);
}

void test_packv_fault_790_is_ok(void) {
    // 79.0 V — Oturum 2'de gözlenen gerçek pack voltajı, güvenli bant
    TEST_ASSERT_TRUE(check(790) == CanParse::BmsPackVoltageFault::NONE);
}

void test_packv_fault_601_875_band_is_ok(void) {
    TEST_ASSERT_TRUE(check(601) == CanParse::BmsPackVoltageFault::NONE);
    TEST_ASSERT_TRUE(check(875) == CanParse::BmsPackVoltageFault::NONE);
}

void test_packv_fault_876_boundary_is_overvoltage(void) {
    // 87.6 V — eşik dahil (>= semantiği)
    TEST_ASSERT_TRUE(check(876) ==
                     CanParse::BmsPackVoltageFault::OVERVOLTAGE);
}

void test_packv_fault_877_is_overvoltage(void) {
    // 87.7 V — spec maks'ın üstünde
    TEST_ASSERT_TRUE(check(877) ==
                     CanParse::BmsPackVoltageFault::OVERVOLTAGE);
}
