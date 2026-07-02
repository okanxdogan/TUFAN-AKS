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
    d.TEL_speedKmhX10 = 1413;  // rpmToSpeedKmhX10(1500) ile tutarlı
    return d;
}

}  // namespace

// ---------------------------------------------------------------------------
// begin() çağrılmadan sendStatus(): hiçbir byte UART'a yazılmamalı.
// ---------------------------------------------------------------------------
void test_no_write_before_begin(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.sendStatus(makeZeroData());
    TEST_ASSERT_EQUAL_size_t(0, fake_uart_get_size());
}

// ---------------------------------------------------------------------------
// İlk paket "TEL,2,0," ile başlamalı (versiyon=2, seq=0).
// ---------------------------------------------------------------------------
void test_first_packet_has_v1_seq0_prefix(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,0,"));
}

// ---------------------------------------------------------------------------
// Ardışık 3 çağrı: seq 0,1,2 olarak monoton artmalı.
// ---------------------------------------------------------------------------
void test_sequence_increments(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,0,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,1,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,2,"));
}

// ---------------------------------------------------------------------------
// begin() çağrısı sequence counter'ı 0'a sıfırlamalı.
// ---------------------------------------------------------------------------
void test_begin_resets_sequence(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());  // seq 0 ve 1

    fake_uart_reset();  // önceki içeriği temizle
    tel.begin();        // yeniden başlat
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,0,"));
    TEST_ASSERT_NULL(strstr(buf, "TEL,2,1,"));
}

// ---------------------------------------------------------------------------
// Paket "\r\n" ile sonlanmalı.
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
// Negatif torque işaretle birlikte yazılmalı.
// ---------------------------------------------------------------------------
void test_negative_torque_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_motorTorqueFeedback = -500;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",-500,"));
}

void test_negative_current_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsCurrentCentiMa = -128;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",-128,"));
}

void test_negative_temperature_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsTempHighestC = -20;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",-20,"));
}

// ---------------------------------------------------------------------------
// Boolean alanlar 0/1 olarak render edilmeli.
// ---------------------------------------------------------------------------
void test_motor_valid_renders_as_one(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_motorDataValid = true;
    tel.sendStatus(d);

    // Format: TEL,2,0,rpm,torque,motorErr,motorValid,motorTimeout,...
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "TEL,2,0,0,0,0,1,0,"));
}

void test_motor_timeout_renders_as_one(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_motorTimeoutActive = true;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "TEL,2,0,0,0,0,0,1,"));
}

void test_bms_valid_renders_as_one(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsDataValid = true;
    tel.sendStatus(d);

    // v2: bmsDataValid artık son alan değil; ts_ms=0, spd_x10=0 arkasından gelir.
    // makeZeroData ile bitiş: "...,1,0,0\r\n"
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",1,0,0\r\n"));
}

// ---------------------------------------------------------------------------
// Tüm alanları farklı değerlerle dolduran tam payload kontrolü.
// ---------------------------------------------------------------------------
void test_full_format_with_distinct_values(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeDistinctData());

    const char* buf = fake_uart_get_buffer();
    const char* expected =
        "TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,12345,1413\r\n";
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

// ---------------------------------------------------------------------------
// İki paket aralarında "\r\nTEL," ayırıcısı olmalı.
// ---------------------------------------------------------------------------
void test_two_packets_have_separator(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "\r\nTEL,"));
}

// ---------------------------------------------------------------------------
// ts_ms doğru pozisyon ve değerle encode edilmeli.
// ---------------------------------------------------------------------------
void test_ts_ms_is_encoded(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_timestampMs = 99999;
    tel.sendStatus(d);

    // bmsDataValid=0, ts_ms=99999, spd_x10=0 → ",0,99999,0\r\n"
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",0,99999,0\r\n"));
}

// ---------------------------------------------------------------------------
// spd_x10 doğru pozisyon ve değerle encode edilmeli.
// ---------------------------------------------------------------------------
void test_spd_x10_is_encoded(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_speedKmhX10 = 423;
    tel.sendStatus(d);

    // ts_ms=0, spd_x10=423 → ",0,0,423\r\n"
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",0,0,423\r\n"));
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
// rpmToSpeedKmhX10: uint16_t max rpm — sonuç 65535'i aşmamalı (clamp).
// ---------------------------------------------------------------------------
void test_rpm_to_speed_clamp(void) {
    uint16_t result = rpmToSpeedKmhX10(65535u);
    TEST_ASSERT_TRUE(result <= 65535u);
}
