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
    d.TEL_motorVoltageDeciV = 240;
    d.TEL_motorErrorFlags = 5;
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;
    d.TEL_bmsCellVoltageMaxDeciMv = 37734;  // 3773.4 mV
    d.TEL_bmsCellVoltageMinDeciMv = 37422;  // 3742.2 mV
    d.TEL_bmsTempHighestC = 32;
    d.TEL_bmsTempLowestC  = 31;
    d.TEL_bmsSystemState  = 2;              // IDLE
    d.TEL_bmsPackVoltageDeciV = 780;        // 78.0 V
    d.TEL_bmsCurrentCentiA = -181610;      // ham centi-A (format testi; ölçek önemsiz)
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
// Motor voltajı formata uygun yazılmalı.
// ---------------------------------------------------------------------------
void test_motor_voltage_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_motorVoltageDeciV = 245; // 24.5V
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",245,"));
}

void test_negative_current_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsCurrentCentiA = -128;
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

// NOT: Eski test_negative_rpm_is_formatted SİLİNDİ. TEL_motorRpm işaretsiz
// (uint16_t) ve LoRa/UKS sözleşmesi rpm'i 0..65535 bekler (contract.py);
// negatif render sözleşmeyi ihlal ederdi. Geri yön dönüşü CanManager'da
// mutlak değere çevrilir (bkz. CanManager.cpp TEL_motorRpm ataması).

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
        "TEL,2,0,1500,240,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,12345,1413\r\n";
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
// rpmToSpeedKmhX10: rpm=1500, D=0.56, GR=1.0 →
//   km/h = 1500*PI*0.56*60/1000 ≈ 158.34 → ×10 = 1583
// ---------------------------------------------------------------------------
void test_rpm_to_speed_typical(void) {
    // 1500 rpm × pi × 0.56 × 60/1000 = 158.3 km/h (0.56m/GR=1, 2026-07)
    TEST_ASSERT_EQUAL_UINT16(1583, rpmToSpeedKmhX10(1500));
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10: uint16_t max rpm — sonuç TEL_SPD_X10_MAX'i (UKS sanity
// sınırı) aşmamalı (clamp). Eskiden 65535'e clamp'liyordu; UKS Decode_Line
// spd_x10 için 0..3000 kabul ediyor, üstünü parse_fail ile reddediyor.
// ---------------------------------------------------------------------------
void test_rpm_to_speed_clamp(void) {
    uint16_t result = rpmToSpeedKmhX10(65535u);
    TEST_ASSERT_EQUAL_UINT16(TEL_SPD_X10_MAX, result);
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10: TEL_RPM_MAX (UKS sanity, 20000) civarında da sonuç
// TEL_SPD_X10_MAX'e clamp'lenmeli — placeholder geometriyle (D=0.5, GR=1.0)
// bu rpm'de ham hesap ~18849 çıkar, clamp olmasa UKS tüm paketi reddederdi.
// ---------------------------------------------------------------------------
void test_rpm_to_speed_clamp_at_rpm_20000(void) {
    uint16_t result = rpmToSpeedKmhX10(20000u);
    TEST_ASSERT_EQUAL_UINT16(TEL_SPD_X10_MAX, result);
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10: clamp eşiği sınırı.
// clamp eşiği: 3000/(pi×0.56×60/1000) ≈ 2842.4 rpm
// rpm=2843'te ham hesap (~3001) sınırı geçer ve clamp devreye girmelidir.
// ---------------------------------------------------------------------------
void test_rpm_to_speed_clamp_just_above_threshold_rpm(void) {
    uint16_t result = rpmToSpeedKmhX10(2843u);
    TEST_ASSERT_EQUAL_UINT16(TEL_SPD_X10_MAX, result);
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10: eşiğin hemen altında (rpm=2842) ham hesap ~2999.6 —
// clamp devreye GİRMEMELİ, gerçek (kırpılmamış) değer dönmeli.
// ---------------------------------------------------------------------------
void test_rpm_to_speed_no_clamp_just_below_threshold_rpm(void) {
    uint16_t result = rpmToSpeedKmhX10(2842u);
    TEST_ASSERT_TRUE(result < TEL_SPD_X10_MAX);
}

// ---------------------------------------------------------------------------
// rpmToSpeedKmhX10Impl — el hesabı testleri. Bilinçli olarak
// VehicleParams.h'deki WHEEL_DIAMETER_M/GEAR_RATIO makrolarına DEĞİL,
// testin kendi yerel D/GR sabitlerine bağlıdır: gerçek araç değerleri
// VehicleParams.h'de güncellendiğinde bu testler KIRILMAMALIDIR.
// ---------------------------------------------------------------------------

// D=0.5 m, GR=1.0 (mevcut placeholder ile aynı, ama makro yerine yerel
// literal kullanılıyor), motorRpmIsWheelRpm=false, rpm=1500:
// km/h = 1500*pi*0.5*60/1000 ≈ 141.37 → x10 = 1413 (mevcut clamp testiyle
// aynı beklenen değer — refactor'ün üretim yolunu bozmadığını kanıtlar).
void test_impl_hand_calc_motor_rpm_with_gear_ratio(void) {
    uint16_t result = rpmToSpeedKmhX10Impl(1500, 0.5f, 1.0f, false);
    TEST_ASSERT_EQUAL_UINT16(1413, result);
}

// NOT: Eski test_impl_hand_calc_negative_motor_rpm SİLİNDİ. rpmToSpeedKmhX10Impl
// parametresi uint16_t (işaretsiz) — negatif rpm burada anlamlı ifade edilemez.
// Geri yön dönüşünün büyüklüğe çevrilmesi CanManager katmanında yapılır.

// D=1.0 m, GR=2.0, motorRpmIsWheelRpm=false, rpm=1000:
// wheelRpm = 1000/2.0 = 500 → km/h = 500*pi*1.0*60/1000 ≈ 94.2478 → x10=942.
// GR farklı bir değerle de doğru bölündüğünü kanıtlar (D=0.5/GR=1.0'a
// özel bir davranış olmadığını gösterir).
void test_impl_hand_calc_different_wheel_and_gear_ratio(void) {
    uint16_t result = rpmToSpeedKmhX10Impl(1000u, 1.0f, 2.0f, false);
    TEST_ASSERT_EQUAL_UINT16(942, result);
}

// ---------------------------------------------------------------------------
// MOTOR_RPM_IS_WHEEL_RPM dallanması: motorRpmIsWheelRpm=true iken GEAR_RATIO
// bölmesi TAMAMEN atlanmalı — GR=5.0 verilse bile sonuç GR'den etkilenmez.
// D=0.5, rpm=500: km/h = 500*pi*0.5*60/1000 ≈ 47.1239 → x10=471.
// ---------------------------------------------------------------------------
void test_impl_motor_rpm_is_wheel_rpm_skips_gear_ratio(void) {
    uint16_t withFlagTrue = rpmToSpeedKmhX10Impl(500u, 0.5f, 5.0f, true);
    TEST_ASSERT_EQUAL_UINT16(471, withFlagTrue);
}

// Aynı rpm/D, motorRpmIsWheelRpm=false, GR=5.0 → GR'ye bölünür, sonuç
// yukarıdakinden belirgin şekilde FARKLI olmalı (dallanmanın gerçekten
// etkili olduğunu kanıtlar): wheelRpm=100 → km/h≈9.4248 → x10=94.
void test_impl_motor_rpm_false_applies_gear_ratio(void) {
    uint16_t withFlagFalse = rpmToSpeedKmhX10Impl(500u, 0.5f, 5.0f, false);
    TEST_ASSERT_EQUAL_UINT16(94, withFlagFalse);
}

// ---------------------------------------------------------------------------
// Impl fonksiyonu da TEL_SPD_X10_MAX clamp'ini uygulamalı (sarmalayıcıya
// özel bir davranış değil, çekirdek fonksiyonun kendisinde).
// ---------------------------------------------------------------------------
void test_impl_applies_clamp(void) {
    uint16_t result = rpmToSpeedKmhX10Impl(65535u, 0.5f, 1.0f, false);
    TEST_ASSERT_EQUAL_UINT16(TEL_SPD_X10_MAX, result);
}
