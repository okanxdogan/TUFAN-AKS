#include <unity.h>

#include "SystemConfig.h"
#include "VcuLogic.h"
#include "../test_native_vcu_logic/fake_freertos.h"
#include "../test_native_vcu_logic/mock_relay_actuator.h"
#include "test_helpers.h"

// ===========================================================================
// RELAY_ROLES_ASSIGNED=1 suite'i ([env:native_roles]) — şartname Bölüm 3:
//   6.e.ii  : sıcaklık uyarısı flaşörü (sesli+ışıklı), histerezisli
//   6.e.iii : 55 uyarı / 70 kapanma, 15 °C sabit aralık
//   8.2.a   : S1 (şarj hattı, kanal 8) / S2 (sürüş hattı, kanal 0) ayrımı
// Varsayılan (bayrak=0) davranış regresyonları test_native_vcu_logic'te
// kalır; bu dosya YALNIZ bayrak=1 iken derlenen mantığı doğrular.
// ===========================================================================

#if !RELAY_ROLES_ASSIGNED
#error "Bu suite yalniz RELAY_ROLES_ASSIGNED=1 ile derlenmeli (env:native_roles)"
#endif

using test_helpers::makeTelemetryDataValid;
using VcuLogic::VcuEvent;
using VcuLogic::VcuState;

namespace {

void primeIdle() {
    VcuLogic::resetForTest();
    fake_freertos_reset();
    fake_relay_reset();
    VcuLogic::init(g_mockRelay);
    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

void setTemp(int8_t tempC) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = tempC;
    VcuLogic::setTelemetryData(d);
}

}  // namespace

// ---------------------------------------------------------------------------
// (a) Sıcaklık sınırları bayrak=1 iken de aynen korunur: 54 → koşul yok,
// 55 → WARN, 69 → WARN, 70 → KRİTİK (FAULT). Saf predicate'ler üzerinden.
// ---------------------------------------------------------------------------
void test_roles_temp_54_no_condition(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 54;
    TEST_ASSERT_FALSE(VcuLogic::hasWarningCondition(d));
    TEST_ASSERT_FALSE(VcuLogic::hasCriticalCondition(d, VcuState::IDLE));
}

void test_roles_temp_55_is_warning(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 55;
    TEST_ASSERT_TRUE(VcuLogic::hasWarningCondition(d));
    TEST_ASSERT_FALSE(VcuLogic::hasCriticalCondition(d, VcuState::IDLE));
}

void test_roles_temp_69_is_warning_only(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 69;
    TEST_ASSERT_TRUE(VcuLogic::hasWarningCondition(d));
    TEST_ASSERT_FALSE(VcuLogic::hasCriticalCondition(d, VcuState::IDLE));
}

void test_roles_temp_70_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 70;
    TEST_ASSERT_TRUE(VcuLogic::hasCriticalCondition(d, VcuState::IDLE));
}

// ---------------------------------------------------------------------------
// (b) Flaşör: 55'te ON — şartname 6.e.ii.
// ---------------------------------------------------------------------------
void test_roles_flasher_on_at_55(void) {
    primeIdle();
    setTemp(55);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
}

// 54'te HÂLÂ ON (histerezis bandı: 53..54 durumu korur).
void test_roles_flasher_stays_on_at_54(void) {
    primeIdle();
    setTemp(55);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);

    setTemp(54);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
}

// 52'de OFF (55 − FLASHER_HYSTERESIS_C = 53'ün altı).
void test_roles_flasher_off_at_52(void) {
    primeIdle();
    setTemp(55);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);

    setTemp(52);
    VcuLogic::run();
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
}

// bmsDataValid=false iken duruma DOKUNULMAZ (bayat veriyle söndürme yok).
void test_roles_flasher_unchanged_when_bms_invalid(void) {
    primeIdle();
    setTemp(55);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_bmsTempHighestC = 0;  // bayat/anlamsız değer — etkisi olmamalı
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
}

// BMS timeout'ta da duruma dokunulmaz (bayat veriyle yakma da yok).
void test_roles_flasher_unchanged_on_bms_timeout(void) {
    primeIdle();
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTimeoutActive = true;
    d.TEL_bmsTempHighestC = 90;  // bayat yüksek değer — flaşörü YAKMAMALI
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
}

// FAULT'ta bank açıkken flaşör ON kalır: 70 °C → FAULT → allOff bank
// maskesini açar (S1+S2+bank), flaşör kanalı maske dışı olduğundan YANIK
// kalır (şartname 6.e.ii + 8.2.a.vi birlikte).
void test_roles_flasher_stays_on_in_fault_with_bank_open(void) {
    primeIdle();
    setTemp(70);
    VcuLogic::run();  // flaşör ON + kritik → FAULT'a geçiş
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);

    VcuLogic::run();  // t=20 ≥ VCU_CONTACTOR_OPEN_DELAY_MS → allOff (bank)
    TEST_ASSERT_TRUE(g_fake_relay_allOff_count > 0);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);   // yanık
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_S1_CHARGE]); // açık
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_S2_DRIVE]);  // açık

    // Periyodik re-assert de (1 sn sonrası silent allOff) flaşörü söndürmez.
    for (int i = 0; i < 50; ++i)
        VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_allOff_silent_count > 0);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
}

// ---------------------------------------------------------------------------
// (c) S1/S2 mod anahtarlaması — şartname 8.2.a.
// ---------------------------------------------------------------------------
// chargerActive iken: S1 kapalı + S2 açık (8.2.a.iii) ve READY reddedilir.
void test_roles_charger_active_closes_s1_and_rejects_ready(void) {
    primeIdle();
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_chargerActive = true;
    VcuLogic::setTelemetryData(d);

    VcuLogic::run();  // handleIdle: S1 kapatılır
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_S1_CHARGE]);
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_S2_DRIVE]);

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();  // READY reddi (isReadyEntryPermitted: chargerActive)
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_S1_CHARGE]);
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_S2_DRIVE]);
}

// Charger bayatlayınca (chargerActive=false) IDLE'da S1 tekrar açılır.
void test_roles_charger_stale_opens_s1_in_idle(void) {
    primeIdle();
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_chargerActive = true;
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_S1_CHARGE]);

    d.TEL_chargerActive = false;
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_S1_CHARGE]);
}

// READY'de tersi: S1 açık + S2 ve sürüş bankı kapalı (8.2.a.vii). allOn
// KULLANILMAZ (allOn S1'i de kapatırdı) — bank-maskeli kapatma yapılır.
void test_roles_ready_closes_drive_bank_keeps_s1_open(void) {
    primeIdle();
    VcuLogic::run();  // ilk IDLE tick'i (S1 açık komutlanır)

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);  // allOn kullanılmadı
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_S1_CHARGE]);  // açık
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_DRIVE_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_TRUE(g_fake_relay_channelState[ch]);  // S2 + bank kapalı
        }
    }
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
}

// FAULT'ta hepsi açık (8.2.a.vi): READY'den kritik koşulla FAULT'a düş,
// allOff bank maskesini (S1 + S2 + sürüş bankı) açar.
void test_roles_fault_opens_s1_s2_and_bank(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 590;  // < 600 dV kritik düşük gerilim
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();  // → FAULT (transition tick)
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
    VcuLogic::run();  // t=20 → allOff

    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_FALSE(g_fake_relay_channelState[ch]);  // hepsi açık
        }
    }
}

// ---------------------------------------------------------------------------
// (d) allOff flaşör kanalını KAPATMAZ — maske üzerinden (mock, RelayManager
// ile aynı bank-maskesi semantiği; gerçek sürücü doğrulaması
// test_roles_relay_mask suite'indedir).
// ---------------------------------------------------------------------------
void test_roles_allOff_does_not_touch_flasher_channel(void) {
    fake_relay_reset();
    g_mockRelay.setRelay(RELAY_CH_FLASHER, true);
    g_mockRelay.allOn();
    g_mockRelay.allOff(false);

    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_FALSE(g_fake_relay_channelState[ch]);
        }
    }
    // Maske sözleşmesi: flaşör dışarıda, S1+S2 içeride (8.2.a.vi / 6.e.ii).
    TEST_ASSERT_EQUAL_HEX16(0x1FF, RELAY_CONTACTOR_BANK_MASK);
    TEST_ASSERT_EQUAL_HEX16(0x0FF, RELAY_DRIVE_BANK_MASK);
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_roles_temp_54_no_condition);
    RUN_TEST(test_roles_temp_55_is_warning);
    RUN_TEST(test_roles_temp_69_is_warning_only);
    RUN_TEST(test_roles_temp_70_is_critical);

    RUN_TEST(test_roles_flasher_on_at_55);
    RUN_TEST(test_roles_flasher_stays_on_at_54);
    RUN_TEST(test_roles_flasher_off_at_52);
    RUN_TEST(test_roles_flasher_unchanged_when_bms_invalid);
    RUN_TEST(test_roles_flasher_unchanged_on_bms_timeout);
    RUN_TEST(test_roles_flasher_stays_on_in_fault_with_bank_open);

    RUN_TEST(test_roles_charger_active_closes_s1_and_rejects_ready);
    RUN_TEST(test_roles_charger_stale_opens_s1_in_idle);
    RUN_TEST(test_roles_ready_closes_drive_bank_keeps_s1_open);
    RUN_TEST(test_roles_fault_opens_s1_s2_and_bank);

    RUN_TEST(test_roles_allOff_does_not_touch_flasher_channel);

    return UNITY_END();
}
