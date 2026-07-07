#include <unity.h>

#include "BmsAlgo.h"
#include "BmsNextionPacket.h"
#include "fake_nextion_emit.h"

// ===========================================================================
// G8/M4 — Hücre kaynağı doğrulama kapısı (cellDataValid).
//
// SORUN: pack voltajı 24'e bölünüp tüm hücrelere yazılınca gerçek bir hücre
// dengesizliği (tek hücre 2.3 V) ekranda SAĞLIKLI görünüyordu. computePack
// artık cellDataValid=false iken dengeleme/uyarıyı HESAPLAMAZ; "veri yok"
// (NO_DATA, nötr) döndürür. cellDataValid=true iken gerçek hesap çalışır.
// ===========================================================================

namespace {

// Gerçekçi dengesiz paket: hücre[0] 2300 mV (kritik altı), diğerleri 3300 mV.
// Doğrulanmış veri sayılırsa CRITICAL + dengeleme tetiklenmeli; doğrulanmamışsa
// bu dengesizlik HİÇ değerlendirilmemeli (NO_DATA).
BmsPackData makeImbalancedPack() {
    BmsPackData d{};
    d.isValid = true;
    d.cellVoltageMv[0] = 2300;  // çökük hücre
    for (uint8_t i = 1; i < BMS_CELL_COUNT; ++i)
        d.cellVoltageMv[i] = 3300;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i)
        d.cellTempC[i] = 25;
    return d;
}

}  // namespace

// --- verified=false → per-hücre çıktıların hepsi nötr/sentinel ---------------

// Paket geçerli ama hücre kaynağı doğrulanmamış: dengeleme YOK, uyarı NO_DATA.
void test_unverified_cell_source_returns_no_data(void) {
    BmsPackData d = makeImbalancedPack();
    d.cellDataValid = false;  // hücre kaynağı DOĞRULANMADI

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_NO_DATA, c.warningLevel);
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i)
        TEST_ASSERT_FALSE(c.balanceFlag[i]);  // hiçbir hücre dengelenmez
    TEST_ASSERT_EQUAL_UINT8(0, c.socPercent);  // ortalamadan SoC de fabrikasyon olurdu
    TEST_ASSERT_EQUAL_UINT16(0, c.cellDeltaMv);
}

// REGRESYON: aynı dengesiz paket doğrulanmamışken ASLA CRITICAL/"sağlıklı"
// üretmez — yalanci güven de yalancı alarm da yok, yalnız NO_DATA.
void test_unverified_imbalance_is_not_masked_as_healthy_nor_critical(void) {
    BmsPackData d = makeImbalancedPack();
    d.cellDataValid = false;

    BmsComputed c = computePack(d);

    TEST_ASSERT_NOT_EQUAL(BMS_WARN_OK, c.warningLevel);        // "sağlıklı" DEĞİL
    TEST_ASSERT_NOT_EQUAL(BMS_WARN_CRITICAL, c.warningLevel);  // yalancı alarm DEĞİL
    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_NO_DATA, c.warningLevel);
}

// --- verified=true → gerçek hesap (dengesizlik artık maskelenmiyor) ----------

// Aynı paket DOĞRULANMIŞ sayılırsa: çökük hücre CRITICAL üretir ve gerçek
// min/max hesaplanır. (Eskiden ortalama fabrikasyonu bunu gizliyordu.)
void test_verified_cell_source_detects_imbalance(void) {
    BmsPackData d = makeImbalancedPack();
    d.cellDataValid = true;  // doğrulanmış (varsayılan da true)

    BmsComputed c = computePack(d);

    TEST_ASSERT_EQUAL_UINT8(BMS_WARN_CRITICAL, c.warningLevel);  // 2300 < 2500 undervolt
    TEST_ASSERT_EQUAL_UINT16(2300, c.cellMinMv);
    TEST_ASSERT_EQUAL_UINT16(3300, c.cellMaxMv);
    TEST_ASSERT_EQUAL_UINT8(0, c.cellMinIndex);
}

// cellDataValid VARSAYILANI true olmalı (mevcut gerçek/sim kaynaklar hücreleri
// dürüstçe doldurur; yalnız doğrulanmamış yollar false yapar).
void test_cell_data_valid_defaults_true(void) {
    BmsPackData d{};
    TEST_ASSERT_TRUE(d.cellDataValid);
}

// --- Nextion boru hattı: doğrulanmamış yol sentinel + warn=NO_DATA üretir ----

// main.cpp'nin doğrulanmamış yolunu birebir taklit et: tüm hücreler sentinel,
// cellDataValid=false → computePack NO_DATA. buildBmsNextionCommands: hücreler
// "--" (65535), barlar boş (0), dengeleme 0, warn=3.
void test_unverified_pipeline_emits_sentinels_and_no_data_warn(void) {
    fake_nextion_reset();

    BmsPackData raw{};
    raw.isValid = true;
    raw.cellDataValid = false;
    for (uint8_t i = 0; i < BMS_CELL_COUNT; ++i)
        raw.cellVoltageMv[i] = 65535;  // HMI_CELL_VOLTAGE_NO_DATA

    BmsComputed comp = computePack(raw);
    // main.cpp cellMax/Min'i sentinel'e override eder (per-hücre kaynak yok).
    comp.cellMaxMv = 65535;
    comp.cellMinMv = 65535;

    BmsNextionCache cache{};
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache,
                            /*forceFullRefresh=*/true, /*updateCells=*/true, 2000);

    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("cell0.val=65535"),
                                 "hücre gerilimi sentinel olmalı (--)");
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("j0.val=0"),
                                 "bar boş olmalı");
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("bal0.val=0"),
                                 "dengeleme kapalı olmalı");
    TEST_ASSERT_NOT_NULL_MESSAGE(fake_nextion_find("warn.val=3"),
                                 "uyarı NO_DATA (3) olmalı");
}
