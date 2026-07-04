#include <unity.h>

#include "CanParse.h"

// =========================================================================
// Charger komut frame'i — CAN ID 0x1806E5F4 (parseCharger1806E5F4)
// BMS -> Charger; AKS yalnızca dinler.
// DOĞRULANMIŞ alanlar (sniffer Oturum 2 + J1939 şarj protokolü):
//   byte[0:1] = şarj voltaj hedefi, big-endian uint16, raw * 0.1 = V
//   byte[2:3] = şarj akım hedefi, big-endian uint16, raw * 0.1 = A
// =========================================================================

namespace {

twai_message_t makeChargerMsg(uint8_t dlc,
                              uint8_t v_hi, uint8_t v_lo,
                              uint8_t i_hi, uint8_t i_lo) {
    twai_message_t m{};
    m.identifier = 0x1806E5F4;
    m.data_length_code = dlc;
    m.data[0] = v_hi;
    m.data[1] = v_lo;
    m.data[2] = i_hi;
    m.data[3] = i_lo;
    return m;
}

}  // namespace

void test_charger_dlc_too_short(void) {
    // DLC < 4 → false, out değiştirilmemeli
    twai_message_t m = makeChargerMsg(3, 0x03, 0x70, 0x03, 0xE8);
    ChargerCommand out{};
    TEST_ASSERT_FALSE(CanParse::parseCharger1806E5F4(m, out));
    TEST_ASSERT_EQUAL_UINT16(0, out.chargeVoltageSetpointDeciV);
    TEST_ASSERT_EQUAL_UINT16(0, out.chargeCurrentSetpointDeciA);
}

void test_charger_session2_frame(void) {
    // Sniffer Oturum 2 gerçek frame'i: 03 70 03 E8 00 00 00 00
    // Vset = 0x0370 = 880 deciV (88.0 V), Iset = 0x03E8 = 1000 deciA (100.0 A)
    twai_message_t m = makeChargerMsg(8, 0x03, 0x70, 0x03, 0xE8);
    ChargerCommand out{};
    TEST_ASSERT_TRUE(CanParse::parseCharger1806E5F4(m, out));
    TEST_ASSERT_EQUAL_UINT16(880, out.chargeVoltageSetpointDeciV);
    TEST_ASSERT_EQUAL_UINT16(1000, out.chargeCurrentSetpointDeciA);
}

void test_charger_big_endian_order(void) {
    // Byte sırası big-endian: MSB önce
    twai_message_t m = makeChargerMsg(4, 0x12, 0x34, 0xAB, 0xCD);
    ChargerCommand out{};
    TEST_ASSERT_TRUE(CanParse::parseCharger1806E5F4(m, out));
    TEST_ASSERT_EQUAL_UINT16(0x1234, out.chargeVoltageSetpointDeciV);
    TEST_ASSERT_EQUAL_UINT16(0xABCD, out.chargeCurrentSetpointDeciA);
}

void test_charger_dlc_4_minimum_ok(void) {
    // DLC tam 4 → geçerli (byte[4:7] gerekmiyor)
    twai_message_t m = makeChargerMsg(4, 0x00, 0x00, 0x00, 0x00);
    ChargerCommand out{};
    TEST_ASSERT_TRUE(CanParse::parseCharger1806E5F4(m, out));
    TEST_ASSERT_EQUAL_UINT16(0, out.chargeVoltageSetpointDeciV);
    TEST_ASSERT_EQUAL_UINT16(0, out.chargeCurrentSetpointDeciA);
}
