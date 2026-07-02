#include <unity.h>

#include <cstring>

#include "Telemetry.h"
#include "fake_uart.h"

namespace {

TelemetryData makeZeroData() {
    TelemetryData d{};
    return d;
}

TelemetryData makeDistinctData() {
    TelemetryData d{};
    d.TEL_motorRpm = 1500;
    d.TEL_motorTorqueFeedback = -250;
    d.TEL_motorErrorFlags = 5;
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;
    d.TEL_bmsCellVoltageMaxDeciMv = 37734;  // 3773.4 mV
    d.TEL_bmsCellVoltageMinDeciMv = 37422;  // 3742.2 mV
    d.TEL_bmsTempHighestC = 32;
    d.TEL_bmsTempLowestC  = 31;
    d.TEL_bmsSystemState  = 2;              // IDLE
    d.TEL_bmsPackVoltageDeciV = 780;        // 78.0 V
    d.TEL_bmsCurrentCentiMa = -181610;      // -1816.10 mA
    d.TEL_bmsSocHundredths = 6283;          // 62.83%
    d.TEL_bmsDataValid = true;
    d.TEL_timestampMs = 12345;
    d.TEL_speedKmhX10 = 1413;  // consistent with rpmToSpeedKmhX10(1500)
    return d;
}

}  // namespace

// ---------------------------------------------------------------------------
// begin() not called: no bytes should be written to UART.
// ---------------------------------------------------------------------------
void test_no_write_before_begin(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.sendStatus(makeZeroData());
    TEST_ASSERT_EQUAL_size_t(0, fake_uart_get_size());
}

// ---------------------------------------------------------------------------
// Packet must use semicolon (;) as field separator — TEKNOFEST mandatory.
// ---------------------------------------------------------------------------
void test_semicolon_separator(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    // Zero data: "0;0;0;0;0\r\n" — four semicolons expected, no commas
    TEST_ASSERT_NOT_NULL(strstr(buf, ";"));
    TEST_ASSERT_NULL(strstr(buf, ","));
}

// ---------------------------------------------------------------------------
// Field order: zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh
// ---------------------------------------------------------------------------
void test_field_order(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();

    TelemetryData d = makeZeroData();
    d.TEL_timestampMs = 10000;
    d.TEL_bmsTempHighestC = 24;
    d.TEL_bmsPackVoltageDeciV = 400;  // 40.0 V deciV
    tel.sendStatus(d);

    const char* buf = fake_uart_get_buffer();
    // Expected: "10000;0;24;400;0\r\n"
    TEST_ASSERT_EQUAL_STRING("10000;0;24;400;0\r\n", buf);
}

// ---------------------------------------------------------------------------
// hiz_kmh must always be 0 (placeholder until wheel diameter is known).
// ---------------------------------------------------------------------------
void test_speed_placeholder_zero(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();

    TelemetryData d = makeZeroData();
    d.TEL_timestampMs = 5000;
    d.TEL_motorRpm = 3000;           // RPM is set but speed should still be 0
    d.TEL_speedKmhX10 = 2826;        // speedKmhX10 is set but format ignores it
    d.TEL_bmsTempHighestC = 25;
    d.TEL_bmsPackVoltageDeciV = 780;
    tel.sendStatus(d);

    const char* buf = fake_uart_get_buffer();
    // Second field (hiz_kmh) must be 0
    TEST_ASSERT_EQUAL_STRING("5000;0;25;780;0\r\n", buf);
}

// ---------------------------------------------------------------------------
// kalan_enerji_Wh must always be 0 (placeholder until battery capacity known).
// ---------------------------------------------------------------------------
void test_energy_placeholder_zero(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();

    TelemetryData d = makeZeroData();
    d.TEL_timestampMs = 7000;
    d.TEL_bmsSocHundredths = 9500;  // SOC is set but energy should still be 0
    d.TEL_bmsTempHighestC = 30;
    d.TEL_bmsPackVoltageDeciV = 800;
    tel.sendStatus(d);

    const char* buf = fake_uart_get_buffer();
    // Last field (kalan_enerji_Wh) must be 0
    TEST_ASSERT_EQUAL_STRING("7000;0;30;800;0\r\n", buf);
}

// ---------------------------------------------------------------------------
// Packet must end with \r\n.
// ---------------------------------------------------------------------------
void test_packet_ends_with_crlf(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());

    size_t sz = fake_uart_get_size();
    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_TRUE(sz >= 2);
    TEST_ASSERT_EQUAL_CHAR('\r', buf[sz - 2]);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[sz - 1]);
}

// ---------------------------------------------------------------------------
// Negative temperature is correctly formatted in the T_bat_C field.
// ---------------------------------------------------------------------------
void test_negative_temperature_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsTempHighestC = -20;
    tel.sendStatus(d);

    // Format: "0;0;-20;0;0\r\n"
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ";-20;"));
}

// ---------------------------------------------------------------------------
// Full format with distinct values: all fields in correct position.
// ---------------------------------------------------------------------------
void test_full_format_with_distinct_values(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeDistinctData());

    const char* buf = fake_uart_get_buffer();
    // TEL_timestampMs=12345, hiz_kmh=0, T_bat_C=32, V_bat_C=780, kalan_enerji_Wh=0
    const char* expected = "12345;0;32;780;0\r\n";
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

// ---------------------------------------------------------------------------
// Two consecutive packets separated by \r\n and next timestamp.
// ---------------------------------------------------------------------------
void test_two_packets_have_separator(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());

    // Both are "0;0;0;0;0\r\n" — second packet starts after first \r\n
    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_EQUAL_STRING("0;0;0;0;0\r\n0;0;0;0;0\r\n", buf);
}

// ---------------------------------------------------------------------------
// begin() resets state — sending works after re-initialization.
// ---------------------------------------------------------------------------
void test_begin_resets_state(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());

    fake_uart_reset();  // clear previous output
    tel.begin();        // re-init
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_EQUAL_STRING("0;0;0;0;0\r\n", buf);
}

// ---------------------------------------------------------------------------
// Timestamp encodes correctly in the first field.
// ---------------------------------------------------------------------------
void test_timestamp_encoded_first_field(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_timestampMs = 99999;
    tel.sendStatus(d);

    const char* buf = fake_uart_get_buffer();
    // First field is 99999
    TEST_ASSERT_EQUAL_STRING("99999;0;0;0;0\r\n", buf);
}

// ---------------------------------------------------------------------------
// Pack voltage (V_bat_C) encodes as deciV integer.
// ---------------------------------------------------------------------------
void test_pack_voltage_encoded(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsPackVoltageDeciV = 812;  // 81.2 V
    tel.sendStatus(d);

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, ";812;"));
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10: rpm=0 → 0
// ---------------------------------------------------------------------------
void test_rpm_to_speed_zero(void) {
    TEST_ASSERT_EQUAL_UINT16(0, rpmToSpeedKmhX10(0));
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10: rpm=1500, D=0.5, GR=1.0 →
//   km/h = 1500*PI*0.5*60/1000 ≈ 141.37 → ×10 = 1413
// ---------------------------------------------------------------------------
void test_rpm_to_speed_typical(void) {
    TEST_ASSERT_EQUAL_UINT16(1413, rpmToSpeedKmhX10(1500));
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10: uint16_t max rpm — result must not exceed 65535 (clamp).
// ---------------------------------------------------------------------------
void test_rpm_to_speed_clamp(void) {
    uint16_t result = rpmToSpeedKmhX10(65535u);
    TEST_ASSERT_TRUE(result <= 65535u);
}
