#include <unity.h>

#include "RelayManager.h"
#include "SystemConfig.h"
#include "../test_native_relay/fake_spi.h"

// ===========================================================================
// RELAY_ROLES_ASSIGNED=1 iken GERÇEK RelayManager sürücüsünün bank-maskesi
// davranışı ([env:native_roles]): allOn/allOff yalnız RELAY_CONTACTOR_BANK_
// MASK (0x17B — flaşör 9 + fan 7 + far 2 HARİÇ) kanallarını sürer; flaşör,
// fan ve far kanallarının son yazılan durumu shadow'da korunur ve
// verifyOutputs geri-okuması yeni maskeyle tutarlıdır. Varsayılan (0x3FF)
// davranış regresyonları test_native_relay'de.
// ===========================================================================

#if !RELAY_ROLES_ASSIGNED
#error "Bu suite yalniz RELAY_ROLES_ASSIGNED=1 ile derlenmeli (env:native_roles)"
#endif

namespace {
void primeRelay() {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();
    fake_spi_reset();
}
}  // namespace

// allOff, yanık flaşörü SÖNDÜRMEZ: kanal 9 (OLATB bit1) LOW (aktif) kalır,
// bank kanalları (0-8) HIGH (açık) olur.
void test_allOff_preserves_flasher_channel(void) {
    primeRelay();
    RelayManager::instance().setRelay(RELAY_CH_FLASHER, true);  // flaşör yanık
    RelayManager::instance().allOn();                            // bank kapalı
    fake_spi_reset();

    RelayManager::instance().allOff();

    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_FLASHER));
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(ch));
        }
    }
    // Donanım seviyesi (active-low): state=0x200 → hw=~0x200 → OLATA=0xFF,
    // OLATB=0xFD (bit1=kanal9 LOW: flaşör hâlâ enerjili).
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_get_reg(MCP23S17_OLATA));
    TEST_ASSERT_EQUAL_HEX8(0xFD, fake_spi_get_reg(MCP23S17_OLATB));
}

// allOn, sönük flaşörü/fanı/farı YAKMAZ: yalnız bank kanalları (0,1,3-6,8)
// kapanır — bank dışı kanallar (2 far, 7 fan, 9 flaşör) etkilenmez.
void test_allOn_does_not_energize_flasher(void) {
    primeRelay();

    RelayManager::instance().allOn();

    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_FLASHER));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_HEADLIGHT));
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(ch));
        }
    }
    // state=0x17B → hw=~0x017B=0xFE84 → OLATA=0x84 (bit2 far + bit7 fan HIGH:
    // sönük), OLATB=0xFE (bit1=kanal9 flaşör HIGH: sönük).
    TEST_ASSERT_EQUAL_HEX8(0x84, fake_spi_get_reg(MCP23S17_OLATA));
    TEST_ASSERT_EQUAL_HEX8(0xFE, fake_spi_get_reg(MCP23S17_OLATB));
}

// allOff, yanık fan ve farı SÖNDÜRMEZ (bank DIŞI kanallar): sıcak batarya
// soğutması / sürücü far kontrolü güvenlik açmasıyla kesilmez.
void test_allOff_preserves_fan_and_headlight(void) {
    primeRelay();
    RelayManager::instance().setRelay(RELAY_CH_FAN, true);
    RelayManager::instance().setRelay(RELAY_CH_HEADLIGHT, true);
    RelayManager::instance().allOn();
    fake_spi_reset();

    RelayManager::instance().allOff();

    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_HEADLIGHT));
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(ch));
        }
    }
}

// HV- (HVNEG, kanal 1) S2 (kanal 0) ile BİRLİKTE açılıp kapanır: ikisi de
// sürüş bankı (RELAY_DRIVE_BANK_MASK) üyesi. allOn → ikisi kapalı; allOff →
// ikisi açık.
void test_hvneg_switches_with_s2(void) {
    primeRelay();

    RelayManager::instance().allOn();
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_S2_DRIVE));
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_HVNEG));

    RelayManager::instance().allOff();
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_S2_DRIVE));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_HVNEG));
}

// verifyOutputs, maske dışı (flaşör) kanal dahil TÜM shadow'u geri-okumayla
// karşılaştırır — flaşör yanıkken allOff sonrası doğrulama TUTARLI kalmalı
// (fault latch'lenmemeli).
void test_verify_consistent_with_mask_after_allOff(void) {
    primeRelay();
    RelayManager::instance().setRelay(RELAY_CH_FLASHER, true);
    RelayManager::instance().allOff();
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());

    fake_spi_reset();
    TEST_ASSERT_TRUE(RelayManager::instance().verifyOutputs());
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());
    TEST_ASSERT_EQUAL_size_t(0, fake_spi_write_count());  // yalnız READ
}

// Maske sözleşmesi (derleme sabitleri): flaşör + fan + far dışarıda, S1 + S2 +
// HV- içeride; sürüş bankı = kontaktör bankı − S1.
void test_mask_contract_values(void) {
    TEST_ASSERT_EQUAL_HEX16(0x17B, RELAY_CONTACTOR_BANK_MASK);
    TEST_ASSERT_EQUAL_HEX16(0x07B, RELAY_DRIVE_BANK_MASK);
    TEST_ASSERT_EQUAL_UINT8(0, RELAY_CH_S2_DRIVE);
    TEST_ASSERT_EQUAL_UINT8(1, RELAY_CH_HVNEG);
    TEST_ASSERT_EQUAL_UINT8(2, RELAY_CH_HEADLIGHT);
    TEST_ASSERT_EQUAL_UINT8(7, RELAY_CH_FAN);
    TEST_ASSERT_EQUAL_UINT8(8, RELAY_CH_S1_CHARGE);
    TEST_ASSERT_EQUAL_UINT8(9, RELAY_CH_FLASHER);
    // Bank DIŞI: far, fan, flaşör. Bank İÇİ: S1, S2, HV-.
    TEST_ASSERT_EQUAL_UINT(0, RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_HEADLIGHT));
    TEST_ASSERT_EQUAL_UINT(0, RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_FAN));
    TEST_ASSERT_EQUAL_UINT(0, RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_FLASHER));
    TEST_ASSERT_TRUE(RELAY_DRIVE_BANK_MASK & (1u << RELAY_CH_HVNEG));
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_allOff_preserves_flasher_channel);
    RUN_TEST(test_allOn_does_not_energize_flasher);
    RUN_TEST(test_allOff_preserves_fan_and_headlight);
    RUN_TEST(test_hvneg_switches_with_s2);
    RUN_TEST(test_verify_consistent_with_mask_after_allOff);
    RUN_TEST(test_mask_contract_values);
    return UNITY_END();
}
