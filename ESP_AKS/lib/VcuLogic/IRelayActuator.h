#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// IRelayActuator — VcuLogic'in aktüatör (röle / kontaktör sürücüsü) katmanından
// enjekte edilerek kullandığı soyut arayüz.
// ---------------------------------------------------------------------------
// M2: VcuLogic artık somut RelayManager::instance() singleton'ına DEĞİL, bu
// arayüze bağımlıdır (bağımlılık tersine çevirme). Böylece:
//   - Üretimde main.cpp (kompozisyon kökü) RelayManager'ı saran bir adapter
//     enjekte eder; RelayManager saf donanım sürücüsü kalır (VcuLogic'ten
//     habersiz), lib/VcuLogic'e bağımlılığı olmaz.
//   - Native testler gerçek bir mock nesnesi enjekte eder — link-time sembol
//     değiştirme (fake_relay_manager.cpp) veya VCU_LOGIC_TESTABLE makrosu
//     GEREKMEZ.
//
// Yüzey, mevcut RelayManager public API'sine BİREBİR uyacak şekilde adlandırıldı
// (P4 sonrası oluşan yüzey): allOn / allOff / setRelay / verifyOutputs /
// verifyIfDue / actuator-fault sorgu. Bu projede PRECHARGE devresi YOK; bu
// yüzden closeSequence/openSequence gibi bir soyutlama TANIMLANMADI.
//
// Sanal-çağrı maliyeti: VCU görev tik'i başına (20 ms) yalnız birkaç çağrı
// yapılır → ESP32'de vtable dispatch ihmal edilebilir. Kod tabanının mevcut
// donanım-soyutlama deseniyle (RelayManager, CanManager gibi sınıflar) tutarlı
// olsun diye fonksiyon-pointer struct yerine soyut sınıf seçildi.
class IRelayActuator {
   public:
    virtual ~IRelayActuator() = default;

    // Kontaktör BANK maskesini (RELAY_CONTACTOR_BANK_MASK) kapatır.
    // RELAY_ROLES_ASSIGNED=0 iken maske 10 kanalın tamamı (eski "tüm pozitif
    // kontaktörler" davranışı); roller atandığında flaşör kanalı maskenin
    // dışındadır (durumu korunur).
    virtual void allOn() = 0;

    // Kontaktör bank maskesini açar (GÜVENLİK — şartname 8.2.a.vi; maske
    // dışı flaşör kanalına dokunmaz). silent=true yalnız LOG açısından
    // sessizdir; G3 geri-okuma/doğrulaması yine yapılır.
    virtual void allOff(bool silent = false) = 0;

    // Tekil röle kanalını set/reset eder.
    virtual void setRelay(uint8_t channel, bool state) = 0;

    // Periyodik röle çıkış doğrulaması tetikleyicisi. RELAY_VERIFY_PERIOD_MS'den
    // seyrek olmayacak şekilde iç geri-okumayı yapar (her tik'te çağrılabilir;
    // hız sınırlama sürücü katmanına aittir).
    virtual void verifyIfDue(uint32_t nowMs) = 0;

    // Anlık çıkış doğrulaması (OLAT/IODIR geri-okuma + gölge-durum karşılaştırma).
    // Eşleşirse true. Uyuşmazlıkta re-init + re-assert + kalıcı fault latch (G3).
    virtual bool verifyOutputs() = 0;

    // Kalıcı (latched) aktüatör-fault bayrağı (G3). VcuLogic her tick okur.
    virtual bool hasActuatorFault() const = 0;
    virtual void clearActuatorFault() = 0;
};
