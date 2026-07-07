#include <unity.h>

#include "RelayManager.h"
#include "SystemConfig.h"
#include "fake_spi.h"

// ===========================================================================
// G3 — MCP23S17 çıkış geri-okuma / doğrulama (verifyOutputs) testleri.
// Fake SPI artık bir register modeli tutar: yazılan OLAT/IODIR geri okunabilir,
// fake_spi_set_reg ile bozularak brown-out/reset senaryosu simüle edilir.
// ===========================================================================

namespace {
void primeRelay() {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();  // OLAT/IODIR register modeline yazılır
    fake_spi_reset();                  // write-log temizlenir (register modeli KALIR)
}
}  // namespace

// ---------------------------------------------------------------------------
// Yazma sonrası geri-okuma uyuşuyor → fault yok, happy path yalnız READ yapar
// (write-log'a hiçbir şey eklenmez).
// ---------------------------------------------------------------------------
void test_verify_readback_match_no_fault(void) {
    primeRelay();
    RelayManager::instance().allOn();   // yazma + otomatik verify (eşleşir)
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());

    fake_spi_reset();
    bool ok = RelayManager::instance().verifyOutputs();
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());
    TEST_ASSERT_EQUAL_size_t(0, fake_spi_write_count());  // sadece read
}

// ---------------------------------------------------------------------------
// OLAT bozulması (brown-out) → uyuşmazlık → re-init + re-assert yazımları
// yapılır ve actuator fault latch'lenir; register modeli iyileşir.
// ---------------------------------------------------------------------------
void test_verify_detects_olat_corruption_reasserts_and_faults(void) {
    primeRelay();
    RelayManager::instance().allOn();   // OLATA=0x00, OLATB=0xFC (s_relayState=0x3FF)
    fake_spi_reset();
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());

    // Brown-out: OLATA çıkışları serbest bırakmış gibi 0xFF'e dönmüş.
    fake_spi_set_reg(MCP23S17_OLATA, 0xFF);

    bool ok = RelayManager::instance().verifyOutputs();
    TEST_ASSERT_FALSE(ok);                                      // uyuşmazlık tespit
    TEST_ASSERT_TRUE(RelayManager::instance().hasActuatorFault());
    // re-init(4) + re-assert(2) = en az 6 register yazımı yapılmış olmalı
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(6, fake_spi_write_count());
    // re-assert sonrası OLATA beklenen değere (0x00) iyileşmiş olmalı
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_get_reg(MCP23S17_OLATA));
    TEST_ASSERT_EQUAL_HEX8(0xFC, fake_spi_get_reg(MCP23S17_OLATB));
}

// ---------------------------------------------------------------------------
// IODIR reset senaryosu: brown-out sonrası MCP23S17 default'u IODIR=0xFF (tüm
// pinler INPUT → röle sürücüleri floating). verifyOutputs bunu yakalamalı ve
// IODIR'i tekrar 0x00 (output) yapmalı.
// ---------------------------------------------------------------------------
void test_verify_detects_iodir_reset_to_input(void) {
    primeRelay();
    RelayManager::instance().allOff();  // güvenli durum, verify OK
    fake_spi_reset();
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());

    fake_spi_set_reg(MCP23S17_IODIRA, 0xFF);  // reset default (input)
    fake_spi_set_reg(MCP23S17_IODIRB, 0xFF);

    bool ok = RelayManager::instance().verifyOutputs();
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(RelayManager::instance().hasActuatorFault());
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_get_reg(MCP23S17_IODIRA));  // output'a geri
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_get_reg(MCP23S17_IODIRB));
}

// ---------------------------------------------------------------------------
// clearActuatorFault fault'u temizler (VcuLogic RESET yolu için).
// ---------------------------------------------------------------------------
void test_clear_actuator_fault(void) {
    primeRelay();
    fake_spi_set_reg(MCP23S17_OLATA, 0x55);  // keyfi yanlış değer
    RelayManager::instance().verifyOutputs();
    TEST_ASSERT_TRUE(RelayManager::instance().hasActuatorFault());

    RelayManager::instance().clearActuatorFault();
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());
}

// ---------------------------------------------------------------------------
// verifyIfDue RELAY_VERIFY_PERIOD_MS'den seyrek çalışır: periyot dolmadan
// bozulmayı yakalamaz, dolunca yakalar.
// ---------------------------------------------------------------------------
void test_verifyIfDue_throttles_to_period(void) {
    primeRelay();
    RelayManager::instance().verifyIfDue(0);  // ilk çağrı periyodu başlatır

    fake_spi_set_reg(MCP23S17_OLATA, 0xAA);  // s_relayState=0 iken beklenen 0xFF
    RelayManager::instance().verifyIfDue(RELAY_VERIFY_PERIOD_MS - 20);
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());  // henüz değil

    RelayManager::instance().verifyIfDue(RELAY_VERIFY_PERIOD_MS);
    TEST_ASSERT_TRUE(RelayManager::instance().hasActuatorFault());   // periyot doldu
}

// ---------------------------------------------------------------------------
// readRegister begin() sonrası doğru register değerlerini döndürür.
// ---------------------------------------------------------------------------
void test_readRegister_returns_written_values(void) {
    primeRelay();  // begin() OLAT=0xFF/0xFF, IODIR=0x00/0x00 yazdı
    uint8_t v = 0xEE;
    TEST_ASSERT_TRUE(RelayManager::instance().readRegister(MCP23S17_OLATA, v));
    TEST_ASSERT_EQUAL_HEX8(0xFF, v);
    TEST_ASSERT_TRUE(RelayManager::instance().readRegister(MCP23S17_IODIRA, v));
    TEST_ASSERT_EQUAL_HEX8(0x00, v);
}
