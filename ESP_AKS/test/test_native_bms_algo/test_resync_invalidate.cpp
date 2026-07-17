#include <unity.h>

#include <cstdio>
#include <cstring>

#include "BmsNextionPacket.h"
#include "ResyncPolicy.h"
#include "fake_nextion_emit.h"

// BMS panel round-robin resync — slot invalidasyonu testleri.
// bmsNextionCacheInvalidateSlot + buildBmsNextionCommands birlikte: tespit
// edilemeyen bir Nextion reset'i sonrasında (değerler DEĞİŞMEDEN) panelin
// kendini onarması bu ikiliye dayanır.

namespace {

// Sabit, gerçekçi bir pack durumu: tüm hücreler 3300 mV, dengeleme yok.
void makeSteadyPack(BmsPackData& raw, BmsComputed& comp) {
    raw = BmsPackData{};
    raw.isValid = true;
    for (int i = 0; i < BMS_CELL_COUNT; ++i) {
        raw.cellVoltageMv[i] = 3300;
    }
    comp = BmsComputed{};
    comp.cellMaxMv = 3300;
    comp.cellMinMv = 3300;
    comp.warningLevel = 0;
}

// Cache'i tek büyük-bütçeli çağrıyla ısıtır (her şey yayılmış, isWarm=true).
void warmCache(const BmsComputed& comp, const BmsPackData& raw,
               BmsNextionCache& cache) {
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache,
                            /*force*/ false, /*updateCells*/ true, 4096);
    fake_nextion_reset();
}

}  // namespace

// Isınmış cache + değişmeyen veri → hiçbir komut yayılmaz; hücre slotu
// invalidalanınca YALNIZCA o hücrenin üçlüsü (cell/j/bal) yeniden yayılır.
void test_resync_invalidate_cell_slot_reemits_triple(void) {
    BmsPackData raw;
    BmsComputed comp;
    makeSteadyPack(raw, comp);
    BmsNextionCache cache{};
    warmCache(comp, raw, cache);

    // Kontrol: değişiklik yokken sessiz.
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache,
                            false, true, 4096);
    TEST_ASSERT_EQUAL_size_t(0, fake_nextion_command_count());

    bmsNextionCacheInvalidateSlot(cache, 5);
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache,
                            false, true, 4096);

    TEST_ASSERT_NOT_NULL(fake_nextion_find("cell5.val=3300"));
    TEST_ASSERT_NOT_NULL(fake_nextion_find("j5.val="));
    TEST_ASSERT_NOT_NULL(fake_nextion_find("bal5.val=0"));
    TEST_ASSERT_EQUAL_size_t(3, fake_nextion_command_count());
}

// Özet slotları (cellmax/cellmin/warn) updateCells BEKLEMEDEN yeniden yayılır
// (özet alanlar build'in her çağrısında taranır).
void test_resync_invalidate_summary_slots_reemit(void) {
    BmsPackData raw;
    BmsComputed comp;
    makeSteadyPack(raw, comp);
    BmsNextionCache cache{};
    warmCache(comp, raw, cache);

    bmsNextionCacheInvalidateSlot(cache, BMS_RESYNC_SLOT_CELLMAX);
    bmsNextionCacheInvalidateSlot(cache, BMS_RESYNC_SLOT_CELLMIN);
    bmsNextionCacheInvalidateSlot(cache, BMS_RESYNC_SLOT_WARN);
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache,
                            false, /*updateCells*/ false, 4096);

    TEST_ASSERT_NOT_NULL(fake_nextion_find("cellmax.val=3300"));
    TEST_ASSERT_NOT_NULL(fake_nextion_find("cellmin.val=3300"));
    TEST_ASSERT_NOT_NULL(fake_nextion_find("warn.val=0"));
    TEST_ASSERT_EQUAL_size_t(3, fake_nextion_command_count());
}

// YAPIŞKANLIK: bütçe tükenip slot o tikte yayılamazsa resync KAYBOLMAZ —
// cache uyuşmazlığı sonraki tikte (normal bütçeyle) yayımı garanti eder.
void test_resync_invalidation_survives_budget_exhaustion(void) {
    BmsPackData raw;
    BmsComputed comp;
    makeSteadyPack(raw, comp);
    BmsNextionCache cache{};
    warmCache(comp, raw, cache);

    bmsNextionCacheInvalidateSlot(cache, 0);
    // Bütçe hiçbir komuta yetmeyecek kadar küçük → hiçbir şey yayılmaz.
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache,
                            false, true, 5);
    TEST_ASSERT_EQUAL_size_t(0, fake_nextion_command_count());

    // Sonraki tik, normal bütçe: slot hâlâ kirli → yayılır.
    buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr, cache,
                            false, true, 90);
    TEST_ASSERT_NOT_NULL(fake_nextion_find("cell0.val=3300"));
    TEST_ASSERT_NOT_NULL(fake_nextion_find("j0.val="));
    TEST_ASSERT_NOT_NULL(fake_nextion_find("bal0.val=0"));
}

// ANA GARANTİ: 10 Hz HMI task simülasyonu (updateCells 1 Hz, maxBytes=90,
// veri DEĞİŞMİYOR — yani yayım YALNIZCA resync'ten gelebilir). Tam tur
// (BMS_RESYNC_SLOT_COUNT × BMS aralığı) + kuyruk marjı içinde panelin 75
// komutunun HER BİRİ en az bir kez yeniden yayılır; her tikte 90 B bütçesi
// korunur.
void test_resync_full_rotation_covers_entire_panel(void) {
    BmsPackData raw;
    BmsComputed comp;
    makeSteadyPack(raw, comp);
    BmsNextionCache cache{};
    warmCache(comp, raw, cache);

    bool seenCell[BMS_CELL_COUNT] = {};
    bool seenBar[BMS_CELL_COUNT] = {};
    bool seenBal[BMS_CELL_COUNT] = {};
    bool seenMax = false, seenMin = false, seenWarn = false;

    uint32_t lastResync = 0;
    uint8_t nextSlot = 0;
    uint32_t lastCellUpdate = 0;
    const uint32_t interval = 1000;  // BMS_RESYNC_INTERVAL_MS varsayılanı
    const uint32_t horizon =
        (uint32_t)BMS_RESYNC_SLOT_COUNT * interval + 3000u;

    for (uint32_t now = 0; now <= horizon; now += 100) {
        const int slot = hmi_resync_due_field(now, lastResync, nextSlot,
                                              BMS_RESYNC_SLOT_COUNT, interval);
        if (slot >= 0) {
            bmsNextionCacheInvalidateSlot(cache, (uint8_t)slot);
        }

        bool updateCells = false;
        if (now - lastCellUpdate >= 1000) {
            updateCells = true;
            lastCellUpdate = now;
        }

        fake_nextion_reset();
        buildBmsNextionCommands(comp, raw, fake_nextion_capture, nullptr,
                                cache, false, updateCells, 90);

        size_t tickBytes = 0;
        for (size_t i = 0; i < fake_nextion_command_count(); ++i) {
            tickBytes += strlen(fake_nextion_command_at(i)) + 3;
        }
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(90, tickBytes,
                                          "tik bütçesi (90 B) aşıldı");

        char prefix[16];
        for (int i = 0; i < BMS_CELL_COUNT; ++i) {
            snprintf(prefix, sizeof(prefix), "cell%d.val=", i);
            if (fake_nextion_find(prefix)) seenCell[i] = true;
            snprintf(prefix, sizeof(prefix), "j%d.val=", i);
            if (fake_nextion_find(prefix)) seenBar[i] = true;
            snprintf(prefix, sizeof(prefix), "bal%d.val=", i);
            if (fake_nextion_find(prefix)) seenBal[i] = true;
        }
        if (fake_nextion_find("cellmax.val=")) seenMax = true;
        if (fake_nextion_find("cellmin.val=")) seenMin = true;
        if (fake_nextion_find("warn.val=")) seenWarn = true;
    }

    for (int i = 0; i < BMS_CELL_COUNT; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(seenCell[i],
                                 "hücre voltajı tam turda yayılmadı");
        TEST_ASSERT_TRUE_MESSAGE(seenBar[i], "hücre barı tam turda yayılmadı");
        TEST_ASSERT_TRUE_MESSAGE(seenBal[i],
                                 "dengeleme bayrağı tam turda yayılmadı");
    }
    TEST_ASSERT_TRUE_MESSAGE(seenMax, "cellmax tam turda yayılmadı");
    TEST_ASSERT_TRUE_MESSAGE(seenMin, "cellmin tam turda yayılmadı");
    TEST_ASSERT_TRUE_MESSAGE(seenWarn, "warn tam turda yayılmadı");
}
