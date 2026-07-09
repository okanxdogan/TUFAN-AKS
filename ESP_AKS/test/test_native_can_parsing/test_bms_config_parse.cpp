#include <unity.h>

#include "CanParse.h"
#include "VcuLogic.h"  // isCurrentCritical + BMS_*_CENTI_A eşikleri (ölçek round-trip)

// =========================================================================
// Lithium Balance c-BMS — CAN ID 0xE000 ve 0xE001 Doğrulanmış Testleri
// =========================================================================

namespace {

twai_message_t makeE000Msg(uint8_t dlc,
                           uint8_t b0, uint8_t b1,
                           uint8_t pv_hi, uint8_t pv_lo,
                           uint8_t soc_hi, uint8_t soc_lo,
                           uint8_t b6, uint8_t b7) {
    twai_message_t m{};
    m.identifier = 0x0000E000;
    m.data_length_code = dlc;
    m.data[0] = b0;
    m.data[1] = b1;
    m.data[2] = pv_hi;
    m.data[3] = pv_lo;
    m.data[4] = soc_hi;
    m.data[5] = soc_lo;
    if (dlc > 6) m.data[6] = b6;
    if (dlc > 7) m.data[7] = b7;
    return m;
}

twai_message_t makeE001Msg(uint8_t dlc, uint8_t t1, uint8_t t2) {
    twai_message_t m{};
    m.identifier = 0x0000E001;
    m.data_length_code = dlc;
    if (dlc > 6) m.data[6] = t1;
    if (dlc > 7) m.data[7] = t2;
    return m;
}

}  // namespace

void test_e000_dlc_too_short(void) {
    // DLC < 8 → false, hiçbir alan yazılmamalı
    twai_message_t m = makeE000Msg(7, 0x00, 0x00, 0x02, 0x0E, 0x00, 0x00, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_FALSE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_FALSE(out.TEL_bmsDataValid);
}

void test_e000_parsing_nominal(void) {
    // Akım: 0x00 0x0A (10) -> 10 * 0.1A = 1A -> 100 CentiA
    // Voltaj: 0x02 0x0E (526) -> 52.6V -> 526 deciV
    // SoC: 0x27 0x10 (10000) -> 100.00% -> 10000 hundredths
    twai_message_t m = makeE000Msg(8, 0x00, 0x0A, 0x02, 0x0E, 0x27, 0x10, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_INT32(100, out.TEL_bmsCurrentCentiA);
    TEST_ASSERT_EQUAL_UINT16(526, out.TEL_bmsPackVoltageDeciV);
    TEST_ASSERT_EQUAL_UINT16(10000, out.TEL_bmsSocHundredths);
    TEST_ASSERT_TRUE(out.TEL_bmsDataValid);
}

void test_e000_parsing_negative_current(void) {
    // Akım: 0xFF 0xF6 (-10) -> -1A -> -100 CentiA
    twai_message_t m = makeE000Msg(8, 0xFF, 0xF6, 0x02, 0x0E, 0x27, 0x10, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_INT32(-100, out.TEL_bmsCurrentCentiA);
}

// =========================================================================
// G5 ölçek round-trip: parser çıktısı (centi-A) ile SystemConfig eşikleri
// (centi-A) UÇTAN UCA hizalı olmalı. Eşik yorumuna değil, parser'ın gerçek
// ölçeğine bağlı test — 1.0 A kritik eşiği parser çıktısı 100 ile tetiklenir.
// =========================================================================
void test_e000_current_scale_round_trip(void) {
    // raw=10 (0.1A birimli) -> 10 * 0.1A = 1.0 A -> parser 100 centi-A
    twai_message_t mp = makeE000Msg(8, 0x00, 0x0A, 0x02, 0x0E, 0x27, 0x10, 0x00, 0x00);
    TelemetryData outp{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(mp, outp));
    TEST_ASSERT_EQUAL_INT32(100, outp.TEL_bmsCurrentCentiA);  // 1.0 A

    // raw=-10 -> -1.0 A -> parser -100 centi-A (işaretli yol)
    twai_message_t mn = makeE000Msg(8, 0xFF, 0xF6, 0x02, 0x0E, 0x27, 0x10, 0x00, 0x00);
    TelemetryData outn{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(mn, outn));
    TEST_ASSERT_EQUAL_INT32(-100, outn.TEL_bmsCurrentCentiA);  // -1.0 A
}

void test_e000_current_1A_trips_critical_threshold(void) {
    // 1.0 A şarj: raw=10 -> parser 100 centi-A. BMS_CRITICAL_MAX_CHARGE_
    // CURRENT_CENTI_A = 100 olduğundan kritik eşiği GERÇEKTEN tetikler.
    twai_message_t m = makeE000Msg(8, 0x00, 0x0A, 0x02, 0x0E, 0x27, 0x10, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(m, out));
    TEST_ASSERT_EQUAL_INT32(BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A,
                            out.TEL_bmsCurrentCentiA);
    TEST_ASSERT_TRUE(VcuLogic::isCurrentCritical(out.TEL_bmsCurrentCentiA));

    // Kontrast: 0.9 A (raw=9) -> parser 90 centi-A, kritik eşiğin ALTINDA.
    twai_message_t mb = makeE000Msg(8, 0x00, 0x09, 0x02, 0x0E, 0x27, 0x10, 0x00, 0x00);
    TelemetryData outb{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE000(mb, outb));
    TEST_ASSERT_EQUAL_INT32(90, outb.TEL_bmsCurrentCentiA);
    TEST_ASSERT_FALSE(VcuLogic::isCurrentCritical(outb.TEL_bmsCurrentCentiA));
}

void test_e000_preserves_other_fields(void) {
    twai_message_t m = makeE000Msg(8, 0x00, 0x00, 0x03, 0x10, 0x00, 0x00, 0x00, 0x00);
    TelemetryData out{};
    out.TEL_motorRpm = 1234;
    out.TEL_bmsCellVoltageMaxDeciMv = 40000;
    out.TEL_bmsTempHighestC = 25;
    out.TEL_bmsSystemState = 2;

    CanParse::parseLbBmsE000(m, out);

    TEST_ASSERT_EQUAL_UINT16(1234, out.TEL_motorRpm);
    TEST_ASSERT_EQUAL_UINT16(40000, out.TEL_bmsCellVoltageMaxDeciMv);
    TEST_ASSERT_EQUAL_INT8(25, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_UINT8(2, out.TEL_bmsSystemState);
}

void test_e001_dlc_too_short(void) {
    twai_message_t m = makeE001Msg(7, 20, 25);
    TelemetryData out{};
    TEST_ASSERT_FALSE(CanParse::parseLbBmsE001(m, out));
}

void test_e001_parsing_temps(void) {
    // Temp 1 (Highest): 40 (0x28)
    // Temp 2 (Lowest): -5 (0xFB)
    twai_message_t m = makeE001Msg(8, 0x28, 0xFB);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseLbBmsE001(m, out));
    TEST_ASSERT_EQUAL_INT8(40, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_INT8(-5, out.TEL_bmsTempLowestC);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_e000_dlc_too_short);
    RUN_TEST(test_e000_parsing_nominal);
    RUN_TEST(test_e000_parsing_negative_current);
    RUN_TEST(test_e000_current_scale_round_trip);
    RUN_TEST(test_e000_current_1A_trips_critical_threshold);
    RUN_TEST(test_e000_preserves_other_fields);
    RUN_TEST(test_e001_dlc_too_short);
    RUN_TEST(test_e001_parsing_temps);
    return UNITY_END();
}
