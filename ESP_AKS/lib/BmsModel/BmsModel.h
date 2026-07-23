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
    int16_t cellTempC[BMS_CELL_COUNT];       // Hücre başına sıcaklık, °C —
                                             // BMS per-hücre sıcaklık YAYINLAMIYOR;
                                             // gerçek kaynak aşağıdaki packTemp*
                                             // alanlarıdır. computePack min/max
                                             // sıcaklığı artık BU DİZİDEN DEĞİL
                                             // packTempMaxC/MinC'den alır.
    int32_t packCurrentMa;                   // Paket akımı, mA
                                             // (+ şarj / − deşarj)

    // Paket seviyesi sıcaklık (0xE001 byte[6:7] → TEL_bmsTempHighestC/LowestC,
    // DOĞRULANDI). main.cpp HMI task doldurur; computePack tMax/tMin ve
    // sıcaklık uyarı kararını (55/70, >= semantiği) buradan hesaplar —
    // hücre-başına sahte sıcaklık kopyalama YOKTUR.
    int16_t packTempMaxC = 0;
    int16_t packTempMinC = 0;

    // BYS'nin KENDİ raporladığı hücre min/max özeti (0xE001 byte[0:1]=min,
    // byte[2:3]=max → mV'ye yuvarlanmış). Ekrandaki ÖZET min/max bundan sürülür
    // (şartname B3 6.c: min/max BYS raporundan, 24 hücre tazeliğinden bağımsız).
    // main.cpp HMI task doldurur. İKİSİ DE 0 => kaynak yok, computePack 24'lük
    // taramaya FALLBACK yapar. 24'lük bar paneli (cell0..23) her zaman
    // cellVoltageMv[]'den beslenir — buna dokunulmaz.
    uint16_t bmsReportedCellMaxMv = 0;  // 0xE001 byte[2:3], mV (0 = kaynak yok)
    uint16_t bmsReportedCellMinMv = 0;  // 0xE001 byte[0:1], mV (0 = kaynak yok)

    bool isValid;                            // false => taze/geçerli veri yok

    // G8/M4 FIX: Hücre kaynağı artık DOĞRULANDI (E015-E020).
    // main.cpp'deki HMI task bu alanı gerçek veriyle dolduruyor.
    bool cellDataValid = true;
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
