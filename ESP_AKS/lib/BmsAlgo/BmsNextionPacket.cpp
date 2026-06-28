#include "BmsNextionPacket.h"

#include <cstdio>

// Saf C++; snprintf dışında bağımlılık yok. UART çağrısı YAPMAZ.

const char* bmsWarningText(uint8_t warningLevel) {
    switch (warningLevel) {
        case BMS_WARN_OK:       return "OK";
        case BMS_WARN_WARNING:  return "WARN";
        case BMS_WARN_CRITICAL: return "CRIT";
        default:                return "UNK";
    }
}

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

// "<comp>.txt=\"<value>\"" — metin alanı.
void emitText(BmsNextionEmit emit, void* ctx, const char* comp,
              const char* value) {
    char cmd[48];
    const int len =
        snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", comp, value);
    if (len <= 0) return;
    emit(cmd, static_cast<size_t>(len), ctx);
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

    // Dengeleme bayrakları: bal0..bal23  (0/1)
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        emitIndexedNumeric(emit, ctx, "bal", i, comp.balanceFlag[i] ? 1 : 0);
    }

    // Özet alanlar
    emitNumeric(emit, ctx, "delta", comp.cellDeltaMv);
    emitNumeric(emit, ctx, "soc", comp.socPercent);
    emitNumeric(emit, ctx, "packv", comp.packVoltageMv);
    emitNumeric(emit, ctx, "cellmax", comp.cellMaxMv);
    emitNumeric(emit, ctx, "cellmin", comp.cellMinMv);
    emitNumeric(emit, ctx, "tmax", comp.tempMaxC);
    emitNumeric(emit, ctx, "tmin", comp.tempMinC);

    // Uyarı: hem sayısal (renk/animasyon için) hem metin
    emitNumeric(emit, ctx, "warn", comp.warningLevel);
    emitText(emit, ctx, "warntxt", bmsWarningText(comp.warningLevel));
}
