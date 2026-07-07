#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// G9 — Motor error-flag debounce (SAF, donanımsız, native test edilebilir).
// ---------------------------------------------------------------------------
// SORUN: Motor errorFlags TEK frame'de FAULT (kontaktör açma) tetikliyordu.
// CAN CRC bit hatası gibi GEÇİCİ/tek-seferlik bir flag, güvenli kapanış
// sırasını süratle çalıştırıp kontaktörleri (G2 ile yük altında) açtırabilirdi.
//
// ÇÖZÜM: N ardışık frame onayı. Her motor status frame'inde çağrılır:
//   - errorFlags != 0  => ardışık sayaç artar,
//   - errorFlags == 0  => sayaç SIFIRLANIR (temiz frame zinciri kırar),
//   - sayaç debounceFrames'e ULAŞINCA fault ONAYLANIR (true döner).
//
// Durum çağırana ait: consecutiveCount referansla güncellenir (CanManager bir
// üye değişkende tutar). Fonksiyon MOTOR_DRIVER_PRESENT bayrağından TAMAMEN
// BAĞIMSIZDIR — motor entegrasyonunda (bayrak 1) hazır çalışır; bayrak 0 iken
// zaten hiç motor frame'i gelmediğinden çağrılmaz, davranış DEĞİŞMEZ.
//
// consecutiveCount uint16_t sınırında doygunlaşır (taşma/wrap yok); onaylandıktan
// sonra ardışık hatalı frame'ler true döndürmeye devam eder (fault sürer),
// araya tek temiz frame girerse sayaç 0'a döner ve onay düşer.
inline bool motorErrorFaultConfirmed(uint8_t errorFlags,
                                     uint16_t& consecutiveCount,
                                     uint16_t debounceFrames) {
    if (errorFlags == 0) {
        consecutiveCount = 0;
        return false;
    }
    if (consecutiveCount < 0xFFFFu)
        ++consecutiveCount;
    return consecutiveCount >= debounceFrames;
}
