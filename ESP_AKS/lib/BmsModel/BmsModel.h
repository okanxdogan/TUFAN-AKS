#pragma once
//
// BmsModel.h — 24 hücreli batarya paketinin PAYLAŞILAN veri sözleşmesi.
//
// Bu header, "Sahte Veri Üretici" (simülatör) ile "Ana Denetleyici (MCU)
// algoritması" arasındaki sınırı (HAL — Hardware Abstraction Layer) tanımlar.
// Amaç: ana kart, veriyi simülatörden mi yoksa gerçek BMS'ten mi aldığını
// AYIRT EDEMESİN. Her iki kaynak da ICellDataSource'u birebir aynı şekilde
// doldurur.
//
// Donanım/IDF bağımlılığı YOKTUR — saf C++17. Hem ESP32 firmware'inde hem de
// native (host) Unity testlerinde derlenebilir.
//
#include <cstdint>

// --- Paket geometrisi ---
static constexpr uint8_t BMS_CELL_COUNT = 24;  // Seri hücre sayısı

// --- Tek paket anlık görüntüsü (ham, yorumlanmamış değerler) ---
// Transport-agnostik: UART/CAN/SPI veya simülatör fark etmez, bu struct aynı
// anlamla doldurulur. Yorumlama (SoC, denge, eşik) MCU algoritmasının işidir.
struct BmsPackData {
    uint16_t cellVoltageMv[BMS_CELL_COUNT];  // Hücre başına gerilim, milivolt
                                             // (örn. 3700 = 3.700 V)
    int16_t cellTempC[BMS_CELL_COUNT];       // Hücre başına sıcaklık, °C
    int32_t packCurrentMa;                   // Paket akımı, mA
                                             // (+ şarj / − deşarj)
    bool isValid;                            // false => taze/geçerli veri yok
};

// --- HAL arayüzü: veri kaynağı sözleşmesi ---
// SimCellDataSource (yalancı veri) ve RealCellDataSource (gerçek BMS) bunu
// birebir aynı şekilde implemente eder. MCU yalnızca bu arayüzü tanır.
class ICellDataSource {
   public:
    virtual ~ICellDataSource() = default;

    // Kaynağı başlatır (donanım init, simülatörde no-op olabilir).
    // Başarıda true.
    virtual bool begin() = 0;

    // En güncel anlık görüntüyü `out`a yazar.
    // Taze veri yoksa false döner ve out.isValid=false olur.
    virtual bool read(BmsPackData& out) = 0;
};
