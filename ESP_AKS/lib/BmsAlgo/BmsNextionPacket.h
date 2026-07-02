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
void buildBmsNextionCommands(const BmsComputed& comp, const BmsPackData& raw,
                             BmsNextionEmit emit, void* ctx);
