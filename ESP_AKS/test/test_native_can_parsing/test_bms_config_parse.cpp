#include <unity.h>

#include "CanParse.h"

// =========================================================================
// Lithium Balance c-BMS — CAN ID 0xE000 (parseLbBmsE000)
// DOĞRULANMIŞ alanlar:
//   byte[2:3] = Pack Voltage, big-endian uint16, raw * 0.1 = V
// DOĞRULANMAMIŞ alanlar:
//   byte[0:1] ve byte[4:5] — bilinmiyor, TelemetryData'ya yazılmıyor
// =========================================================================

namespace {

twai_message_t makeE000Msg(uint8_t dlc,
                           uint8_t b0, uint8_t b1,
                           uint8_t pv_hi, uint8_t pv_lo,
                           uint8_t b4, uint8_t b5) {
    twai_message_t m{};
    m.identifier = 0x0000E000;
    m.data_length_code = dlc;
    m.data[0] = b0;
    m.data[1] = b1;
    m.data[2] = pv_hi;
    m.data[3] = pv_lo;
    m.data[4] = b4;
    m.data[5] = b5;
    return m;
}

}  // namespace

void test_e000_dlc_too_short(void) {
    // DLC < 6 → false, hiçbir alan yazılmamalı
    twai_message_t m = makeE000Msg(5, 0x00, 0x00, 0x02, 0x0E, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_FALSE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_FALSE(out.TEL_bmsDataValid);
}

void test_e000_packv_big_endian(void) {
    // CAN sniffer doğrulaması: byte[2]=0x02, byte[3]=0x0E → 0x020E = 526 → 52.6 V
    twai_message_t m = makeE000Msg(6, 0x00, 0x00, 0x02, 0x0E, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(0x020E, out.TEL_bmsPackVoltageDeciV);
}

void test_e000_packv_zero(void) {
    twai_message_t m = makeE000Msg(6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(0, out.TEL_bmsPackVoltageDeciV);
}

void test_e000_packv_max_uint16(void) {
    // Teorik maksimum: 0xFFFF = 65535 deciV = 6553.5 V (pratikte görülmez ama parser'ın taşmaması test edilir)
    twai_message_t m = makeE000Msg(6, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, out.TEL_bmsPackVoltageDeciV);
}

void test_e000_packv_nominal_78v(void) {
    // 78.0 V = 780 deciV = 0x030C
    twai_message_t m = makeE000Msg(8, 0x12, 0x34, 0x03, 0x0C, 0x56, 0x78);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(780, out.TEL_bmsPackVoltageDeciV);
}

void test_e000_sets_valid_flag(void) {
    twai_message_t m = makeE000Msg(6, 0x00, 0x00, 0x03, 0x0C, 0x00, 0x00);
    TelemetryData out{};
    out.TEL_bmsDataValid = false;
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_TRUE(out.TEL_bmsDataValid);
}

void test_e000_preserves_other_fields(void) {
    // parseLbBmsE000 yalnızca TEL_bmsPackVoltageDeciV ve TEL_bmsDataValid yazar,
    // diğer alanlara dokunmamalı.
    twai_message_t m = makeE000Msg(6, 0x00, 0x00, 0x03, 0x10, 0x00, 0x00);
    TelemetryData out{};
    out.TEL_motorRpm = 1234;
    out.TEL_bmsCurrentCentiMa = -50000;
    out.TEL_bmsSocHundredths = 7500;
    out.TEL_bmsCellVoltageMaxDeciMv = 40000;
    out.TEL_bmsTempHighestC = 25;
    out.TEL_bmsSystemState = 2;

    CanParse::parseLbBmsE000(m, out);

    TEST_ASSERT_EQUAL_UINT16(1234, out.TEL_motorRpm);
    TEST_ASSERT_EQUAL_INT32(-50000, out.TEL_bmsCurrentCentiMa);
    TEST_ASSERT_EQUAL_UINT16(7500, out.TEL_bmsSocHundredths);
    TEST_ASSERT_EQUAL_UINT16(40000, out.TEL_bmsCellVoltageMaxDeciMv);
    TEST_ASSERT_EQUAL_INT8(25, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_UINT8(2, out.TEL_bmsSystemState);
}

void test_e000_dlc_8_extra_bytes_ignored(void) {
    // DLC 8 ile geldiğinde byte[6:7] olsa bile parse yalnızca byte[2:3]'e bakar.
    twai_message_t m{};
    m.identifier = 0x0000E000;
    m.data_length_code = 8;
    m.data[0] = 0xAA;
    m.data[1] = 0xBB;
    m.data[2] = 0x03;
    m.data[3] = 0x0C;
    m.data[4] = 0xCC;
    m.data[5] = 0xDD;
    m.data[6] = 0xEE;
    m.data[7] = 0xFF;
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(780, out.TEL_bmsPackVoltageDeciV);
}
