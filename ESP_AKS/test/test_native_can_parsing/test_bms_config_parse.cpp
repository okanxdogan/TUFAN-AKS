#include <unity.h>

#include "CanParse.h"

// =========================================================================
// Lithium Balance c-BMS — CAN ID 0xE000 (parseLbBmsE000)
// DOĞRULANMIŞ alanlar:
//   byte[2:3] = Pack Voltage, big-endian uint16, raw * 0.1 = V
// HİPOTEZ (UNVERIFIED — scale unknown) alanlar — HAM yazılır:
//   byte[0:1] -> TEL_bmsE000RawCurrent (int16, HIPOTEZ-orta)
//   byte[4:5] -> TEL_bmsE000RawCounter1 (uint16, HIPOTEZ-düşük)
//   byte[6:7] -> TEL_bmsE000RawCounter2 (uint16, HIPOTEZ-düşük; DLC<8 ise 0)
// TEL_bmsCurrentCentiMa'ya YAZILMAZ (ölçek doğrulanmadı).
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

void test_e000_dlc_8_raw_fields_parsed(void) {
    // DLC 8: packV (DOĞRULANDI) + ham hipotez alanları big-endian okunur.
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
    // 0xAABB signed int16 = -21829 (HAM — ölçek uygulanmaz)
    TEST_ASSERT_EQUAL_INT16(-21829, out.TEL_bmsE000RawCurrent);
    TEST_ASSERT_EQUAL_UINT16(0xCCDD, out.TEL_bmsE000RawCounter1);
    TEST_ASSERT_EQUAL_UINT16(0xEEFF, out.TEL_bmsE000RawCounter2);
}

void test_e000_session2_idle_frame(void) {
    // Sniffer Oturum 2 gerçek frame'i: FF FF 03 16 0F 5E 09 71
    // packV = 0x0316 = 790 deciV (79.0 V, DOĞRULANDI)
    // rawCurrent = 0xFFFF = -1 (HIPOTEZ-orta, idle'da sıfıra yakın)
    twai_message_t m{};
    m.identifier = 0x0000E000;
    m.data_length_code = 8;
    m.data[0] = 0xFF;
    m.data[1] = 0xFF;
    m.data[2] = 0x03;
    m.data[3] = 0x16;
    m.data[4] = 0x0F;
    m.data[5] = 0x5E;
    m.data[6] = 0x09;
    m.data[7] = 0x71;
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(790, out.TEL_bmsPackVoltageDeciV);
    TEST_ASSERT_EQUAL_INT16(-1, out.TEL_bmsE000RawCurrent);
    TEST_ASSERT_EQUAL_UINT16(0x0F5E, out.TEL_bmsE000RawCounter1);
    TEST_ASSERT_EQUAL_UINT16(0x0971, out.TEL_bmsE000RawCounter2);
    // Ölçek doğrulanmadığı için TEL_bmsCurrentCentiMa'ya YAZILMAMALI.
    TEST_ASSERT_EQUAL_INT32(0, out.TEL_bmsCurrentCentiMa);
}

void test_e000_session2_end_frame(void) {
    // Sniffer Oturum 2 oturum-sonu frame'i: FF FE 03 16 0F 5B 09 6D
    // packV = 0x0316 = 790 deciV (79.0 V) — decode kuralı aynı oturum içinde
    // ikinci bir örnekle de tutarlı.
    // rawCurrent = 0xFFFE = -2 (HAM — ölçek uygulanmaz)
    twai_message_t m{};
    m.identifier = 0x0000E000;
    m.data_length_code = 8;
    m.data[0] = 0xFF;
    m.data[1] = 0xFE;
    m.data[2] = 0x03;
    m.data[3] = 0x16;
    m.data[4] = 0x0F;
    m.data[5] = 0x5B;
    m.data[6] = 0x09;
    m.data[7] = 0x6D;
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(790, out.TEL_bmsPackVoltageDeciV);
    TEST_ASSERT_EQUAL_INT16(-2, out.TEL_bmsE000RawCurrent);
    TEST_ASSERT_EQUAL_UINT16(0x0F5B, out.TEL_bmsE000RawCounter1);
    TEST_ASSERT_EQUAL_UINT16(0x096D, out.TEL_bmsE000RawCounter2);
}

void test_e000_dlc_2_too_short(void) {
    // DLC = 2: packV byte'ları (byte[2:3]) frame'de yok → false, alan yazılmaz
    twai_message_t m{};
    m.identifier = 0x0000E000;
    m.data_length_code = 2;
    m.data[0] = 0xFF;
    m.data[1] = 0xFF;
    TelemetryData out{};
    TEST_ASSERT_FALSE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_UINT16(0, out.TEL_bmsPackVoltageDeciV);
    TEST_ASSERT_FALSE(out.TEL_bmsDataValid);
}

void test_e000_dlc_6_counter2_deterministic_zero(void) {
    // DLC 6 sözleşmesi korunur: byte[6:7] okunamaz, counter2 = 0 yazılır.
    twai_message_t m = makeE000Msg(6, 0xFF, 0xFE, 0x03, 0x16, 0x0F, 0x5B);
    m.data[6] = 0xDE;  // DLC dışı çöp — okunmamalı
    m.data[7] = 0xAD;
    TelemetryData out{};
    out.TEL_bmsE000RawCounter2 = 0x1234;  // önceden dolu olsa bile 0'a çekilmeli
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_INT16(-2, out.TEL_bmsE000RawCurrent);
    TEST_ASSERT_EQUAL_UINT16(0x0F5B, out.TEL_bmsE000RawCounter1);
    TEST_ASSERT_EQUAL_UINT16(0, out.TEL_bmsE000RawCounter2);
}
