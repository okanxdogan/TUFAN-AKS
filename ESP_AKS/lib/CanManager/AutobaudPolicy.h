#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// CAN Autobaud Retry Policy (SAF, FreeRTOS/twai bağımsız, native test edilebilir).
// ---------------------------------------------------------------------------
// SORUN: CanManager::begin() boot'ta 500/125/250 kbps'i sırayla dener (her
// hızda 1 sn twai_receive bekler). Üçü de başarısız olursa 500 kbps'e
// fallback yapar ve BİR DAHA ASLA yeniden denemez. Saha sonucu: BMS boot
// anında sessizse (uykuda/geç açılıyor) veya bus gerçekte 125/250 kbps ise
// AKS kalıcı olarak sağır kalır — hiç LB-E000 logu gelmez (bkz.
// Documents/BRING_UP_CHECKLIST.md bölüm 4).
//
// ÇÖZÜM: bitrate doğrulanmamışken (bitrateVerified=false) VE henüz hiçbir
// geçerli frame alınmamışken (hasReceivedAnyFrame=false) — yani yalnızca
// "pre-reception" durumda — CAN_AUTOBAUD_RETRY_INTERVAL_MS'te bir yeniden
// algılama denensin. İki koşuldan HERHANGİ biri true olduğu anda (retry
// sırasında bitrate doğrulandı YA DA fallback hızında pasif olarak ilk
// geçerli frame alındı) retry KALICI OLARAK durur — çalışan bir bus'ta
// driver bir daha asla yeniden kurulmaz.
//
// Post-reception bayatlama (bir süre veri aktı, SONRADAN kesildi) bu
// mekanizmanın KAPSAMI DIŞINDADIR — o zaten bitrate doğrulanmıştır, sorun
// bitrate değildir; mevcut BmsFreshness/TEL_bmsTimeoutActive yolu geçerlidir.
//
// Tick birimi agnostiktir (çağıran aynı birimi verir); unsigned çıkarma
// (now - lastAttemptTick) sayaç taşmasında (tick wraparound) da doğru
// sonuç verir — bkz. BmsFreshness.h / CanParse::isMotorStatusTimedOut ile
// aynı idiom.
inline bool autobaud_should_retry(bool bitrateVerified,
                                  bool hasReceivedAnyFrame, uint32_t now,
                                  uint32_t lastAttemptTick,
                                  uint32_t retryIntervalTicks) {
    if (bitrateVerified || hasReceivedAnyFrame)
        return false;
    return (uint32_t)(now - lastAttemptTick) >= retryIntervalTicks;
}
