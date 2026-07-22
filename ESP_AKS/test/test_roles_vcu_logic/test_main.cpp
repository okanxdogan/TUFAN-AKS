#include <unity.h>

#include "SystemConfig.h"
#include "VcuLogic.h"
#include "HeadlightSwitch.h"  // far fiziksel düğmesi — saf karar mantığı
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

// Far fiziksel düğmesi spy'ı (VcuLogic reader hook'u). g_fakeSwitchLevel run()
// içinden okunan HAM pin seviyesidir. Aktif-düşük (INPUT_PULLUP): açık konum/
// basılı = LOW = HEADLIGHT_SWITCH_ACTIVE_LEVEL (0), kapalı/boşta = HIGH (1).
constexpr int SWITCH_ON = HEADLIGHT_SWITCH_ACTIVE_LEVEL;         // engaged (far açık konumu)
constexpr int SWITCH_OFF = HEADLIGHT_SWITCH_ACTIVE_LEVEL ? 0 : 1; // disengaged
int g_fakeSwitchLevel = SWITCH_OFF;
int fakeSwitchReader() { return g_fakeSwitchLevel; }

void primeIdle() {
    VcuLogic::resetForTest();
    fake_freertos_reset();
    fake_relay_reset();
    VcuLogic::init(g_mockRelay);
    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    // Far düğmesi reader'ını bağla, boot'ta "kapalı" konumdan başlat (üretimde
    // main.cpp gpio_get_level'i bağlar). Testler bunu run()'dan ÖNCE değiştirir.
    g_fakeSwitchLevel = SWITCH_OFF;
    VcuLogic::setHeadlightSwitchReader(fakeSwitchReader);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

void setTemp(int8_t tempC) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = tempC;
    VcuLogic::setTelemetryData(d);
}

void runTicks(int n) {
    for (int i = 0; i < n; ++i)
        VcuLogic::run();
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
    // Maske sözleşmesi: flaşör+fan+far dışarıda, S1+S2+HV- içeride
    // (8.2.a.vi / 6.e.ii / B3 7.a-b / B2 9.19.c).
    TEST_ASSERT_EQUAL_HEX16(0x17B, RELAY_CONTACTOR_BANK_MASK);
    TEST_ASSERT_EQUAL_HEX16(0x07B, RELAY_DRIVE_BANK_MASK);
}

// ---------------------------------------------------------------------------
// (e) Soğutma fanı — şartname B3 7.a-b (flaşörün ikizi, FAN_ON=40 / FAN_OFF=35).
// ---------------------------------------------------------------------------
// 39 °C: boot OFF durumu korunur (ON eşiği 40'ın altı, OFF eşiği 35'in üstü).
void test_roles_fan_stays_off_at_39(void) {
    primeIdle();
    setTemp(39);
    VcuLogic::run();
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_FAN]);
}

// 40 °C: fan ON.
void test_roles_fan_on_at_40(void) {
    primeIdle();
    setTemp(40);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);
}

// 40 → ON, 38 → HÂLÂ ON (histerezis bandı 36..39 durumu korur).
void test_roles_fan_stays_on_at_38(void) {
    primeIdle();
    setTemp(40);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);

    setTemp(38);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);
}

// 40 → ON, 35 → OFF (FAN_OFF_TEMP_C, <= semantiği).
void test_roles_fan_off_at_35(void) {
    primeIdle();
    setTemp(40);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);

    setTemp(35);
    VcuLogic::run();
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_FAN]);
}

// bmsDataValid=false iken fan durumuna DOKUNULMAZ (bayat veriyle durdurma yok).
void test_roles_fan_unchanged_when_bms_invalid(void) {
    primeIdle();
    setTemp(40);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_bmsTempHighestC = 0;  // bayat düşük değer — fanı KAPATMAMALI
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);
}

// FAULT'ta fan + flaşör birlikte ON kalır: 70 °C → ikisi de yanar → kritik →
// FAULT → allOff bank maskesini açar ama fan+flaşör (bank dışı) yanık kalır
// (sıcak batarya soğutması + uyarı kesilmez).
void test_roles_fan_and_flasher_on_in_fault(void) {
    primeIdle();
    setTemp(70);
    VcuLogic::run();  // fan+flaşör ON + kritik → FAULT'a geçiş
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);

    VcuLogic::run();  // t=20 → allOff (bank açılır)
    TEST_ASSERT_TRUE(g_fake_relay_allOff_count > 0);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FAN]);      // yanık
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_FLASHER]);  // yanık
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_S2_DRIVE]); // açık
}

// ---------------------------------------------------------------------------
// (f) Far — şartname B2 9.19.c (FİZİKSEL düğme; BMS'ten bağımsız).
// ---------------------------------------------------------------------------
// --- (f.1) SAF karar mantığı: HeadlightSwitch::update (native, run() yok) ---

// Latching: anahtar açık(engaged) → far ON; kapalı → far OFF. Debounce dolunca
// commit; ara (debounce dolmamış) geçiş yok sayılır alttaki testte.
void test_headlight_latching_follows_switch(void) {
    HeadlightSwitch::State st = {};
    // Boot: anahtar KAPALI konumda → far OFF.
    TEST_ASSERT_FALSE(HeadlightSwitch::update(st, SWITCH_OFF, 0, /*latching*/ true,
                                              HEADLIGHT_DEBOUNCE_MS));
    // Anahtar AÇIK konuma alındı; debounce dolmadan HENÜZ OFF, dolunca ON.
    HeadlightSwitch::update(st, SWITCH_ON, 10, true, HEADLIGHT_DEBOUNCE_MS);
    TEST_ASSERT_TRUE(HeadlightSwitch::update(st, SWITCH_ON,
                                             10 + HEADLIGHT_DEBOUNCE_MS, true,
                                             HEADLIGHT_DEBOUNCE_MS));
    // Anahtar tekrar KAPALI → debounce dolunca far OFF.
    HeadlightSwitch::update(st, SWITCH_OFF, 100, true, HEADLIGHT_DEBOUNCE_MS);
    TEST_ASSERT_FALSE(HeadlightSwitch::update(st, SWITCH_OFF,
                                              100 + HEADLIGHT_DEBOUNCE_MS, true,
                                              HEADLIGHT_DEBOUNCE_MS));
}

// Latching boot senkronu (reset dayanıklılığı): anahtar AÇIK konumdayken boot'ta
// far ANINDA ON (debounce beklemeden) — ESP reset sonrası desenkronizasyon yok.
void test_headlight_latching_boot_syncs_to_switch_on(void) {
    HeadlightSwitch::State st = {};
    TEST_ASSERT_TRUE(HeadlightSwitch::update(st, SWITCH_ON, 0, /*latching*/ true,
                                             HEADLIGHT_DEBOUNCE_MS));
}

// Ara (debounce dolmamış) geçişler yok sayılır: kısa bir sıçrama far'ı değiştirmez.
void test_headlight_latching_intermediate_ignored(void) {
    HeadlightSwitch::State st = {};
    HeadlightSwitch::update(st, SWITCH_OFF, 0, true, HEADLIGHT_DEBOUNCE_MS);  // boot OFF
    // Anahtar AÇIK'a sıçrar (aday) ama debounce dolmadan geri KAPALI'ya döner.
    HeadlightSwitch::update(st, SWITCH_ON, 10, true, HEADLIGHT_DEBOUNCE_MS);
    TEST_ASSERT_FALSE(HeadlightSwitch::update(st, SWITCH_OFF, 30, true,
                                              HEADLIGHT_DEBOUNCE_MS));  // t=30 < 10+40
    // Uzun süre sonra bile OFF (sıçrama commit edilmedi).
    TEST_ASSERT_FALSE(HeadlightSwitch::update(st, SWITCH_OFF, 500, true,
                                              HEADLIGHT_DEBOUNCE_MS));
}

// Momentary: basma kenarında (open→closed) toggle; boot OFF; bırakma toggle etmez.
void test_headlight_momentary_toggle_on_press_edge(void) {
    HeadlightSwitch::State st = {};
    // Boot: momentary → OFF (anahtar konumundan bağımsız).
    TEST_ASSERT_FALSE(HeadlightSwitch::update(st, SWITCH_OFF, 0, /*latching*/ false,
                                              HEADLIGHT_DEBOUNCE_MS));
    // Basma → debounce dolunca toggle → ON.
    HeadlightSwitch::update(st, SWITCH_ON, 10, false, HEADLIGHT_DEBOUNCE_MS);
    TEST_ASSERT_TRUE(HeadlightSwitch::update(st, SWITCH_ON,
                                             10 + HEADLIGHT_DEBOUNCE_MS, false,
                                             HEADLIGHT_DEBOUNCE_MS));
    // Bırakma (closed→open) toggle ETMEZ → ON kalır.
    HeadlightSwitch::update(st, SWITCH_OFF, 100, false, HEADLIGHT_DEBOUNCE_MS);
    TEST_ASSERT_TRUE(HeadlightSwitch::update(st, SWITCH_OFF,
                                             100 + HEADLIGHT_DEBOUNCE_MS, false,
                                             HEADLIGHT_DEBOUNCE_MS));
    // İkinci basma → toggle → OFF.
    HeadlightSwitch::update(st, SWITCH_ON, 200, false, HEADLIGHT_DEBOUNCE_MS);
    TEST_ASSERT_FALSE(HeadlightSwitch::update(st, SWITCH_ON,
                                              200 + HEADLIGHT_DEBOUNCE_MS, false,
                                              HEADLIGHT_DEBOUNCE_MS));
}

// Momentary: basılı TUTMAK tekrar toggle ETMEZ.
void test_headlight_momentary_hold_no_retoggle(void) {
    HeadlightSwitch::State st = {};
    HeadlightSwitch::update(st, SWITCH_OFF, 0, false, HEADLIGHT_DEBOUNCE_MS);  // boot OFF
    HeadlightSwitch::update(st, SWITCH_ON, 10, false, HEADLIGHT_DEBOUNCE_MS);
    TEST_ASSERT_TRUE(HeadlightSwitch::update(st, SWITCH_ON, 60, false,
                                             HEADLIGHT_DEBOUNCE_MS));  // basma → ON
    // Uzun süre basılı tut → ON kalır (kenar yok, tekrar toggle yok).
    TEST_ASSERT_TRUE(HeadlightSwitch::update(st, SWITCH_ON, 500, false,
                                             HEADLIGHT_DEBOUNCE_MS));
    TEST_ASSERT_TRUE(HeadlightSwitch::update(st, SWITCH_ON, 1000, false,
                                             HEADLIGHT_DEBOUNCE_MS));
}

// --- (f.2) Entegrasyon: run() düğmeyi okur ve far rölesini sürer (latching) ---

// Fiziksel düğme run()'dan okunur ve far kanalını (bank DIŞI) sürer.
void test_roles_headlight_switch_drives_relay(void) {
    g_fakeSwitchLevel = SWITCH_OFF;
    primeIdle();
    runTicks(1);  // init tick — boot OFF
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);

    g_fakeSwitchLevel = SWITCH_ON;
    runTicks(5);  // debounce dolar → ON
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);

    g_fakeSwitchLevel = SWITCH_OFF;
    runTicks(5);  // debounce dolar → OFF
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);
}

// FAULT'ta far KORUNUR: far ON iken FAULT'a düş, allOff far'ı söndürmez.
void test_roles_headlight_preserved_in_fault(void) {
    primeIdle();
    g_fakeSwitchLevel = SWITCH_ON;   // anahtar açık → boot'ta far ON (latching)
    runTicks(3);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);

    setTemp(70);
    VcuLogic::run();  // → FAULT
    VcuLogic::run();  // t=20 → allOff (bank)
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_TRUE(g_fake_relay_allOff_count > 0);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);  // korunur
}

// READY girişi far'ı DEĞİŞTİRMEZ: far ON iken START→READY (sürüş bankı
// kapatılır), far bank dışı olduğundan yanık kalır.
void test_roles_headlight_unchanged_on_ready_entry(void) {
    primeIdle();
    g_fakeSwitchLevel = SWITCH_ON;
    runTicks(3);  // far ON, hâlâ IDLE
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);  // değişmez
}

// BMS bayatken far düğmesi YİNE çalışır (BMS verisinden bağımsız).
void test_roles_headlight_works_when_bms_stale(void) {
    g_fakeSwitchLevel = SWITCH_OFF;
    primeIdle();
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_bmsTimeoutActive = true;
    VcuLogic::setTelemetryData(d);
    runTicks(1);  // init far OFF
    TEST_ASSERT_FALSE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);

    g_fakeSwitchLevel = SWITCH_ON;
    runTicks(5);
    TEST_ASSERT_TRUE(g_fake_relay_channelState[RELAY_CH_HEADLIGHT]);  // bayat BMS'e rağmen
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

    RUN_TEST(test_roles_fan_stays_off_at_39);
    RUN_TEST(test_roles_fan_on_at_40);
    RUN_TEST(test_roles_fan_stays_on_at_38);
    RUN_TEST(test_roles_fan_off_at_35);
    RUN_TEST(test_roles_fan_unchanged_when_bms_invalid);
    RUN_TEST(test_roles_fan_and_flasher_on_in_fault);

    RUN_TEST(test_headlight_latching_follows_switch);
    RUN_TEST(test_headlight_latching_boot_syncs_to_switch_on);
    RUN_TEST(test_headlight_latching_intermediate_ignored);
    RUN_TEST(test_headlight_momentary_toggle_on_press_edge);
    RUN_TEST(test_headlight_momentary_hold_no_retoggle);
    RUN_TEST(test_roles_headlight_switch_drives_relay);
    RUN_TEST(test_roles_headlight_preserved_in_fault);
    RUN_TEST(test_roles_headlight_unchanged_on_ready_entry);
    RUN_TEST(test_roles_headlight_works_when_bms_stale);

    RUN_TEST(test_roles_allOff_does_not_touch_flasher_channel);

    return UNITY_END();
}
