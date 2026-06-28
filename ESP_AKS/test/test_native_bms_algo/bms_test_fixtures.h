#pragma once
// BmsAlgo native testleri için ortak fixture'lar.
// makeUniformPack(): tüm hücreler eşit, güvenli bantta, geçerli veri.
// Tek bir alanı edit ederek eşik kenarlarını izole etmek kolaylaşsın diye
// tüm değerler nominal seçilir (denge yok, uyarı yok).
#include "BmsModel.h"

namespace bms_fixtures {

// Tüm hücreler `mv` mV, tüm sıcaklıklar `tempC` °C, geçerli veri.
inline BmsPackData makeUniformPack(uint16_t mv = 3700, int16_t tempC = 25) {
    BmsPackData p{};
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        p.cellVoltageMv[i] = mv;
        p.cellTempC[i] = tempC;
    }
    p.packCurrentMa = 0;
    p.isValid = true;
    return p;
}

}  // namespace bms_fixtures
