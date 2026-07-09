#include <unity.h>

#include "CanParse.h"

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
