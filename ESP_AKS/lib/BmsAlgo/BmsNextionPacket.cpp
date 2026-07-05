#include "BmsNextionPacket.h"

#include <cstdio>

#include "BmsAlgo.h"  // BMS_SOC_EMPTY_MV / BMS_SOC_FULL_MV — tek kaynak

// Saf C++; snprintf dışında bağımlılık yok. UART çağrısı YAPMAZ.

namespace {

// Hücre gerilimini (mV) progress bar doluluğuna (0..100) çevirir. Aralık
// BMS_SOC_EMPTY_MV..BMS_SOC_FULL_MV (BmsAlgo.h, tek kaynak — LiFePO4 spec
// 2.50-3.65 V/hücre); dışı 0/100'e clamp'lenir.
uint8_t cellBarFill(uint16_t mv) {
    if (mv == 65535) return 0; // Sentinel value (HMI_CELL_VOLTAGE_NO_DATA) -> clear bars
    if (mv <= BMS_SOC_EMPTY_MV) return 0;
    if (mv >= BMS_SOC_FULL_MV) return 100;
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(mv - BMS_SOC_EMPTY_MV) * 100u) /
        (BMS_SOC_FULL_MV - BMS_SOC_EMPTY_MV));
}

}  // namespace

void buildBmsNextionCommands(const BmsComputed& comp, const BmsPackData& raw,
                             BmsNextionEmit emit, void* ctx,
                             BmsNextionCache& cache, bool forceFullRefresh, bool updateCells,
                             size_t maxBytes) {
    if (emit == nullptr) return;

    size_t currentBytes = 0;
    bool budgetExhausted = false;

    // Helper lambda to format, check budget, and emit
    auto emitAndCheck = [&](const char* compName, int index, int32_t value) -> bool {
        if (budgetExhausted) return false;
        char cmd[40];
        int len;
        if (index >= 0) {
            len = snprintf(cmd, sizeof(cmd), "%s%u.val=%ld", compName, static_cast<unsigned>(index), static_cast<long>(value));
        } else {
            len = snprintf(cmd, sizeof(cmd), "%s.val=%ld", compName, static_cast<long>(value));
        }
        
        if (len <= 0) return false;
        
        // +3 bytes for the 0xFF 0xFF 0xFF end sequence appended by the caller
        if (currentBytes + static_cast<size_t>(len) + 3 > maxBytes) {
            budgetExhausted = true;
            return false;
        }
        
        emit(cmd, static_cast<size_t>(len), ctx);
        currentBytes += static_cast<size_t>(len) + 3;
        return true;
    };

    bool allProcessed = true;
    bool mustProcessCells = updateCells || forceFullRefresh || !cache.isWarm;

    if (mustProcessCells) {
        // Hücre gerilimleri: cell0..cell23
        for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
            if (forceFullRefresh || raw.cellVoltageMv[i] != cache.cellVoltageMv[i]) {
                if (!emitAndCheck("cell", i, static_cast<int32_t>(raw.cellVoltageMv[i]))) {
                    allProcessed = false;
                    continue;
                }
                cache.cellVoltageMv[i] = raw.cellVoltageMv[i];
            }
        }

        // Hücre bar doluluğu: j0..j23
        for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
            uint8_t fill = cellBarFill(raw.cellVoltageMv[i]);
            if (forceFullRefresh || fill != cache.cellBarFill[i]) {
                if (!emitAndCheck("j", i, fill)) {
                    allProcessed = false;
                    continue;
                }
                cache.cellBarFill[i] = fill;
            }
        }

        // Dengeleme bayrakları: bal0..bal23
        for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
            uint8_t bal = comp.balanceFlag[i] ? 1 : 0;
            if (forceFullRefresh || bal != cache.balanceFlag[i]) {
                if (!emitAndCheck("bal", i, bal)) {
                    allProcessed = false;
                    continue;
                }
                cache.balanceFlag[i] = bal;
            }
        }
    }

    // Özet alanlar
    if (forceFullRefresh || comp.cellMaxMv != cache.cellMaxMv) {
        if (emitAndCheck("cellmax", -1, comp.cellMaxMv)) {
            cache.cellMaxMv = comp.cellMaxMv;
        } else {
            allProcessed = false;
        }
    }
    
    if (forceFullRefresh || comp.cellMinMv != cache.cellMinMv) {
        if (emitAndCheck("cellmin", -1, comp.cellMinMv)) {
            cache.cellMinMv = comp.cellMinMv;
        } else {
            allProcessed = false;
        }
    }

    if (forceFullRefresh || comp.warningLevel != cache.warningLevel) {
        if (emitAndCheck("warn", -1, comp.warningLevel)) {
            cache.warningLevel = comp.warningLevel;
        } else {
            allProcessed = false;
        }
    }

    if (allProcessed) {
        cache.isWarm = true;
    }
}
