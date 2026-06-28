#pragma once
//
// SimCellDataSource — 24 hücreli batarya paketi için YALANCI (sahte) veri
// üreteci.  ICellDataSource sözleşmesini implemente eder; ana kart bu veriyi
// gerçek BMS'ten mi yoksa simülatörden mi aldığını AYIRT EDEMEZ (HAL kuralı).
//
// Saf C++17 — donanım/IDF bağımlılığı YOKTUR. Native (host) Unity testlerinde
// ve ESP32 firmware'inde birebir aynı şekilde derlenir/çalışır.
//
// Tekrarlanabilirlik: rastgelelik <random> yerine sabit tohumlu basit bir LCG
// ile üretilir. setSeed()/setScenario() ile başlatılan iki örnek, aynı sayıda
// read() çağrısından sonra BİREBİR aynı çıktıyı verir.
//
#include <cstdint>

#include "BmsModel.h"

// Simülasyon senaryoları — ana kartın karşılaşması beklenen durumlar.
enum class SimScenario : uint8_t {
    NORMAL = 0,           // Senaryo A: tüm hücreler ~3650 mV, dengeli
    UNBALANCED = 1,       // Senaryo B: index 6 ~4150 mV, diğerleri ~3800 mV
    DANGER_UNDERVOLT = 2, // Senaryo C-1: bir hücre 2800 mV'a düşer
    DANGER_OVERTEMP = 3,  // Senaryo C-2: bir hücre ~70 °C'ye ısınır
};

class SimCellDataSource : public ICellDataSource {
   public:
    explicit SimCellDataSource(SimScenario SIM_scenario = SimScenario::NORMAL);

    // ICellDataSource sözleşmesi -----------------------------------------
    // Simülatörde donanım yok; her zaman true.
    bool begin() override;

    // Aktif senaryoya göre bir anlık görüntü üretir, dahili sayacı ilerletir.
    // out.isValid daima true. Daima true döner.
    bool read(BmsPackData& out) override;

    // Simülasyon kontrolü ------------------------------------------------
    // Aktif senaryoyu değiştirir. Dahili adım sayacını sıfırlamaz (akış
    // sürekli kalsın); deterministik testler için reset() çağırın.
    void setScenario(SimScenario SIM_scenario);
    SimScenario scenario() const { return SIM_scenario_; }

    // LCG tohumunu ve adım sayacını başlangıç durumuna döndürür. Aynı tohum +
    // aynı senaryo + aynı sayıda read() => birebir aynı çıktı.
    void setSeed(uint32_t SIM_seed);
    void reset();  // tohumu başlangıca, adım sayacını 0'a çeker

   private:
    // Sabit tohumlu Lineer Eşlenik Üreteç (LCG) — <random> yerine, çünkü
    // testlerin platformdan bağımsız ve tekrarlanabilir olması gerekiyor.
    uint16_t nextJitterMv(uint16_t SIM_spanMv);  // [-span, +span] aralığı

    static constexpr uint32_t kDefaultSeed = 0xA53C2718u;

    SimScenario SIM_scenario_;
    uint32_t    SIM_seed_;     // sıfırlama için saklanan başlangıç tohumu
    uint32_t    SIM_lcgState_; // LCG'nin yürüyen durumu
    uint32_t    SIM_step_;     // read() çağrı sayacı (deterministik faz için)
};
