#include "BmsNextionPacket.h"

#include <cstdio>

#include "BmsAlgo.h"  // BMS_SOC_EMPTY_MV / BMS_SOC_FULL_MV — tek kaynak

// Saf C++; snprintf dışında bağımlılık yok. UART çağrısı YAPMAZ.

namespace {

// "<comp>.val=<value>" komutunu formatla ve emit et. Sayısal Nextion alanı.
void emitNumeric(BmsNextionEmit emit, void* ctx, const char* comp,
                 int32_t value) {
    char cmd[40];
    const int len =
        snprintf(cmd, sizeof(cmd), "%s.val=%ld", comp, static_cast<long>(value));
    if (len <= 0) return;
    emit(cmd, static_cast<size_t>(len), ctx);
}

// "<comp>N.val=<value>" — indeksli sayısal alan (cellN, balN).
void emitIndexedNumeric(BmsNextionEmit emit, void* ctx, const char* comp,
                        uint8_t index, int32_t value) {
    char cmd[40];
    const int len = snprintf(cmd, sizeof(cmd), "%s%u.val=%ld", comp,
                             static_cast<unsigned>(index),
                             static_cast<long>(value));
    if (len <= 0) return;
    emit(cmd, static_cast<size_t>(len), ctx);
}

// Hücre gerilimini (mV) progress bar doluluğuna (0..100) çevirir. Aralık
// BMS_SOC_EMPTY_MV..BMS_SOC_FULL_MV (BmsAlgo.h, tek kaynak — LiFePO4 spec
// 2.50-3.65 V/hücre); dışı 0/100'e clamp'lenir.
uint8_t cellBarFill(uint16_t mv) {
    if (mv <= BMS_SOC_EMPTY_MV) return 0;
    if (mv >= BMS_SOC_FULL_MV) return 100;
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(mv - BMS_SOC_EMPTY_MV) * 100u) /
        (BMS_SOC_FULL_MV - BMS_SOC_EMPTY_MV));
}

}  // namespace

void buildBmsNextionCommands(const BmsComputed& comp, const BmsPackData& raw,
                             BmsNextionEmit emit, void* ctx) {
    if (emit == nullptr) return;

    // Hücre gerilimleri: cell0..cell23
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        emitIndexedNumeric(emit, ctx, "cell", i,
                           static_cast<int32_t>(raw.cellVoltageMv[i]));
    }

    // Hücre bar doluluğu: j0..j23 (0..100) — number cell0.. ile aynı veriden.
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        emitIndexedNumeric(emit, ctx, "j", i, cellBarFill(raw.cellVoltageMv[i]));
    }

    // Dengeleme bayrakları: bal0..bal23  (0/1)
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        emitIndexedNumeric(emit, ctx, "bal", i, comp.balanceFlag[i] ? 1 : 0);
    }

    // Özet alanlar (uç hücre gerilimleri). delta/soc/bmspackv/tmax/tmin demo'dan
    // ÇIKARILDI: bunlar ana ekranda gerçek veriyle (bat/packv/temp) zaten var ya
    // da demoya özel ve gereksiz. computePack bu değerleri hesaplamaya devam eder,
    // yalnızca ekrana gönderilmez.
    emitNumeric(emit, ctx, "cellmax", comp.cellMaxMv);
    emitNumeric(emit, ctx, "cellmin", comp.cellMinMv);

    // Uyarı: sayısal seviye (renk/animasyon için)
    emitNumeric(emit, ctx, "warn", comp.warningLevel);
}
