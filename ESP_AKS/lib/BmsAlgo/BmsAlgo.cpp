#include "BmsAlgo.h"

// Saf C++; <cstdint> dışında bağımlılık yok. IDF/Arduino çağrısı YAPMAZ.

namespace {

// in.isValid == false durumunda dönen "güvenli" çıktı: ekranda kritik uyarı
// gösterilir, tüm sayısal alanlar zararsız sıfır/nominal değerdedir, hiçbir
// hücre yanlışlıkla deşarj edilmez.
BmsComputed makeSafeInvalid() {
    BmsComputed c{};                  // tüm alanlar 0 / balanceFlag tümü false
    c.warningLevel = BMS_WARN_CRITICAL;
    c.socPercent = 0;                 // bilinmiyor => en güvenli (boş varsay)
    // packVoltageMv, cellMin/Max, delta, temp => 0 (anlamlı veri yok)
    return c;
}

// Paket geçerli (isValid=true) AMA bu anlık görüntüde 24 hücrenin TAMAMI
// henüz taze/tam değil (cellDataValid=false — kaynak mapping'i DOĞRULANDI,
// E015-E020, G8/M4 FIX; false burada yalnız "boot sonrası tüm CAN ID'leri
// henüz gelmedi / freshness timeout" anlamına gelir). Bu durumda
// dengeleme/uyarı/min-max hücre gerçek veriye dayanamaz — pack
// ortalamasından fabrike per-hücre değere GÜVENMEK gerçek dengesizliği
// maskeler (bir hücre 2.3 V'a düşse bile ekran sağlıklı görünürdü). Bu
// yüzden "veri yok" döndürülür: hiçbir hücre dengelenmez, uyarı NO_DATA
// (nötr). makeSafeInvalid'den FARKI: burada pack bayat/arızalı DEĞİL,
// yalnız hücre görünürlüğü henüz tam değil → CRITICAL yalancı alarmı
// üretilmez.
BmsComputed makeNoCellData() {
    BmsComputed c{};                   // tüm alanlar 0 / balanceFlag tümü false
    c.warningLevel = BMS_WARN_NO_DATA;
    c.socPercent = 0;                  // ortalamadan SoC tahmini de fabrikasyon olurdu
    // cellMin/Max/delta/temp/packV => 0: anlamlı per-hücre veri yok. Ekran
    // tarafı cellMax/Min'i kendi sentinel'iyle ("--") gösterir.
    return c;
}

// Ortalama hücre geriliminden (mV) lineer SoC% (0..100). Aralık dışı clamp'lenir.
uint8_t socFromAvgMv(uint32_t avgMv) {
    if (avgMv <= BMS_SOC_EMPTY_MV) return 0;
    if (avgMv >= BMS_SOC_FULL_MV) return 100;
    // 0..100 ölçekle; tamsayı aritmetiğinde taşmayı önlemek için uint32 çarpım.
    const uint32_t span = BMS_SOC_FULL_MV - BMS_SOC_EMPTY_MV;  // 1200 mV
    const uint32_t above = avgMv - BMS_SOC_EMPTY_MV;
    return static_cast<uint8_t>((above * 100u) / span);
}

}  // namespace

BmsComputed computePack(const BmsPackData& in) {
    // Geçersiz girdi => güvenli kritik çıktı (erken dönüş).
    if (!in.isValid) {
        return makeSafeInvalid();
    }

    // Paket geçerli ama hücre kaynağı doğrulanmamış => dengeleme/uyarı hesaplama,
    // "veri yok" döndür (fabrike per-hücre veriye güvenme). Bkz. makeNoCellData.
    if (!in.cellDataValid) {
        return makeNoCellData();
    }

    BmsComputed c{};  // balanceFlag[] dahil her şey 0/false başlar

    // --- Min/Max/toplam gerilim tek geçişte ---
    // Sıcaklık ARTIK hücre dizisinden TARANMAZ: BMS per-hücre sıcaklık
    // yayınlamıyor (cellTempC[] fiilen hep 0'dı ve sıcaklık uyarısı HİÇ
    // tetiklenmiyordu). Gerçek paket sıcaklığı packTempMaxC/MinC alanlarından
    // gelir (0xE001 → TEL_bmsTempHighestC/LowestC, main.cpp doldurur).
    uint32_t sumMv = 0;
    uint16_t maxMv = in.cellVoltageMv[0];
    uint16_t minMv = in.cellVoltageMv[0];
    uint8_t maxIdx = 0;
    uint8_t minIdx = 0;
    const int16_t tMax = in.packTempMaxC;
    const int16_t tMin = in.packTempMinC;

    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        const uint16_t v = in.cellVoltageMv[i];
        sumMv += v;
        if (v > maxMv) { maxMv = v; maxIdx = i; }
        if (v < minMv) { minMv = v; minIdx = i; }
    }

    // packVoltageMv uint32 olduğundan tüm aralık (24*4200=100800 mV) sığar;
    // doygunluk/sarma gerekmez.
    c.packVoltageMv = sumMv;

    c.cellMaxMv = maxMv;
    c.cellMinMv = minMv;
    c.cellMaxIndex = maxIdx;
    c.cellMinIndex = minIdx;
    c.cellDeltaMv = static_cast<uint16_t>(maxMv - minMv);
    c.tempMaxC = tMax;
    c.tempMinC = tMin;

    // --- SoC: ortalama hücre geriliminden lineer ---
    const uint32_t avgMv = sumMv / BMS_CELL_COUNT;
    c.socPercent = socFromAvgMv(avgMv);

    // --- Dengeleme kuralı ---
    // delta, BMS_BALANCE_THRESHOLD_MV'i AŞIYORSA (strictly greater): en yüksek
    // hücreye BMS_BALANCE_TOP_MARGIN_MV marjı içindeki tüm hücreleri deşarj et.
    // delta <= eşik ise hiçbir hücre dengelenmez (balanceFlag tümü false kalır).
    if (c.cellDeltaMv > BMS_BALANCE_THRESHOLD_MV) {
        // Marj alt sınırını taşmadan hesapla (maxMv >= margin garanti değil).
        const uint16_t lowBound =
            (maxMv > BMS_BALANCE_TOP_MARGIN_MV)
                ? static_cast<uint16_t>(maxMv - BMS_BALANCE_TOP_MARGIN_MV)
                : 0;
        for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
            if (in.cellVoltageMv[i] >= lowBound) {
                c.balanceFlag[i] = true;  // bu hücre "en yüksekler" bandında
            }
        }
    }

    // --- Uyarı seviyesi: en kötü hücre/sıcaklık koşulu kazanır ---
    // Sıcaklık karşılaştırması >= : VCU katmanıyla (isTempWarning/isTempCritical)
    // aynı anda tetiklenir — tam 70 °C'de VCU FAULT'a geçerken ekran da
    // CRITICAL gösterir. Hücre voltajı strictly < / > semantiğini korur.
    uint8_t level = BMS_WARN_OK;
    // CRITICAL koşulları
    if (minMv < BMS_CELL_UNDERVOLT_CRIT_MV ||
        maxMv > BMS_CELL_OVERVOLT_CRIT_MV ||
        tMax >= BMS_TEMP_OVERTEMP_CRIT_C) {
        level = BMS_WARN_CRITICAL;
    }
    // WARNING koşulları (yalnız henüz CRITICAL değilse)
    else if (minMv < BMS_CELL_UNDERVOLT_WARN_MV ||
             maxMv > BMS_CELL_OVERVOLT_WARN_MV ||
             tMax >= BMS_TEMP_OVERTEMP_WARN_C) {
        level = BMS_WARN_WARNING;
    }
    c.warningLevel = level;

    return c;
}
