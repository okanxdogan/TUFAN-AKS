#include <unity.h>

#include <climits>
#include <cstring>

#include "Telemetry.h"
#include "TelemetrySanitize.h"
#include "fake_uart.h"

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
// UKS Decode_Line: Parse_Int(f[4]="torque", -32768, 32767). AKS bu alana
// FİİLEN TEL_motorVoltageDeciV (uint16_t, motor voltajı) yazar — "torque"
// adıyla çelişen BİLİNEN semantik uyumsuzluk (bkz. TelemetrySanitize.h,
// Documents/TORQUE_ALAN_KARAR_NOTU.md). Bu clamp yalnızca frame reddini
// engeller, doğru "tork" değeri üretmez.
// ---------------------------------------------------------------------------
void test_sanitize_motor_volt_for_torque_field_within_range_passthrough(void) {
    TEST_ASSERT_EQUAL_UINT16(0, TelemetrySanitize::sanitizeMotorVoltForTorqueField(0));
    TEST_ASSERT_EQUAL_UINT16(245, TelemetrySanitize::sanitizeMotorVoltForTorqueField(245));
}

void test_sanitize_motor_volt_for_torque_field_at_boundary_passthrough(void) {
    TEST_ASSERT_EQUAL_UINT16(
        32767, TelemetrySanitize::sanitizeMotorVoltForTorqueField(32767));
}

void test_sanitize_motor_volt_for_torque_field_above_boundary_clamped(void) {
    TEST_ASSERT_EQUAL_UINT16(
        32767, TelemetrySanitize::sanitizeMotorVoltForTorqueField(32768));
    TEST_ASSERT_EQUAL_UINT16(
        32767, TelemetrySanitize::sanitizeMotorVoltForTorqueField(40000));
}

void test_sanitize_motor_volt_for_torque_field_uint16_max_clamped(void) {
    TEST_ASSERT_EQUAL_UINT16(
        32767, TelemetrySanitize::sanitizeMotorVoltForTorqueField(65535));
}

// ---------------------------------------------------------------------------
// sanitizeForUplink (S4): tek ortak sanitize kapısı — üç alanı da birlikte
// düzeltir, geçerli değerleri değiştirmeden bırakır.
// ---------------------------------------------------------------------------
void test_sanitize_for_uplink_passthrough_when_all_valid(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 2;
    d.TEL_bmsSocHundredths = 6283;
    d.TEL_bmsCurrentCentiA = -181610;
    d.TEL_motorRpm = 1234;  // sanitize kapsamı dışı alan — dokunulmamalı
    d.TEL_motorVoltageDeciV = 245;  // sınır içinde — değişmemeli

    const TelemetryData out = TelemetrySanitize::sanitizeForUplink(d);

    TEST_ASSERT_EQUAL_UINT8(2, out.TEL_bmsSystemState);
    TEST_ASSERT_EQUAL_UINT16(6283, out.TEL_bmsSocHundredths);
    TEST_ASSERT_EQUAL_INT32(-181610, out.TEL_bmsCurrentCentiA);
    TEST_ASSERT_EQUAL_UINT16(1234, out.TEL_motorRpm);
    TEST_ASSERT_EQUAL_UINT16(245, out.TEL_motorVoltageDeciV);
}

// ---------------------------------------------------------------------------
// sanitizeForUplink: TEL_motorVoltageDeciV sözleşmenin int16 üst sınırını
// (32767) aşarsa kırpılmalı — aksi halde UKS Parse_Int tüm frame'i reddeder.
// ---------------------------------------------------------------------------
void test_sanitize_for_uplink_clamps_motor_volt_above_torque_range(void) {
    TelemetryData d = {};
    d.TEL_motorVoltageDeciV = 40000;  // 4000.0 V — int16 sınırını aşar

    const TelemetryData out = TelemetrySanitize::sanitizeForUplink(d);
    TEST_ASSERT_EQUAL_UINT16(32767, out.TEL_motorVoltageDeciV);
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
    d.TEL_bmsCurrentCentiA = INT32_MIN;  // -> INT32_MIN + 1

    const TelemetryData out = TelemetrySanitize::sanitizeForUplink(d);

    TEST_ASSERT_EQUAL_UINT8(4, out.TEL_bmsSystemState);
    TEST_ASSERT_EQUAL_UINT16(10000, out.TEL_bmsSocHundredths);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN + 1, out.TEL_bmsCurrentCentiA);
}

// ---------------------------------------------------------------------------
// Uçtan uca (S4 kapısından geçen) gerçek yol: TEL_motorVoltageDeciV=65535
// ile sanitizeForUplink -> sendStatus çağrıldığında, CSV çıktısındaki 4.
// alan (torque) 32767'yi AŞMAMALI — aksi halde UKS Parse_Int tüm frame'i
// reddederdi. sendStatus'un kendisi kırpma YAPMAZ (bkz. Telemetry.cpp
// yorumu); garanti yalnızca sanitizeForUplink'in her zaman önce
// çağrılmasından gelir (main.cpp / OfflineBuffer replay yolu, bkz.
// test_replay_sanitize_and_seq.cpp).
// ---------------------------------------------------------------------------
void test_sendStatus_output_clamps_motor_volt_field_via_sanitize_gate(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();

    TelemetryData d = {};
    d.TEL_motorVoltageDeciV = 65535;  // uint16_t max — sözleşme int16 sınırını aşar

    tel.sendStatus(TelemetrySanitize::sanitizeForUplink(d));

    // Format: TEL,ver,seq,rpm,<torque/motorVoltDeciV>,... -> "TEL,2,0,0,32767,"
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "TEL,2,0,0,32767,"));
}
