#include <unity.h>

#include "CanParse.h"

// =========================================================================
// Lithium Balance c-BMS — CAN ID 0xE002..0xE033 (BİLİNMİYOR)
//
// Bu test dosyası:
// 1. Alan anlamı henüz bilinmeyen ID'lerin stub parser'larının derleme ve
//    temel DLC davranışını doğrular.
// 2. GERÇEK CAN sniffer log frame'leriyle E000 ve E001 doğrulama testleri
//    (Oturum 3, 2026-07-09 — zemin gerçeği, bkz. CAN_Message_Table.md).
// =========================================================================

namespace {

twai_message_t makeStubMsg(uint32_t canId, uint8_t dlc) {
    twai_message_t m{};
    m.identifier = canId;
    m.data_length_code = dlc;
    for (uint8_t i = 0; i < 8; i++)
        m.data[i] = 0xA0 + i;  // tanınabilir dummy pattern
    return m;
}

// Gerçek log frame'inden E000 mesajı oluştur
twai_message_t makeRealE000(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                            uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
    twai_message_t m{};
    m.identifier = 0x0000E000;
    m.data_length_code = 8;
    m.data[0] = b0; m.data[1] = b1; m.data[2] = b2; m.data[3] = b3;
    m.data[4] = b4; m.data[5] = b5; m.data[6] = b6; m.data[7] = b7;
    return m;
}

// Gerçek log frame'inden E001 mesajı oluştur
twai_message_t makeRealE001(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                            uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
    twai_message_t m{};
    m.identifier = 0x0000E001;
    m.data_length_code = 8;
    m.data[0] = b0; m.data[1] = b1; m.data[2] = b2; m.data[3] = b3;
    m.data[4] = b4; m.data[5] = b5; m.data[6] = b6; m.data[7] = b7;
    return m;
}

}  // namespace

// =========================================================================
// GERÇEK LOG FRAME TESTLERİ — Oturum 3 (2026-07-09 18:01, zemin gerçeği)
// =========================================================================

// E000: FF F5 03 20 26 7D 26 58
// Beklenen: Current = -1.1 A, PackV = 80.0 V, SoC1 = 98.53%, SoC2 = 98.16%
void test_e000_real_log_frame_session3_sample1(void) {
    twai_message_t m = makeRealE000(0xFF, 0xF5, 0x03, 0x20, 0x26, 0x7D, 0x26, 0x58);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));

    // Current: 0xFFF5 = int16(-11) × 10 = -110 centi-A = -1.10 A
    TEST_ASSERT_EQUAL_INT32(-110, out.TEL_bmsCurrentCentiA);

    // Voltage: 0x0320 = 800 deciV = 80.0 V
    TEST_ASSERT_EQUAL_UINT16(800, out.TEL_bmsPackVoltageDeciV);

    // SoC1: 0x267D = 9853 hundredths = 98.53%
    TEST_ASSERT_EQUAL_UINT16(9853, out.TEL_bmsSocHundredths);

    // SoC2: 0x2658 = 9816 hundredths = 98.16%
    TEST_ASSERT_EQUAL_UINT16(9816, out.TEL_bmsSoc2Hundredths);

    TEST_ASSERT_TRUE(out.TEL_bmsDataValid);
}

// E000: FF F3 03 20 26 7D 26 58 → Current = -1.3 A
void test_e000_real_log_frame_session3_sample2(void) {
    twai_message_t m = makeRealE000(0xFF, 0xF3, 0x03, 0x20, 0x26, 0x7D, 0x26, 0x58);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_INT32(-130, out.TEL_bmsCurrentCentiA);  // -1.3 A
    TEST_ASSERT_EQUAL_UINT16(800, out.TEL_bmsPackVoltageDeciV);
}

// E000: FF F1 03 20 26 7D 26 58 → Current = -1.5 A
void test_e000_real_log_frame_session3_sample3(void) {
    twai_message_t m = makeRealE000(0xFF, 0xF1, 0x03, 0x20, 0x26, 0x7D, 0x26, 0x58);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_INT32(-150, out.TEL_bmsCurrentCentiA);  // -1.5 A
}

// E001: 82 1D 82 35 82 27 19 18 → Temp1=25°C, Temp2=24°C
void test_e001_real_log_frame_session3_sample1(void) {
    twai_message_t m = makeRealE001(0x82, 0x1D, 0x82, 0x35, 0x82, 0x27, 0x19, 0x18);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE001(m, out));

    // byte[6]=0x19=25, byte[7]=0x18=24 → max=25, min=24
    TEST_ASSERT_EQUAL_INT8(25, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_INT8(24, out.TEL_bmsTempLowestC);
}

// E001: temp2 > temp1 durumunu test et (max/min doğruluğu)
void test_e001_max_min_reversed(void) {
    // byte[6]=20°C, byte[7]=30°C → max=30, min=20
    twai_message_t m = makeRealE001(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 20, 30);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE001(m, out));
    TEST_ASSERT_EQUAL_INT8(30, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_INT8(20, out.TEL_bmsTempLowestC);
}

// E001: negatif sıcaklık — signed cast doğrulaması
void test_e001_negative_temps(void) {
    // byte[6]=0xFB=-5°C, byte[7]=0xEC=-20°C → max=-5, min=-20
    twai_message_t m = makeRealE001(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFB, 0xEC);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE001(m, out));
    TEST_ASSERT_EQUAL_INT8(-5, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_INT8(-20, out.TEL_bmsTempLowestC);
}

// E001: byte[0:5] parse EDİLMEMELİ — önceden set edilen packV korunmalı
void test_e001_preserves_other_fields(void) {
    twai_message_t m = makeRealE001(0x82, 0x1D, 0x82, 0x35, 0x82, 0x27, 0x19, 0x18);
    TelemetryData out{};
    out.TEL_bmsPackVoltageDeciV = 800;
    out.TEL_bmsCurrentCentiA = -110;
    out.TEL_bmsSocHundredths = 9853;
    CanParse::parseLbBmsE001(m, out);
    // E001 yalnızca temp yazmalı, E000 alanlarını DEĞİŞTİRMEMELİ
    TEST_ASSERT_EQUAL_UINT16(800, out.TEL_bmsPackVoltageDeciV);
    TEST_ASSERT_EQUAL_INT32(-110, out.TEL_bmsCurrentCentiA);
    TEST_ASSERT_EQUAL_UINT16(9853, out.TEL_bmsSocHundredths);
}

// =========================================================================
// BİLİNMİYOR ID stub testleri — E002-E033
// =========================================================================

void test_e002_stub_accepts_valid_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E002, 6);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE002(m, out));
}

void test_e032_stub_accepts_valid_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E032, 8);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE032(m, out));
}

void test_e033_stub_accepts_valid_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E033, 8);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE033(m, out));
}

// Stub'lar TelemetryData'ya yazmamalı (regression)
void test_stubs_do_not_write_telemetry(void) {
    TelemetryData baseline{};
    baseline.TEL_motorRpm = 500;
    baseline.TEL_bmsPackVoltageDeciV = 780;
    baseline.TEL_bmsCurrentCentiA = -100000;
    baseline.TEL_bmsSocHundredths = 8000;
    baseline.TEL_bmsTempHighestC = 25;
    baseline.TEL_bmsSystemState = 2;
    baseline.TEL_bmsDataValid = false;

    TelemetryData out = baseline;

    twai_message_t m = makeStubMsg(0x0000E003, 8);
    CanParse::parseLbBmsE003(m, out);

    // Hiçbir alan değişmemiş olmalı
    TEST_ASSERT_EQUAL_UINT16(500, out.TEL_motorRpm);
    TEST_ASSERT_EQUAL_UINT16(780, out.TEL_bmsPackVoltageDeciV);
    TEST_ASSERT_EQUAL_INT32(-100000, out.TEL_bmsCurrentCentiA);
    TEST_ASSERT_EQUAL_UINT16(8000, out.TEL_bmsSocHundredths);
    TEST_ASSERT_EQUAL_INT8(25, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_UINT8(2, out.TEL_bmsSystemState);
    TEST_ASSERT_FALSE(out.TEL_bmsDataValid);
}

// NOT: Bu dosyanın kendi main()'i YOK. Tek test runner test_main.cpp'dedir.
