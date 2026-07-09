#pragma once
//
// BmsComputed.h — BmsPackData'nın YORUMLANMIŞ (karar verilmiş) sonucu.
//
// Rol 2 (Gömülü Sistem & Algoritma) çıktısının veri sözleşmesi. Ham hücre
// verisi (BmsPackData) computePack() ile bu yapıya dönüştürülür: paket
// gerilimi, min/max hücre, dengeleme bayrakları, SoC ve uyarı seviyesi.
//
// Donanım/IDF bağımlılığı YOKTUR — saf C++17. Hem ESP32 firmware'inde hem de
// native (host) Unity testlerinde derlenebilir.
//
#include <cstdint>

#include "BmsModel.h"  // BMS_CELL_COUNT, BmsPackData (PAYLAŞILAN sözleşme)

// --- Uyarı seviyeleri (warningLevel alanı) ---
// Tek bir bütünleşik ciddiyet derecesi; ekrandaki tehlike metnini de bu belirler.
static constexpr uint8_t BMS_WARN_OK = 0;        // Her şey nominal
static constexpr uint8_t BMS_WARN_WARNING = 1;   // Dikkat — eşiğe yaklaşıldı
static constexpr uint8_t BMS_WARN_CRITICAL = 2;  // Kritik — koruma gerekebilir
// G8/M4: Hücre kaynağı DOĞRULANMADI — uyarı seviyesi hesaplanamıyor. "Sağlıklı"
// (OK) göstermek YANLIŞ GÜVEN yaratır; CRITICAL göstermek de yalancı alarmdır.
// Bu sentinel "veri yok / nötr" anlamına gelir. NOT: Nextion warn bileşeni bu
// değeri (3) nötr/"--" olarak göstermelidir (ekran/.HMI dosyası işi).
static constexpr uint8_t BMS_WARN_NO_DATA = 3;

// --- Yorumlanmış paket durumu ---
// computePack() bu yapıyı eksiksiz doldurur. Tüketiciler (HMI paketleyici,
// orchestrator) yalnızca bu yapıyı okur; ham eşik mantığını tekrar etmez.
struct BmsComputed {
    uint32_t packVoltageMv;  // 24 hücrenin gerilim toplamı, milivolt.
                             // uint32: 24*4200=100800 mV uint16'ya sığmaz,
                             // nominal 24*3650=87600 mV bile taşardı.

    uint16_t cellMaxMv;       // En yüksek hücre gerilimi, mV
    uint16_t cellMinMv;       // En düşük hücre gerilimi, mV
    uint8_t cellMaxIndex;     // En yüksek hücrenin indeksi [0..23]
    uint8_t cellMinIndex;     // En düşük hücrenin indeksi [0..23]
    uint16_t cellDeltaMv;     // cellMaxMv - cellMinMv (dengesizlik göstergesi)

    uint8_t socPercent;       // Tahmini şarj durumu, 0..100

    // Dengeleme (pasif deşarj) bayrakları. balanceFlag[i]=true => i. hücrenin
    // deşarj direnci aktif edilmeli (o hücre fazla dolu).
    bool balanceFlag[BMS_CELL_COUNT];

    uint8_t warningLevel;  // BMS_WARN_OK / WARNING / CRITICAL

    int16_t tempMaxC;  // En yüksek hücre sıcaklığı, °C
    int16_t tempMinC;  // En düşük hücre sıcaklığı, °C
};
