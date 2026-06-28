//
// SimCellDataSource.cpp — yalancı veri üretim mantığı.
// Saf C++17, donanım bağımlılığı yok.
//
#include "SimCellDataSource.h"

namespace {

// --- Senaryo sabitleri (mV / °C / mA) ---
constexpr uint16_t kNormalNominalMv = 3650;  // Senaryo A nominal hücre gerilimi
constexpr uint16_t kNormalJitterMv = 50;     // ± dalgalanma genliği

constexpr uint16_t kUnbalancedBaseMv = 3800;  // Senaryo B çoğunluk hücre
constexpr uint16_t kUnbalancedHighMv = 4150;  // Senaryo B kaçak hücre (index 6)
constexpr uint8_t kUnbalancedCellIdx = 6;     // 7. hücre (0 tabanlı index 6)

constexpr uint16_t kDangerUnderBaseMv = 3700;  // Senaryo C-1 normal hücreler
constexpr uint16_t kDangerUnderLowMv = 2800;   // Senaryo C-1 çöken hücre
constexpr uint8_t kDangerUnderCellIdx = 11;    // çöken hücre konumu

constexpr uint16_t kDangerOverNominalMv = 3650;  // Senaryo C-2 gerilimleri normal
constexpr int16_t kDangerOverHotTempC = 70;      // aşırı ısınan hücre °C
constexpr uint8_t kDangerOverCellIdx = 3;        // ısınan hücre konumu

// Normal çalışma sıcaklığı / akımı (her senaryoda taban olarak kullanılır).
constexpr int16_t kNominalTempC = 30;
constexpr int16_t kTempJitterC = 2;
constexpr int32_t kDischargeCurrentMa = -5000;  // ~ -5 A deşarj

}  // namespace

SimCellDataSource::SimCellDataSource(SimScenario SIM_scenario)
    : SIM_scenario_(SIM_scenario),
      SIM_seed_(kDefaultSeed),
      SIM_lcgState_(kDefaultSeed),
      SIM_step_(0) {}

bool SimCellDataSource::begin() {
    // Simülatörde başlatılacak donanım yok; deterministik başlangıç için
    // sayaç ve LCG'yi sıfırla.
    reset();
    return true;
}

void SimCellDataSource::setScenario(SimScenario SIM_scenario) {
    SIM_scenario_ = SIM_scenario;
}

void SimCellDataSource::setSeed(uint32_t SIM_seed) {
    SIM_seed_ = SIM_seed;
    SIM_lcgState_ = SIM_seed;
    SIM_step_ = 0;
}

void SimCellDataSource::reset() {
    SIM_lcgState_ = SIM_seed_;
    SIM_step_ = 0;
}

// Numerical Recipes LCG sabitleri. Üst bitler alt bitlerden daha "rastgele"
// olduğu için 16 bit kaydırıp span'e indirgiyoruz. Dönüş [-span, +span].
uint16_t SimCellDataSource::nextJitterMv(uint16_t SIM_spanMv) {
    SIM_lcgState_ = SIM_lcgState_ * 1664525u + 1013904223u;
    if (SIM_spanMv == 0) {
        return 0;
    }
    // 0 .. (2*span) aralığına indirip span çıkararak [-span, +span] yap.
    uint32_t SIM_range = static_cast<uint32_t>(SIM_spanMv) * 2u + 1u;
    uint32_t SIM_pick = (SIM_lcgState_ >> 16) % SIM_range;
    // İşaretli ofseti uint16_t içinde taşımadan döndürmek için çağıran taraf
    // bunu int aritmetiğiyle ekler; burada (pick - span) işaretli olabilir.
    return static_cast<uint16_t>(SIM_pick);  // 0 .. 2*span
}

bool SimCellDataSource::read(BmsPackData& out) {
    // Önce tüm alanları makul "taban" değerlerle doldur, sonra senaryoya göre
    // üzerine yaz. Böylece her senaryo tam bir paket görüntüsü üretir.
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
        // Sıcaklık: ~30 °C ± 2, deterministik küçük dalgalanma.
        int16_t SIM_tJit =
            static_cast<int16_t>(nextJitterMv(kTempJitterC)) - kTempJitterC;
        out.cellTempC[i] = static_cast<int16_t>(kNominalTempC + SIM_tJit);
    }
    out.packCurrentMa = kDischargeCurrentMa;
    out.isValid = true;

    switch (SIM_scenario_) {
        case SimScenario::NORMAL: {
            // Senaryo A: her hücre 3650 mV ± 50 mV.
            for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                int16_t SIM_jit =
                    static_cast<int16_t>(nextJitterMv(kNormalJitterMv)) -
                    static_cast<int16_t>(kNormalJitterMv);
                out.cellVoltageMv[i] =
                    static_cast<uint16_t>(kNormalNominalMv + SIM_jit);
            }
            break;
        }

        case SimScenario::UNBALANCED: {
            // Senaryo B: 23 hücre ~3800 mV, index 6 ~4150 mV (denge tetikleyici).
            for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                if (i == kUnbalancedCellIdx) {
                    out.cellVoltageMv[i] = kUnbalancedHighMv;
                } else {
                    out.cellVoltageMv[i] = kUnbalancedBaseMv;
                }
            }
            break;
        }

        case SimScenario::DANGER_UNDERVOLT: {
            // Senaryo C-1: bir hücre 2800 mV'a düşer (düşük gerilim alarmı).
            for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                if (i == kDangerUnderCellIdx) {
                    out.cellVoltageMv[i] = kDangerUnderLowMv;
                } else {
                    out.cellVoltageMv[i] = kDangerUnderBaseMv;
                }
            }
            break;
        }

        case SimScenario::DANGER_OVERTEMP: {
            // Senaryo C-2: gerilimler normal, bir hücre ~70 °C'ye ısınır.
            for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i) {
                out.cellVoltageMv[i] = kDangerOverNominalMv;
            }
            out.cellTempC[kDangerOverCellIdx] = kDangerOverHotTempC;
            break;
        }
    }

    ++SIM_step_;
    return true;
}
