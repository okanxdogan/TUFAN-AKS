#include <unity.h>

#include <climits>

#include "TelemetrySanitize.h"

// UKS Decode_Line: Parse_Int(f[12], 1, 4). Aralik disi sysState guvenlik
// acisindan FAULT (4) sayilmali; UKS aksi halde tum frame'i reddeder.

void test_sanitize_system_state_valid_passthrough(void) {
    TEST_ASSERT_EQUAL_UINT8(1, TelemetrySanitize::sanitizeSystemState(1));
    TEST_ASSERT_EQUAL_UINT8(2, TelemetrySanitize::sanitizeSystemState(2));
    TEST_ASSERT_EQUAL_UINT8(3, TelemetrySanitize::sanitizeSystemState(3));
    TEST_ASSERT_EQUAL_UINT8(4, TelemetrySanitize::sanitizeSystemState(4));
}

void test_sanitize_system_state_zero_becomes_fault(void) {
    TEST_ASSERT_EQUAL_UINT8(4, TelemetrySanitize::sanitizeSystemState(0));
}

void test_sanitize_system_state_five_becomes_fault(void) {
    TEST_ASSERT_EQUAL_UINT8(4, TelemetrySanitize::sanitizeSystemState(5));
}

// UKS Decode_Line: Parse_Int(f[15], 0, 10000).

void test_sanitize_soc_within_range_passthrough(void) {
    TEST_ASSERT_EQUAL_UINT16(0, TelemetrySanitize::sanitizeSoc(0));
    TEST_ASSERT_EQUAL_UINT16(6283, TelemetrySanitize::sanitizeSoc(6283));
}

void test_sanitize_soc_at_max_passthrough(void) {
    TEST_ASSERT_EQUAL_UINT16(10000, TelemetrySanitize::sanitizeSoc(10000));
}

void test_sanitize_soc_above_max_clamped(void) {
    TEST_ASSERT_EQUAL_UINT16(10000, TelemetrySanitize::sanitizeSoc(10001));
    TEST_ASSERT_EQUAL_UINT16(10000, TelemetrySanitize::sanitizeSoc(65535));
}

// UKS Decode_Line: Parse_Int(f[14], -2147483647, 2147483647) — INT32_MIN
// haric.

void test_sanitize_current_int32_min_shifted(void) {
    TEST_ASSERT_EQUAL_INT32(INT32_MIN + 1,
                            TelemetrySanitize::sanitizeCurrent(INT32_MIN));
}

void test_sanitize_current_int32_min_plus_one_unchanged(void) {
    TEST_ASSERT_EQUAL_INT32(INT32_MIN + 1,
                            TelemetrySanitize::sanitizeCurrent(INT32_MIN + 1));
}

void test_sanitize_current_normal_passthrough(void) {
    TEST_ASSERT_EQUAL_INT32(-181610, TelemetrySanitize::sanitizeCurrent(-181610));
    TEST_ASSERT_EQUAL_INT32(0, TelemetrySanitize::sanitizeCurrent(0));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, TelemetrySanitize::sanitizeCurrent(INT32_MAX));
}
