#include <unity.h>

#include "BmsAlgo.h"

// ===========================================================================
// Ekran ÖZET min/max kaynağı = BYS'nin KENDİ raporu (0xE001 → BmsPackData
// .bmsReportedCellMax/MinMv). Şartname B3 6.c: min/max BYS raporundan
// gösterilir. 24'lük tarama (cellVoltageMv[]) yalnız FALLBACK'tir ve bar
// paneli + dengeleme/uyarı/indeks HÂLÂ taramadan gelir (REGRESYON KİLİDİ).
// ===========================================================================

namespace {

// Taramanın min=3300, max=3320, delta=20 (< 50 mV eşik → dengeleme KAPALI)
// vereceği; BYS raporunun ise min=3305, max=3400, delta=95 (> 50 mV eşik)
// olduğu paket. Delta'lar eşiğin İKİ YANINDA seçildi: dengeleme yanlışlıkla
// BYS raporunu okursa TETİKLENİR, taramayı okursa KAPALI kalır → ayırt edilir.
BmsPackData makePackWithScanAndReport() {
    BmsPackData d{};
    d.isValid = true;
    d.cellDataValid = true;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i)
        d.cellVoltageMv[i] = 3300;
    d.cellVoltageMv[7] = 3320;   // tarama max, index 7
    d.cellVoltageMv[3] = 3300;   // tarama min (ilk 3300, index 0)
    d.bmsReportedCellMaxMv = 3400;
    d.bmsReportedCellMinMv = 3305;
    return d;
}

}  // namespace

// --- bmsReported* dolu → özet min/max/delta ONLARDAN gelir -------------------
void test_reported_minmax_overrides_scan_summary(void) {
    BmsPackData d = makePackWithScanAndReport();

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT16(3400, c.cellMaxMv);   // BYS raporu, tarama 3320 DEĞİL
    TEST_ASSERT_EQUAL_UINT16(3305, c.cellMinMv);   // BYS raporu, tarama 3300 DEĞİL
    TEST_ASSERT_EQUAL_UINT16(95, c.cellDeltaMv);   // 3400-3305, tarama deltası 20 DEĞİL
}

// REGRESYON KİLİDİ: bmsReported* dolu OLSA BİLE cellMax/MinIndex tarama
// kaynaklı kalır (E001 indeks taşımaz).
void test_reported_minmax_keeps_scan_indices(void) {
    BmsPackData d = makePackWithScanAndReport();

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT8(7, c.cellMaxIndex);  // tarama max index
    TEST_ASSERT_EQUAL_UINT8(0, c.cellMinIndex);  // ilk 3300, index 0
}

// REGRESYON KİLİDİ: dengeleme kararı YALNIZ taramadan. Tarama deltası (20) eşik
// altındayken hiçbir hücre dengelenmez — BYS raporunun büyük deltası (45)
// dengelemeyi TETİKLEMEMELİ.
void test_reported_minmax_does_not_alter_balancing(void) {
    BmsPackData d = makePackWithScanAndReport();
    // Tarama deltası 20 mV; BMS_BALANCE_THRESHOLD_MV bunun üstünde olmalı ki
    // dengeleme kapalı kalsın (mevcut regresyon kilidi test_balance ile aynı
    // varsayım).
    BmsComputed c = computePack(d);

    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i)
        TEST_ASSERT_FALSE_MESSAGE(c.balanceFlag[i],
            "BYS raporu dengeleme davranisini DEGISTIRMEMELI");
}

// --- bmsReported* = 0 → tarama FALLBACK ------------------------------------
void test_zero_reported_falls_back_to_scan(void) {
    BmsPackData d = makePackWithScanAndReport();
    d.bmsReportedCellMaxMv = 0;
    d.bmsReportedCellMinMv = 0;

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT16(3320, c.cellMaxMv);   // tarama
    TEST_ASSERT_EQUAL_UINT16(3300, c.cellMinMv);   // tarama
    TEST_ASSERT_EQUAL_UINT16(20, c.cellDeltaMv);   // tarama deltası
}

// Yalnız BİRİ 0 ise (yarım kaynak) yine tarama fallback (ikisi de != 0 şartı).
void test_partial_zero_reported_falls_back_to_scan(void) {
    BmsPackData d = makePackWithScanAndReport();
    d.bmsReportedCellMinMv = 0;  // yalnız min 0

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT16(3320, c.cellMaxMv);   // tarama fallback
    TEST_ASSERT_EQUAL_UINT16(3300, c.cellMinMv);
}

// --- cellDataValid=false + bmsReported* dolu → KARAR: özet E001'den ---------
// warningLevel NO_DATA, balanceFlag[] tümü false, indeks 0 KALIR; yalnız
// cellMax/Min/Delta E001'den doldurulur.
void test_no_cell_data_but_reported_fills_summary_only(void) {
    BmsPackData d{};
    d.isValid = true;
    d.cellDataValid = false;      // 24 hücre henüz tam/taze değil
    d.bmsReportedCellMaxMv = 3400;
    d.bmsReportedCellMinMv = 3305;

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT16(3400, c.cellMaxMv);
    TEST_ASSERT_EQUAL_UINT16(3305, c.cellMinMv);
    TEST_ASSERT_EQUAL_UINT16(95, c.cellDeltaMv);
    // Bunlar makeNoCellData değerinde KALIR:
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_NO_DATA, c.warningLevel);
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i)
        TEST_ASSERT_FALSE(c.balanceFlag[i]);
    TEST_ASSERT_EQUAL_UINT8(0, c.cellMaxIndex);
    TEST_ASSERT_EQUAL_UINT8(0, c.cellMinIndex);
    TEST_ASSERT_EQUAL_UINT8(0, c.socPercent);
    TEST_ASSERT_EQUAL_UINT32(0, c.packVoltageMv);
}

// cellDataValid=false + bmsReported*=0 → mevcut davranış: üç alan da 0.
void test_no_cell_data_and_zero_reported_stays_zero(void) {
    BmsPackData d{};
    d.isValid = true;
    d.cellDataValid = false;

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT16(0, c.cellMaxMv);
    TEST_ASSERT_EQUAL_UINT16(0, c.cellMinMv);
    TEST_ASSERT_EQUAL_UINT16(0, c.cellDeltaMv);
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_NO_DATA, c.warningLevel);
}

// isValid=false → makeSafeInvalid AYNEN; bmsReported* dolu olsa bile
// hiçbir alana güvenilmez (pack bayat/arızalı).
void test_invalid_pack_ignores_reported_minmax(void) {
    BmsPackData d{};
    d.isValid = false;            // pack bayat/arızalı
    d.bmsReportedCellMaxMv = 3350;
    d.bmsReportedCellMinMv = 3305;

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);  // makeSafeInvalid
    TEST_ASSERT_EQUAL_UINT16(0, c.cellMaxMv);
    TEST_ASSERT_EQUAL_UINT16(0, c.cellMinMv);
    TEST_ASSERT_EQUAL_UINT16(0, c.cellDeltaMv);
}
