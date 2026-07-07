#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// IRelayActuator — VcuLogic'in aktüatör (röle / kontaktör sürücüsü) katmanından
// ihtiyaç duyduğu ASGARİ soyut arayüz.
// ---------------------------------------------------------------------------
// M2: VcuLogic artık somut RelayManager::instance() singleton'ına DEĞİL, bu
// arayüze bağımlıdır (bağımlılık tersine çevirme). Arayüz, onu KULLANAN yüksek
// seviyeli politikaya (VcuLogic) ait; somut düşük seviyeli sürücü (RelayManager)
// bu arayüzü karşılar. Böylece:
//   - Üretimde main.cpp (kompozisyon kökü) RelayManager'ı saran bir adapter
//     enjekte eder; RelayManager'ın VcuLogic'ten haberi olmaz.
//   - Native testler gerçek bir mock nesnesi enjekte eder — link-time sembol
//     değiştirme (fake_relay_manager.cpp) veya VCU_LOGIC_TESTABLE makrosu
//     GEREKMEZ.
//
// Yüzey, P4/P5'te oluşan gerçek RelayManager çağrı yüzeyinden türetildi
// (asgari set): kontaktör kapama/açma + periyodik doğrulama + kalıcı (latched)
// aktüatör-fault sorgu/temizleme.
//
// Sanal-çağrı maliyeti: VCU görev tik'i başına (20 ms) yalnız birkaç çağrı
// yapılır → ESP32'de vtable dispatch ihmal edilebilir. Kod tabanının mevcut
// donanım-soyutlama deseniyle (RelayManager, CanManager gibi sınıflar) tutarlı
// olsun diye fonksiyon-pointer struct yerine soyut sınıf seçildi.
class IRelayActuator {
   public:
    virtual ~IRelayActuator() = default;

    // Tüm pozitif kontaktörleri kapatır (READY girişi — HV bus enerjilenir).
    virtual void closeAllContactors() = 0;

    // Tüm kontaktörleri açar (GÜVENLİK). silent=true yalnız LOG açısından
    // sessizdir; G3 geri-okuma/doğrulaması yine yapılır.
    virtual void openAllContactors(bool silent) = 0;

    // Periyodik röle çıkış doğrulaması. RELAY_VERIFY_PERIOD_MS'den seyrek
    // olmayacak şekilde iç geri-okumayı tetikler (her tik'te çağrılabilir).
    virtual void verifyIfDue(uint32_t nowMs) = 0;

    // Kalıcı (latched) aktüatör-fault bayrağı (G3). VcuLogic her tick okur.
    virtual bool hasActuatorFault() const = 0;
    virtual void clearActuatorFault() = 0;
};
