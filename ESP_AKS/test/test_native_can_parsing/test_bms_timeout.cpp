#include <unity.h>

#include "CanParse.h"

// isBmsStatusTimedOut: isMotorStatusTimedOut ile birebir aynı mantık.
// Sabit: CAN_BMS_STATUS_TIMEOUT_MS = 500 (SystemConfig.h).
// Native testte pdMS_TO_TICKS identity olduğundan 500 doğrudan kullanılır.
static constexpr TickType_t BMS_TIMEOUT = 500;

void test_bms_timeout_not_seen_yet(void) {
    // hasSeen=false → henüz mesaj görülmedi, timeout sayılmaz.
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(false, true, 1000, 0, BMS_TIMEOUT));
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(false, false, 1000, 0, BMS_TIMEOUT));
}

void test_bms_timeout_already_invalidated(void) {
    // lastValid=false → zaten invalidate edilmiş, tekrar tetiklenmemeli.
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(true, false, 10000, 0, BMS_TIMEOUT));
}

void test_bms_timeout_within_window(void) {
    // 499 ms geçti, eşik aşılmadı → false.
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(true, true, 499, 0, BMS_TIMEOUT));
}

void test_bms_timeout_at_threshold(void) {
    // Tam 500 ms: >= karşılaştırması → true.
    TEST_ASSERT_TRUE(
        CanParse::isBmsStatusTimedOut(true, true, 500, 0, BMS_TIMEOUT));
}

void test_bms_timeout_past_threshold(void) {
    // 500 ms'den fazla → kesinlikle true.
    TEST_ASSERT_TRUE(
        CanParse::isBmsStatusTimedOut(true, true, 1500, 0, BMS_TIMEOUT));
}
