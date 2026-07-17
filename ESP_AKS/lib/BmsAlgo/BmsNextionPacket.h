#pragma once
//
// BmsNextionPacket.h — BmsComputed/BmsPackData'dan Nextion KOMUT METİNLERİ
// üreten SAF katman. UART'a YAZMAZ.
//
// Tasarım: her komut (örn. "cell0.val=3700") bir callback'e verilir. Komut
// gövdesi 0xFF 0xFF 0xFF end-byte'larını İÇERMEZ; bu byte'lar gerçek UART
// gönderiminde orchestrator tarafından eklenir (mevcut HMIHelpers kalıbı).
// Callback tabanlı API native test edilebilir: test, callback'te string'leri
// toplar; firmware'de callback uart_write_bytes + HMI_sendEndBytes çağırır.
//
// ---------------------------------------------------------------------------
// FIRMWARE ENTEGRASYON ÖRNEĞİ (orchestrator / HMI task içinde):
//
//   #include "BmsAlgo.h"
//   #include "BmsNextionPacket.h"
//   #include "HMIHelpers.h"          // HMI_sendEndBytes
//   #include "driver/uart.h"          // uart_write_bytes
//   #include "SystemConfig.h"         // HMI_UART_NUM
//
//   BmsPackData raw; source.read(raw);
//   BmsComputed comp = computePack(raw);
//   buildBmsNextionCommands(comp, raw,
//       [](const char* cmd, size_t len, void*) {
//           uart_write_bytes(HMI_UART_NUM, cmd, len);  // komut gövdesi
//           HMI_sendEndBytes();                         // 0xFF 0xFF 0xFF
//       }, nullptr);
//
// (Değişiklik-önbellekli gönderim isteniyorsa callback içinde HMIHelpers'taki
//  HMI_sendNumericIfChanged/HMI_sendTextIfChanged kalıbı uygulanabilir.)
// ---------------------------------------------------------------------------
//
// Donanım/IDF bağımlılığı YOKTUR — saf C++17.
//
#include <cstddef>
#include <cstdint>

#include "BmsComputed.h"
#include "BmsModel.h"

// Her bir tam Nextion komutu için çağrılır. `cmd` NUL-sonlu, `len` = strlen.
// `ctx` çağırana ait opak işaretçi (test buffer'ı, UART numarası vb.).
using BmsNextionEmit = void (*)(const char* cmd, size_t len, void* ctx);

// BmsComputed + ham BmsPackData'dan tüm Nextion komutlarını üretir ve sırayla
// `emit` callback'ine verir. Üretilen komutlar (gövde, end-byte HARİÇ):
//   - cell0.val=<mV> .. cell23.val=<mV>     (her hücre gerilimi, number)
//   - j0.val=<0..100> .. j23.val=<0..100>   (her hücre bar doluluğu, progress bar)
//   - bal0.val=<0|1> .. bal23.val=<0|1>     (dengeleme bayrakları)
//   - cellmax.val / cellmin.val             (uç hücre gerilimleri; ŞİMDİLİK DEMO/
//                                            sim. Gerçek zamanlıya geçişte bu iki
//                                            emit kaldırılır, cellmax/cellmin
//                                            updateScreen'den (BMS_USE_REALTIME_
//                                            MINMAX) gerçek BMS verisiyle sürülür)
//   - warn.val=<0|1|2>                      (uyarı seviyesi, sayısal)
// emit nullptr ise hiçbir şey yapılmaz.
struct BmsNextionCache {
    uint16_t cellVoltageMv[BMS_CELL_COUNT];
    uint8_t cellBarFill[BMS_CELL_COUNT];
    uint8_t balanceFlag[BMS_CELL_COUNT];
    uint16_t cellMaxMv = 0;
    uint16_t cellMinMv = 0;
    uint8_t warningLevel = 255;
    bool isWarm = false;

    BmsNextionCache() {
        for (int i = 0; i < BMS_CELL_COUNT; ++i) {
            cellVoltageMv[i] = 0;
            cellBarFill[i] = 255;
            balanceFlag[i] = 255;
        }
    }
};

void buildBmsNextionCommands(const BmsComputed& comp, const BmsPackData& raw,
                             BmsNextionEmit emit, void* ctx,
                             BmsNextionCache& cache, bool forceFullRefresh, bool updateCells,
                             size_t maxBytes = 90);

// ---------------------------------------------------------------------------
// BMS panel round-robin resync — slot invalidasyonu (SAF).
// ---------------------------------------------------------------------------
// Skalar alanların resync katmanı gibi (bkz. lib/DisplayHMI/ResyncPolicy.h
// "SORUN/ÇÖZÜM"), 24 hücrelik panel de tespit EDİLEMEYEN bir Nextion reset'i
// (Startup event'i brown-out'ta RX hattında kaybolursa) sonrasında kalıcı
// yarı-dolu kalabilir. Emniyet katmanı: her BMS_RESYNC_INTERVAL_MS'te bir
// SIRADAKİ TEK slot'un cache girdileri, üretimde OLUŞAMAYACAK değerlerle
// geçersiz kılınır; mevcut change-compare yolu o slot'u bir sonraki uygun
// tikte (hücre slotları için updateCells ≤ 1 sn gecikmeyle) yeniden yayar.
//
// Zorla-gönder bayrağı YERİNE invalidasyon seçildi: invalidasyon YAPIŞKANDIR
// — maxBytes bütçesi o tikte tükenirse cache uyuşmazlığı (ve isWarm=false
// mekanizması) sonraki tiklere taşınır, resync KAYBOLMAZ.
//
// Slot haritası:
//   0..BMS_CELL_COUNT-1 → hücre i üçlüsü (cellN.val + jN.val + balN.val)
//   BMS_RESYNC_SLOT_CELLMAX / _CELLMIN / _WARN → özet alanlar
constexpr uint8_t BMS_RESYNC_SLOT_CELLMAX = BMS_CELL_COUNT;      // 24
constexpr uint8_t BMS_RESYNC_SLOT_CELLMIN = BMS_CELL_COUNT + 1;  // 25
constexpr uint8_t BMS_RESYNC_SLOT_WARN    = BMS_CELL_COUNT + 2;  // 26
constexpr uint8_t BMS_RESYNC_SLOT_COUNT   = BMS_CELL_COUNT + 3;  // 27

// Hücre mV invalidasyon sentineli: parser çıktısı raw16/10 ≤ 6553 mV, "veri
// yok" sentineli 65535 (HMI_CELL_VOLTAGE_NO_DATA) — 65534 üretimde OLUŞAMAZ,
// her gerçek değerle uyuşmazlık garantidir. (Savunma derinliği: hücre slotu
// ayrıca bar=255 ve bal=255 ile de invalidalanır; ikisi de geçerli aralık
// dışıdır, mV sentineli bir gün çakışsa bile bar/bal yine yeniden yayılır.)
constexpr uint16_t BMS_RESYNC_MV_INVALID = 65534;

inline void bmsNextionCacheInvalidateSlot(BmsNextionCache& cache,
                                          uint8_t slot) {
    if (slot < BMS_CELL_COUNT) {
        cache.cellVoltageMv[slot] = BMS_RESYNC_MV_INVALID;
        cache.cellBarFill[slot] = 255;   // geçerli aralık 0..100
        cache.balanceFlag[slot] = 255;   // geçerli değerler 0/1
    } else if (slot == BMS_RESYNC_SLOT_CELLMAX) {
        cache.cellMaxMv = BMS_RESYNC_MV_INVALID;
    } else if (slot == BMS_RESYNC_SLOT_CELLMIN) {
        cache.cellMinMv = BMS_RESYNC_MV_INVALID;
    } else if (slot == BMS_RESYNC_SLOT_WARN) {
        cache.warningLevel = 255;        // geçerli seviyeler 0/1/2
    }
}
