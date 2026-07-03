#pragma once

// Saf, donanimsiz telemetri sanitizasyon fonksiyonlari.
//
// UKS parser'i (TUFAN-UKS-TELEMETRY Core/Src/telemetry.c, Decode_Line)
// her alani ayri ayri sert aralik kontrolunden (Parse_Int) gecirir ve
// TEK alan aralik disindaysa TUM frame'i reddeder (parse_fail) — yani
// RPM, gerilim, sicaklik gibi diger tum gecerli alanlar da birlikte
// kaybolur. Sartname 9.2.b/9.2.d telemetri akisinin surekliligini
// zorunlu kildigi icin AKS, UKS'in kabul araligi disina KESINLIKLE
// cikmayan degerler uretmelidir.
//
// Bu dosya donanim/log bagimliligi olmadan (yalniz <cstdint>/<climits>)
// native ortamda test edilebilir olacak sekilde tasarlandi. Cagiran
// taraf (CanManager) throttle'li WARN log'u kendi bunyesinde uretir.
#include <cstdint>
#include <climits>

#include "Telemetry.h"

namespace TelemetrySanitize {

// UKS Decode_Line: Parse_Int(f[12], min=1, max=4).
// Bilinmeyen/gecersiz BMS sistem durumu guvenlik acisindan FAULT (4)
// gibi ele alinir — hem guvenli varsayim hem de UKS'in reddetmeyecegi
// bir deger garantisi.
inline uint8_t sanitizeSystemState(uint8_t raw) {
    return (raw >= 1U && raw <= 4U) ? raw : 4U;
}

// UKS Decode_Line: Parse_Int(f[15], min=0, max=10000).
inline uint16_t sanitizeSoc(uint16_t raw) {
    return (raw > 10000U) ? 10000U : raw;
}

// UKS Decode_Line: Parse_Int(f[14], min=-2147483647, max=2147483647) —
// tam int32_t araliginin degil, INT32_MIN'i (bir eksik ucta) HARIC
// tutan simetrik bir aralik. INT32_MIN fiziksel olarak anlamsiz bir
// akim degeri oldugundan (sensor/CAN bozulma senaryosu), UKS sinirina
// gore +1 kaydirmanin fiziksel anlam kaybi yoktur.
inline int32_t sanitizeCurrent(int32_t raw) {
    return (raw == INT32_MIN) ? (INT32_MIN + 1) : raw;
}

// Tek ortak sanitize kapısı: canlı VE replay (OfflineBuffer'dan gelen)
// paketler, UKS'e gitmeden hemen önce (sendStatus çağrısının hemen
// öncesinde) buradan geçer — böylece ikisi de aynı garantiye sahip olur
// (S4). Yukarıdaki alan-bazlı fonksiyonların KENDİSİ değiştirilmedi;
// bu yalnızca onları tek noktada birleştiren bir sarmalayıcıdır.
inline TelemetryData sanitizeForUplink(const TelemetryData& raw) {
    TelemetryData out = raw;
    out.TEL_bmsSystemState     = sanitizeSystemState(out.TEL_bmsSystemState);
    out.TEL_bmsSocHundredths   = sanitizeSoc(out.TEL_bmsSocHundredths);
    out.TEL_bmsCurrentCentiMa  = sanitizeCurrent(out.TEL_bmsCurrentCentiMa);
    return out;
}

}  // namespace TelemetrySanitize
