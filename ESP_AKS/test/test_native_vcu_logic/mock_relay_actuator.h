#pragma once
#include "IRelayActuator.h"
#include "SystemConfig.h"  // RELAY_TOTAL_CHANNELS, RELAY_CONTACTOR_BANK_MASK

// ---------------------------------------------------------------------------
// M2: VcuLogic'e enjekte edilen GERÇEK arayüz mock'u (IRelayActuator).
// Eski fake_relay_manager.cpp link-time sembol değiştirme hilesinin yerini
// alır: artık RelayManager singleton'ı taklit edilmez; VcuLogic::init(mock)
// ile bu nesne referansla verilir.
//
// Sayaç ADLARI ve semantiği bilerek korunmuştur (g_fake_relay_*) — böylece
// test SENARYOLARI değişmeden kalır, yalnız kurulum (init çağrısı) değişir.
// ---------------------------------------------------------------------------
extern unsigned g_fake_relay_allOn_count;
extern unsigned g_fake_relay_allOff_count;
extern unsigned g_fake_relay_allOff_silent_count;
extern unsigned g_fake_relay_setRelay_count;

// G3: actuator fault enjeksiyonu — testler VcuLogic'in polling davranışını
// (READY reddi / READY-DRIVE'da FAULT'a geçiş) doğrulamak için bayrağı set eder.
extern bool     g_fake_relay_actuatorFault;
extern unsigned g_fake_relay_clearFault_count;
extern unsigned g_fake_relay_verifyIfDue_count;

// Kanal-bazlı mantıksal durum izi (true = röle çekili/kontaktör kapalı).
// allOn/allOff, gerçek RelayManager ile AYNI bank-maskesi semantiğini taklit
// eder (RELAY_CONTACTOR_BANK_MASK dışındaki kanal — roller atandığında flaşör
// — allOn/allOff'tan ETKİLENMEZ); setRelay tekil kanalı yazar. S1/S2 ve
// flaşör testleri bu diziden doğrular; eski testler yalnız sayaçları okur.
extern bool g_fake_relay_channelState[RELAY_TOTAL_CHANNELS];

// G2: çağrı SIRASI kaydı — E-STOP/FAULT'ta sıfır-tork'un kontaktör açmadan
// ÖNCE çağrıldığını doğrulamak için paylaşılan monoton sayaç. Torque spy ve
// mock allOff aynı sayaçtan ilk çağrı sıra numaralarını alır.
extern unsigned g_fake_call_seq;              // monoton; ++ ile sıra numarası üretir
extern unsigned g_fake_relay_allOff_firstSeq; // 0 = allOff henüz çağrılmadı

// Tüm sayaçları sıfırlar; setUp()/prime içinde çağrılmalıdır.
void fake_relay_reset(void);

// IRelayActuator mock'u — çağrıları yukarıdaki global sayaçlara işler. Metot
// adları gerçek RelayManager yüzeyiyle birebir aynıdır.
class MockRelayActuator : public IRelayActuator {
   public:
    void allOn() override;
    void allOff(bool silent) override;
    void setRelay(uint8_t channel, bool state) override;
    void verifyIfDue(uint32_t nowMs) override;
    bool verifyOutputs() override;
    bool hasActuatorFault() const override;
    void clearActuatorFault() override;
};

// Testlerin VcuLogic::init(g_mockRelay) ile enjekte ettiği kalıcı mock nesnesi.
extern MockRelayActuator g_mockRelay;
