#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// G12 — BMS veri tazeliği, mesaj-ID bazında (SAF, native test edilebilir).
// ---------------------------------------------------------------------------
// SORUN: BMS iki AYRI mesaj-ID'sinden beslenir — E000 (packV/current/soc) ve
// E001 (sıcaklık). Freshness yalnız E000'e bağlıyken, E000 akmaya devam edip
// E001 kesilirse TEL_bmsTempHighestC süresiz BAYAT kalır ama TEL_bmsDataValid
// hâlâ true görünürdü. Böylece bayat sıcaklık P1 READY interlock'u ve kritik
// eşik kontrollerinden gizlenirdi.
//
// ÇÖZÜM: freshness'i ID bazına değerlendir. BMS verisi ancak HER İKİ ID de
// görülmüş VE timeout içinde taze ise geçerlidir; biri bayatsa TEL_bmsDataValid
// düşer ve (görülmüş-ama-bayat ise) TEL_bmsTimeoutActive ile kritik yola eskale
// edilir.
//
// Per-ID timeout semantiği CanParse::isBmsStatusTimedOut ile AYNIDIR
// ((now - lastTick) >= timeout → bayat). Burada INLINE tutulur ki native test,
// link-sorunlu can_parsing suite'ine (CanParse.cpp) bağımlı olmadan tam
// senaryoyu doğrulayabilsin. Tick birimi agnostiktir (çağıran aynı birimi verir).

struct BmsFreshnessResult {
    bool dataValid;      // TEL_bmsDataValid: her iki ID de taze mi
    bool timeoutActive;  // TEL_bmsTimeoutActive: görülmüş bir ID bayatladı mı
};

// Tek bir ID taze mi? Hiç görülmediyse (pre-reception) taze DEĞİLDİR.
inline bool bms_id_fresh(bool seen, uint32_t now, uint32_t lastTick,
                         uint32_t timeoutTicks) {
    if (!seen)
        return false;
    return (uint32_t)(now - lastTick) < timeoutTicks;
}

// E000 + E001 birleşik tazelik değerlendirmesi.
inline BmsFreshnessResult bms_evaluate_freshness(bool sawE000, uint32_t lastE000,
                                                 bool sawE001, uint32_t lastE001,
                                                 uint32_t now,
                                                 uint32_t timeoutTicks) {
    const bool e000Fresh = bms_id_fresh(sawE000, now, lastE000, timeoutTicks);
    const bool e001Fresh = bms_id_fresh(sawE001, now, lastE001, timeoutTicks);

    BmsFreshnessResult r;
    r.dataValid = e000Fresh && e001Fresh;
    // Post-reception bayatlama (görülmüş ama taze değil) kritik yola eskale
    // edilir; pre-reception (hiç görülmemiş) IDLE'da tolere edilir.
    const bool e000Stale = sawE000 && !e000Fresh;
    const bool e001Stale = sawE001 && !e001Fresh;
    r.timeoutActive = e000Stale || e001Stale;
    return r;
}
