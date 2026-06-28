#pragma once
//
// bms_edge_helpers.h — Rol 3 (Sistem Entegratörü) EDGE CASE testleri için
// SADECE BmsModel.h'ye (BmsPackData) bağımlı yardımcı doğrulayıcılar.
//
// AMAÇ: Edge case testlerini paylaşılan sözleşmeye (BmsPackData) karşı, hiçbir
// BmsSim/BmsAlgo modülüne DERLEME bağımlılığı OLMADAN yazmak. Buradaki saf
// fonksiyonlar, bir tüketicinin (algoritma/HMI) ham paket üzerinde yapması
// beklenen MİNİMUM veri-doğrulama mantığını modeller. Orchestrator final
// entegrasyonda gerçek BmsAlgo'yu bağladığında bu beklentiler referans kalır.
//
// Saf C++17 — donanım/IDF bağımlılığı YOKTUR.
//
#include <cstdint>

#include "BmsModel.h"  // BMS_CELL_COUNT, BmsPackData (PAYLAŞILAN sözleşme)

namespace bmsedge {

// --- Beklenen sınır sabitleri (sözleşme-tarafı; BmsAlgo eşiklerinin AYNASI
//     DEĞİL, bağımsız doğrulama referansıdır) -------------------------------
static constexpr uint16_t kCellMaxPlausibleMv = 4250;  // üstü => aşırı şarj
static constexpr uint16_t kCellMinPlausibleMv = 2500;  // altı => aşırı deşarj
static constexpr int16_t kTempOverC = 70;              // > => aşırı sıcak
static constexpr int16_t kTempUnderC = -20;            // < => aşırı soğuk

// 24 hücrenin teorik maksimum gerilim toplamı (mV). uint16 (max 65535) bu
// değeri TUTAMAZ — KRİTİK taşma sınırı. Bu sabit int32 olarak tutulur.
static constexpr int32_t kPackSumMaxMv =
    static_cast<int32_t>(BMS_CELL_COUNT) * 4200;  // 24 * 4200 = 100800

// --- Üretici yardımcılar ---------------------------------------------------

// Tüm hücreleri tek bir gerilime, tüm sıcaklıkları tek bir değere ayarlar.
inline BmsPackData makeUniform(uint16_t cellMv, int16_t tempC,
                               int32_t currentMa, bool valid) {
    BmsPackData p{};
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        p.cellVoltageMv[i] = cellMv;
        p.cellTempC[i] = tempC;
    }
    p.packCurrentMa = currentMa;
    p.isValid = valid;
    return p;
}

// Nominal, sağlıklı bir paket (tüm hücreler 3650 mV, 25 °C, geçerli).
inline BmsPackData makeNominal() {
    return makeUniform(3650, 25, 0, true);
}

// --- Saf doğrulayıcılar (tüketicinin yapması beklenen kontroller) ----------

// Toplam paket gerilimini int32 olarak güvenli biçimde toplar (taşma YOK).
inline int32_t sumPackVoltageMv(const BmsPackData& p) {
    int32_t sum = 0;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        sum += static_cast<int32_t>(p.cellVoltageMv[i]);
    }
    return sum;
}

// En yüksek hücre indeksi (eşitlikte ilk/en küçük indeks).
inline uint8_t maxCellIndex(const BmsPackData& p) {
    uint8_t idx = 0;
    for (uint8_t i = 1; i < BMS_CELL_COUNT; ++i) {
        if (p.cellVoltageMv[i] > p.cellVoltageMv[idx]) idx = i;
    }
    return idx;
}

// En düşük hücre indeksi (eşitlikte ilk/en küçük indeks).
inline uint8_t minCellIndex(const BmsPackData& p) {
    uint8_t idx = 0;
    for (uint8_t i = 1; i < BMS_CELL_COUNT; ++i) {
        if (p.cellVoltageMv[i] < p.cellVoltageMv[idx]) idx = i;
    }
    return idx;
}

// max - min gerilim farkı (dengesizlik göstergesi). int32 ile taşmasız.
inline int32_t cellDeltaMv(const BmsPackData& p) {
    return static_cast<int32_t>(p.cellVoltageMv[maxCellIndex(p)]) -
           static_cast<int32_t>(p.cellVoltageMv[minCellIndex(p)]);
}

// uint16 toplama yapılırsa sonucun ne olacağını GÖSTERİR (taşma kanıtı için).
// Gerçek koddan farklı olarak burada bilerek uint16 aritmetiği uygulanır.
inline uint16_t sumPackVoltageMvAsUint16(const BmsPackData& p) {
    uint16_t sum = 0;  // kasıtlı dar tip
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        sum = static_cast<uint16_t>(sum + p.cellVoltageMv[i]);
    }
    return sum;
}

// Sözleşme-tarafı geçerlilik: paket güvenle yorumlanabilir mi?
inline bool isConsumable(const BmsPackData& p) { return p.isValid; }

// Aşırı sıcaklık (herhangi bir hücre > kTempOverC).
inline bool hasOvertemp(const BmsPackData& p) {
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (p.cellTempC[i] > kTempOverC) return true;
    }
    return false;
}

// Aşırı soğuk (herhangi bir hücre < kTempUnderC).
inline bool hasUndertemp(const BmsPackData& p) {
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (p.cellTempC[i] < kTempUnderC) return true;
    }
    return false;
}

// Aşırı deşarj (herhangi bir hücre < kCellMinPlausibleMv).
inline bool hasUndervolt(const BmsPackData& p) {
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (p.cellVoltageMv[i] < kCellMinPlausibleMv) return true;
    }
    return false;
}

// "Tüm hücreler 0 mV" — sensör kopması / boş çerçeve sezgiseli.
inline bool allCellsZero(const BmsPackData& p) {
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        if (p.cellVoltageMv[i] != 0) return false;
    }
    return true;
}

}  // namespace bmsedge
