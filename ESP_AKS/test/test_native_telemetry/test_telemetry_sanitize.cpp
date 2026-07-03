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

// ---------------------------------------------------------------------------
// sanitizeForUplink (S4): tek ortak sanitize kapısı — üç alanı da birlikte
// düzeltir, geçerli değerleri değiştirmeden bırakır.
// ---------------------------------------------------------------------------
void test_sanitize_for_uplink_passthrough_when_all_valid(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 2;
    d.TEL_bmsSocHundredths = 6283;
    d.TEL_bmsCurrentCentiMa = -181610;
    d.TEL_motorRpm = 1234;  // sanitize kapsamı dışı alan — dokunulmamalı

    const TelemetryData out = TelemetrySanitize::sanitizeForUplink(d);

    TEST_ASSERT_EQUAL_UINT8(2, out.TEL_bmsSystemState);
    TEST_ASSERT_EQUAL_UINT16(6283, out.TEL_bmsSocHundredths);
    TEST_ASSERT_EQUAL_INT32(-181610, out.TEL_bmsCurrentCentiMa);
    TEST_ASSERT_EQUAL_UINT16(1234, out.TEL_motorRpm);
}

// ---------------------------------------------------------------------------
// sanitizeForUplink: aralık dışı sysState (0 veya 7 gibi) FAULT(4)'e
// düzeltilmeli — buffer'a bozuk veri girse bile replay çıktısı UKS'in
// kabul aralığında olur (S4).
// ---------------------------------------------------------------------------
void test_sanitize_for_uplink_corrects_invalid_system_state(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 7;  // aralık dışı

    const TelemetryData out = TelemetrySanitize::sanitizeForUplink(d);
    TEST_ASSERT_EQUAL_UINT8(4, out.TEL_bmsSystemState);
}

// ---------------------------------------------------------------------------
// sanitizeForUplink: soc ve current de aynı anda düzeltilmeli.
// ---------------------------------------------------------------------------
void test_sanitize_for_uplink_corrects_soc_and_current_together(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;             // -> 4
    d.TEL_bmsSocHundredths = 65535;       // -> 10000
    d.TEL_bmsCurrentCentiMa = INT32_MIN;  // -> INT32_MIN + 1

    const TelemetryData out = TelemetrySanitize::sanitizeForUplink(d);

    TEST_ASSERT_EQUAL_UINT8(4, out.TEL_bmsSystemState);
    TEST_ASSERT_EQUAL_UINT16(10000, out.TEL_bmsSocHundredths);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN + 1, out.TEL_bmsCurrentCentiMa);
}
