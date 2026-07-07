#pragma once
// Fake RelayManager için global sayaçlar — testlerden include edilir.
extern unsigned g_fake_relay_allOn_count;
extern unsigned g_fake_relay_allOff_count;
extern unsigned g_fake_relay_allOff_silent_count;
extern unsigned g_fake_relay_setRelay_count;

// G3: actuator fault enjeksiyonu — testler VcuLogic'in polling davranışını
// (READY reddi / READY-DRIVE'da FAULT'a geçiş) doğrulamak için bayrağı set eder.
extern bool     g_fake_relay_actuatorFault;
extern unsigned g_fake_relay_clearFault_count;
extern unsigned g_fake_relay_verifyIfDue_count;

// G2: çağrı SIRASI kaydı — E-STOP/FAULT'ta sıfır-tork'un kontaktör açmadan
// ÖNCE çağrıldığını doğrulamak için paylaşılan monoton sayaç. Torque spy ve
// fake allOff aynı sayaçtan ilk çağrı sıra numaralarını alır.
extern unsigned g_fake_call_seq;              // monoton; ++ ile sıra numarası üretir
extern unsigned g_fake_relay_allOff_firstSeq; // 0 = allOff henüz çağrılmadı

// Tüm sayaçları sıfırlar; setUp() içinde çağrılmalıdır.
void fake_relay_reset(void);
