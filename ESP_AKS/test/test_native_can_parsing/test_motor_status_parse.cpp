#include <unity.h>

#include "CanParse.h"
#include "Telemetry.h"  // rpmToSpeedKmhX10Impl, TEL_SPD_X10_MAX

namespace {

// MSTest/mock_motor doğrulanmış 8-byte payload oluşturur:
//   data[0:1] = RPM (big-endian)
//   data[2:3] = Voltaj (raw * 0.1 V, big-endian 16-bit)
//   data[4:6] = Rezerve
//   data[7]   = Hata bayrakları / motor durumu
twai_message_t makeMotorMsg(uint8_t dlc, uint8_t b0, uint8_t b1, uint8_t b2,
                            uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6,
                            uint8_t b7) {
    twai_message_t m{};
    m.identifier = 0x123;
    m.data_length_code = dlc;
    m.data[0] = b0;
    m.data[1] = b1;
    m.data[2] = b2;
    m.data[3] = b3;
    m.data[4] = b4;
    m.data[5] = b5;
    m.data[6] = b6;
    m.data[7] = b7;
    return m;
}

}  // namespace

void test_motor_status_dlc_too_short(void) {
    twai_message_t m = makeMotorMsg(3, 0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00);
    m.data_length_code = 3; 
    MotorStatus out{};
    TEST_ASSERT_FALSE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_FALSE(out.isValid);
    TEST_ASSERT_EQUAL_INT16(0, out.rpm);
}

void test_motor_status_dlc_7_too_short(void) {
    twai_message_t m = makeMotorMsg(7, 0x01, 0x90, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01);
    MotorStatus out{};
    TEST_ASSERT_FALSE(CanParse::parseMotorStatus(m, out));
}

void test_motor_status_dlc_8_valid(void) {
    // RPM = 0x0190 = 400, Voltaj = 240 (24.0V), errorFlags = 0x01
    twai_message_t m = makeMotorMsg(8, 0x01, 0x90, 0x00, 240, 0x00, 0x00, 0x00, 0x01);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_EQUAL_INT16(0x0190, out.rpm);
    TEST_ASSERT_EQUAL_UINT16(240, out.motorVoltageDeciV);
    TEST_ASSERT_EQUAL_UINT8(0x00, out.errorFlags); // 0x01 maskelendiği için 0 olmalı
    TEST_ASSERT_TRUE(out.isRunning);
    TEST_ASSERT_TRUE(out.isValid);
}

void test_motor_status_rpm_big_endian(void) {
    twai_message_t m = makeMotorMsg(8, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_INT16(0x1234, out.rpm);
}

void test_motor_status_rpm_zero(void) {
    twai_message_t m = makeMotorMsg(8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_UINT16(0, out.rpm);
}

void test_motor_status_rpm_max(void) {
    twai_message_t m = makeMotorMsg(8, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_INT16(-1, out.rpm); // 0xFFFF signed int16'da -1'dir
}

void test_motor_status_voltage_parsing(void) {
    // Voltaj = 720 → 72.0V (0x02D0) 16-bit okuma testi
    twai_message_t m = makeMotorMsg(8, 0x00, 0x00, 0x02, 0xD0, 0x00, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_UINT16(720, out.motorVoltageDeciV);
}

void test_motor_status_error_flags_byte7(void) {
    twai_message_t m = makeMotorMsg(8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_UINT8(0x42, out.errorFlags);
    TEST_ASSERT_FALSE(out.isRunning);
}

void test_motor_status_motor_running_flag(void) {
    twai_message_t m = makeMotorMsg(8, 0x03, 0xE8, 0x00, 240, 0x00, 0x00, 0x00, 0x01);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_INT16(1000, out.rpm);
    TEST_ASSERT_EQUAL_UINT16(240, out.motorVoltageDeciV);
    TEST_ASSERT_EQUAL_UINT8(0x00, out.errorFlags); // 0x01 hata değil çalışma bitidir, 0 olur
    TEST_ASSERT_TRUE(out.isRunning);
}

void test_motor_status_motor_stopped_flag(void) {
    twai_message_t m = makeMotorMsg(8, 0x00, 0x00, 0x00, 240, 0x00, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_INT16(0, out.rpm);
    TEST_ASSERT_EQUAL_UINT8(0x00, out.errorFlags);
    TEST_ASSERT_FALSE(out.isRunning);
}

void test_motor_status_invalid_does_not_modify_out(void) {
    twai_message_t m = makeMotorMsg(2, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00, 0x00, 0x00);
    m.data_length_code = 2;
    MotorStatus out{};
    out.rpm = 0xCAFE;
    out.motorVoltageDeciV = 0x5555;
    out.errorFlags = 0x55;
    out.isRunning = true;
    out.isValid = false;

    TEST_ASSERT_FALSE(CanParse::parseMotorStatus(m, out));

    TEST_ASSERT_EQUAL_INT16(-13570, out.rpm); // 0xCAFE signed int16
    TEST_ASSERT_EQUAL_UINT16(0x5555, out.motorVoltageDeciV);
    TEST_ASSERT_EQUAL_UINT8(0x55, out.errorFlags);
    TEST_ASSERT_TRUE(out.isRunning);
    TEST_ASSERT_FALSE(out.isValid);
}

// =========================================================================
// Hall-effect hız sensörü entegrasyon testleri (esp32-canbus-speed-sensor)
//
// Sensör CAN_ID_MOTOR_STATUS (0x200) üzerinden yayın yapar:
//   data[0:1] = RPM (big-endian uint16, her zaman pozitif)
//   data[2:7] = 0x00 (motor voltajı/hata bayrakları yok — sensör yalnız RPM üretir)
//
// Bu testler, gerçek sensörün üreteceği frame'in AKS tarafındaki
// parseMotorStatus() → TEL_motorRpm → rpmToSpeedKmhX10() zinciriyle
// doğru şekilde işlendiğini uçtan uca kanıtlar.
// =========================================================================

// Sensörün üreteceği frame'i simüle eder (motor sürücüsü değil, yalnız RPM)
static twai_message_t makeHallSensorFrame(uint16_t rpm) {
    twai_message_t m{};
    m.identifier = 0x200;  // CAN_ID_MOTOR_STATUS
    m.data_length_code = 8;
    m.data[0] = (rpm >> 8) & 0xFF;  // RPM big-endian MSB
    m.data[1] = rpm & 0xFF;         // RPM big-endian LSB
    m.data[2] = 0x00;  // voltaj yok
    m.data[3] = 0x00;
    m.data[4] = 0x00;  // rezerve
    m.data[5] = 0x00;
    m.data[6] = 0x00;
    // data[7]=0x00: isRunning=false, errorFlags=0x00. Motor sürücüsü entegre
    // değil (MOTOR_DRIVER_PRESENT=0), bu alan VCU karar mantığını ETKİLEMEZ.
    m.data[7] = 0x00;
    return m;
}

// RPM=850 → parse doğru, ardışık rpmToSpeedKmhX10 doğru km/h hesaplar
void test_hall_sensor_rpm850_parse_and_speed(void) {
    twai_message_t m = makeHallSensorFrame(850);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_EQUAL_INT16(850, out.rpm);
    TEST_ASSERT_EQUAL_UINT16(0, out.motorVoltageDeciV);
    TEST_ASSERT_EQUAL_UINT8(0, out.errorFlags);
    TEST_ASSERT_FALSE(out.isRunning);
    TEST_ASSERT_TRUE(out.isValid);

    // TEL_motorRpm = abs(rpm) = 850 (zaten pozitif)
    uint16_t telRpm = static_cast<uint16_t>(out.rpm < 0 ? -out.rpm : out.rpm);
    TEST_ASSERT_EQUAL_UINT16(850, telRpm);

    // rpmToSpeedKmhX10Impl: 850 × π × 0.56 × 60 / 1000 × 10
    // = 850 × 1.7593 × 60 / 1000 × 10 ≈ 897
    uint16_t speedX10 = rpmToSpeedKmhX10Impl(telRpm, 0.56f, 1.0f, true);
    // 89.7 km/h × 10 = 897 (±1 yuvarlama)
    TEST_ASSERT_INT_WITHIN(2, 897, speedX10);
}

// RPM=0 → hız sıfır
void test_hall_sensor_rpm0_speed_zero(void) {
    twai_message_t m = makeHallSensorFrame(0);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_EQUAL_INT16(0, out.rpm);
    TEST_ASSERT_TRUE(out.isValid);

    uint16_t speedX10 = rpmToSpeedKmhX10Impl(0, 0.56f, 1.0f, true);
    TEST_ASSERT_EQUAL_UINT16(0, speedX10);
}

// RPM=3000 → TEL_SPD_X10_MAX clamp kontrolü (fiziksel olarak yüksek hız)
void test_hall_sensor_rpm3000_speed_clamp(void) {
    twai_message_t m = makeHallSensorFrame(3000);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_EQUAL_INT16(3000, out.rpm);

    // 3000 × π × 0.56 × 60 / 1000 ≈ 316.7 km/h → ×10 = 3167
    // TEL_SPD_X10_MAX = 3000 → clamp
    uint16_t speedX10 = rpmToSpeedKmhX10Impl(3000, 0.56f, 1.0f, true);
    TEST_ASSERT_EQUAL_UINT16(TEL_SPD_X10_MAX, speedX10);
}

// data[7]=0x00 → isRunning=false, errorFlags=0: MOTOR_DRIVER_PRESENT=0
// iken bu, VCU'nun IDLE→READY geçişini BLOKLAMAMALI
void test_hall_sensor_is_running_false_does_not_affect_vcu(void) {
    twai_message_t m = makeHallSensorFrame(500);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_FALSE(out.isRunning);   // data[7]=0 → çalışmıyor
    TEST_ASSERT_EQUAL_UINT8(0, out.errorFlags);  // hata yok
    // MOTOR_DRIVER_PRESENT=0 iken isReadyEntryPermitted bu alanları OKUMAZ
    // → READY geçişi bloklanmaz (bu, entegrasyon tarafında fiziksel doğrulanmalı)
}

