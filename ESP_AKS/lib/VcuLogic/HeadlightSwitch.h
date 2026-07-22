#pragma once
#include <cstdint>

#include "SystemConfig.h"  // HEADLIGHT_SWITCH_ACTIVE_LEVEL, HEADLIGHT_DEBOUNCE_MS

// ---------------------------------------------------------------------------
// Far fiziksel düğmesi — SAF karar mantığı (şartname B2 9.19.c).
// ---------------------------------------------------------------------------
// Far ARTIK ekran butonuyla değil, sürücünün basacağı fiziksel bir düğmeyle
// kontrol edilir. Bu başlık, düğme okumasından far ON/OFF kararını üreten SAF
// (donanım/FreeRTOS bağımsız, native test edilebilir) mantığı içerir —
// VcuLogic.h'deki flasherDesiredState/fanDesiredState deseniyle paralel, ama
// debounce durumu taşıdığından stateless bir predicate yerine çağırana ait bir
// State bloğu + update() fonksiyonu kullanır (durum çağırana aittir; aynı
// idiom: lib/DisplayHMI/ResyncPolicy.h). VcuLogic bu State'i modül-içi statikte
// tutar; native testler doğrudan update()'i çağırır (VcuLogic.cpp linklenmeden).
//
// İKİ DÜĞME TİPİ (HEADLIGHT_SWITCH_LATCHING, SystemConfig.h):
//   * latching=true (kalıcı/anahtarlı — otomotiv normu): far durumu doğrudan
//     anahtar KONUMUNU takip eder. ESP reset atsa bile boot'ta update() ilk
//     okumadan far durumunu anahtar konumuna eşitler → anahtar hâlâ açık
//     konumundaysa far geri yanar; reset sonrası desenkronizasyon İMKÂNSIZ.
//   * latching=false (anlık/butonlu): far, düğmenin BASMA kenarında
//     (open→closed) toggle edilir; basılı tutmak tekrar toggle ETMEZ; boot'ta OFF.
//
// DEBOUNCE: aday konum, debounceMs süresince kararlı kalmadıkça (bounce/
// kararsız geçişler) commit EDİLMEZ; kısa süreli sıçramalar yok sayılır.
//
// Zaman birimi agnostiktir (çağıran aynı birimi verir); unsigned çıkarma
// (now - candidateSinceMs) sayaç taşmasında da doğru sonuç verir (AutobaudPolicy.h
// / ResyncPolicy.h ile aynı idiom).

namespace HeadlightSwitch {

struct State {
    bool headlightOn;           // far çıkışının istenen durumu (update() çıktısı)
    bool stableEngaged;         // son debounce'lanmış düğme konumu (kapalı=true)
    bool candidateEngaged;      // debounce penceresindeki aday konum
    uint32_t candidateSinceMs;  // adayın ilk görüldüğü zaman damgası
    bool initialized;           // ilk okuma yapıldı mı (boot senkronu)
};

// rawLevel : HAM pin okuması. INPUT_PULLUP → düğme pini GND'ye çeker; far açık
//            konumu/basılı = LOW = HEADLIGHT_SWITCH_ACTIVE_LEVEL (0).
// nowMs    : monoton zaman damgası (VcuLogic s_uptimeMs verir; testler açık verir).
// latching : true = kalıcı/anahtarlı, false = anlık/butonlu (yukarıya bkz.).
// debounceMs: kararlılık penceresi (HEADLIGHT_DEBOUNCE_MS).
// Dönüş     : far ON (true) / OFF (false) — st.headlightOn ile aynı.
inline bool update(State& st, int rawLevel, uint32_t nowMs, bool latching,
                   uint32_t debounceMs) {
    const bool engaged = (rawLevel == HEADLIGHT_SWITCH_ACTIVE_LEVEL);

    if (!st.initialized) {
        // Boot senkronu: ilk okumadan kararlı konumu debounce beklemeden kur —
        // anahtar konumu boot anında bilinir olmalı (latching desenkronizasyon
        // önlemesi tam da buna dayanır).
        st.stableEngaged = engaged;
        st.candidateEngaged = engaged;
        st.candidateSinceMs = nowMs;
        st.initialized = true;
        if (latching) {
            // Far açılışta anahtar KONUMUNU takip eder.
            st.headlightOn = engaged;
        }
        // Momentary: boot OFF (st.headlightOn zaten false); ilk basma kenarı
        // sonradan toggle eder.
        return st.headlightOn;
    }

    // Aday konum değiştiyse debounce penceresini yeniden başlat (kararsız
    // sıçrama sayacı sıfırlanır).
    if (engaged != st.candidateEngaged) {
        st.candidateEngaged = engaged;
        st.candidateSinceMs = nowMs;
    }

    // Aday, kararlı konumdan FARKLI ve debounce süresince kararlı kaldıysa
    // commit et.
    if (st.candidateEngaged != st.stableEngaged &&
        (uint32_t)(nowMs - st.candidateSinceMs) >= debounceMs) {
        const bool wasEngaged = st.stableEngaged;
        st.stableEngaged = st.candidateEngaged;
        if (latching) {
            st.headlightOn = st.stableEngaged;  // konumu doğrudan takip et
        } else if (!wasEngaged && st.stableEngaged) {
            // Momentary: yalnız basma kenarında (open→closed) toggle; bırakma
            // kenarı (closed→open) ve basılı tutmak far'ı DEĞİŞTİRMEZ.
            st.headlightOn = !st.headlightOn;
        }
    }
    return st.headlightOn;
}

}  // namespace HeadlightSwitch
