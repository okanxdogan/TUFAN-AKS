#include <unity.h>

#include "CanParse.h"

// =========================================================================
// Lithium Balance c-BMS — CAN ID 0xE001..0xE033 (DOĞRULANMADI)
//
// Bu test dosyası, alan anlamı henüz bilinmeyen ID'lerin stub
// parser'larının derleme ve temel DLC davranışını doğrular.
// Solion varsayımına dayanan eski testler SİLİNDİ — yanlış varsayımı
// doğrulayan yeşil testler güvenlik riski oluşturur.
//
// İleride gerçek alan anlamı çözüldüğünde bu dosyaya gerçek parse
// testleri eklenecektir.
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

}  // namespace

// --- E001 stub testleri ---

void test_e001_stub_accepts_valid_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E001, 8);
    TelemetryData out{};
    out.TEL_bmsPackVoltageDeciV = 999;  // önceden set edilmiş değer korunmalı
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE001(m, out));
    // Stub, TelemetryData'ya hiçbir alan YAZMAMALI
    TEST_ASSERT_EQUAL_UINT16(999, out.TEL_bmsPackVoltageDeciV);
}

void test_e001_stub_rejects_zero_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E001, 0);
    TelemetryData out{};
    TEST_ASSERT_FALSE(CanParse::parseLbBmsE001(m, out));
}

// --- E002 stub testleri ---

void test_e002_stub_accepts_valid_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E002, 6);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE002(m, out));
}

// --- E032 stub testleri ---

void test_e032_stub_accepts_valid_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E032, 8);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE032(m, out));
}

// --- E033 stub testleri ---

void test_e033_stub_accepts_valid_dlc(void) {
    twai_message_t m = makeStubMsg(0x0000E033, 8);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE033(m, out));
}

// --- Stub'lar TelemetryData'ya yazmamalı (regression) ---

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
