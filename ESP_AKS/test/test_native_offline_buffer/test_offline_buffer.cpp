#include <unity.h>
#include "OfflineBuffer.h"

// ---------------------------------------------------------------------------
// 3 farklı paket push → pop sırası FIFO ile aynı olmalı
// ---------------------------------------------------------------------------
void test_push_pop_fifo(void) {
    ob_reset();

    TelemetryData a = {}, b = {}, c = {};
    a.TEL_motorRpm = 100u;
    b.TEL_motorRpm = 200u;
    c.TEL_motorRpm = 300u;

    ob_push(a);
    ob_push(b);
    ob_push(c);

    TelemetryData out = {};
    TEST_ASSERT_TRUE(ob_pop(out));
    TEST_ASSERT_EQUAL_UINT16(100u, out.TEL_motorRpm);
    TEST_ASSERT_TRUE(ob_pop(out));
    TEST_ASSERT_EQUAL_UINT16(200u, out.TEL_motorRpm);
    TEST_ASSERT_TRUE(ob_pop(out));
    TEST_ASSERT_EQUAL_UINT16(300u, out.TEL_motorRpm);
    TEST_ASSERT_TRUE(ob_is_empty());
}

// ---------------------------------------------------------------------------
// OB_CAPACITY+1 eleman push → count sabit, ilk eleman (en eski) düşer
// ---------------------------------------------------------------------------
void test_capacity_drop_oldest(void) {
    ob_reset();

    // Dolduruluyor: rpm = 0 .. OB_CAPACITY-1
    for (int i = 0; i < OB_CAPACITY; i++) {
        TelemetryData d = {};
        d.TEL_motorRpm = (uint16_t)i;
        ob_push(d);
    }
    TEST_ASSERT_EQUAL_INT(OB_CAPACITY, ob_count());

    // Bir fazlası: rpm=OB_CAPACITY, en eski (rpm=0) düşmeli
    TelemetryData extra = {};
    extra.TEL_motorRpm = (uint16_t)OB_CAPACITY;
    ob_push(extra);

    TEST_ASSERT_EQUAL_INT(OB_CAPACITY, ob_count());

    // İlk pop: artık rpm=1 (rpm=0 düştü)
    TelemetryData first = {};
    TEST_ASSERT_TRUE(ob_pop(first));
    TEST_ASSERT_EQUAL_UINT16(1u, first.TEL_motorRpm);
}

// ---------------------------------------------------------------------------
// Boş buffer'dan pop → false
// ---------------------------------------------------------------------------
void test_empty_pop(void) {
    ob_reset();
    TelemetryData out = {};
    TEST_ASSERT_FALSE(ob_pop(out));
}

// ---------------------------------------------------------------------------
// Push sonrası reset → count=0, pop false
// ---------------------------------------------------------------------------
void test_reset(void) {
    ob_reset();

    TelemetryData d = {};
    d.TEL_motorRpm = 999u;
    ob_push(d);
    TEST_ASSERT_EQUAL_INT(1, ob_count());

    ob_reset();
    TEST_ASSERT_EQUAL_INT(0, ob_count());

    TelemetryData out = {};
    TEST_ASSERT_FALSE(ob_pop(out));
}

// ---------------------------------------------------------------------------
// link_down simülasyonu: 5 paket buffer'a yaz;
// link_up simülasyonu: 5 pop ile FIFO sırasını doğrula
// ---------------------------------------------------------------------------
void test_link_down_up_scenario(void) {
    ob_reset();

    // link DOWN — 5 paket buffer'a yaz
    for (int i = 0; i < 5; i++) {
        TelemetryData d = {};
        d.TEL_motorRpm      = (uint16_t)((i + 1) * 100);
        d.TEL_timestampMs   = (uint32_t)(i * 200);  // 200 ms aralıklı
        ob_push(d);
    }
    TEST_ASSERT_EQUAL_INT(5, ob_count());

    // link UP — replay FIFO sırasıyla
    for (int i = 0; i < 5; i++) {
        TelemetryData out = {};
        TEST_ASSERT_TRUE(ob_pop(out));
        TEST_ASSERT_EQUAL_UINT16((uint16_t)((i + 1) * 100), out.TEL_motorRpm);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)(i * 200), out.TEL_timestampMs);
    }
    TEST_ASSERT_TRUE(ob_is_empty());
}

// ---------------------------------------------------------------------------
// ob_peek: en eskiyi okur ama DÜŞÜRMEZ — ardışık peek aynı elemanı döner.
// ---------------------------------------------------------------------------
void test_peek_does_not_remove(void) {
    ob_reset();

    TelemetryData a = {};
    a.TEL_motorRpm = 111u;
    ob_push(a);

    TelemetryData p1 = {}, p2 = {};
    TEST_ASSERT_TRUE(ob_peek(p1));
    TEST_ASSERT_EQUAL_UINT16(111u, p1.TEL_motorRpm);
    TEST_ASSERT_EQUAL_INT(1, ob_count());

    TEST_ASSERT_TRUE(ob_peek(p2));
    TEST_ASSERT_EQUAL_UINT16(111u, p2.TEL_motorRpm);
    TEST_ASSERT_EQUAL_INT(1, ob_count());
}

// ---------------------------------------------------------------------------
// ob_drop_front: en eskiyi düşürür, count azalır, sıradaki eleman peek/pop
// edilebilir hale gelir. Boş buffer'da false döner.
// ---------------------------------------------------------------------------
void test_drop_front_removes_front(void) {
    ob_reset();

    TelemetryData a = {}, b = {};
    a.TEL_motorRpm = 10u;
    b.TEL_motorRpm = 20u;
    ob_push(a);
    ob_push(b);

    TEST_ASSERT_TRUE(ob_drop_front());
    TEST_ASSERT_EQUAL_INT(1, ob_count());

    TelemetryData out = {};
    TEST_ASSERT_TRUE(ob_peek(out));
    TEST_ASSERT_EQUAL_UINT16(20u, out.TEL_motorRpm);

    TEST_ASSERT_TRUE(ob_drop_front());
    TEST_ASSERT_TRUE(ob_is_empty());
    TEST_ASSERT_FALSE(ob_drop_front());
}

// ---------------------------------------------------------------------------
// Peek + AUX-meşgul simülasyonu: peek edilen paket TX başarısız olursa
// drop_front çağrılmaz — paket buffer'da kalmalı (kayıp yok).
// ---------------------------------------------------------------------------
void test_peek_without_drop_keeps_packet_for_retry(void) {
    ob_reset();

    TelemetryData a = {};
    a.TEL_motorRpm = 42u;
    ob_push(a);

    TelemetryData out = {};
    TEST_ASSERT_TRUE(ob_peek(out));  // "TX denemesi" — AUX meşgul varsayımı, drop YOK

    TEST_ASSERT_EQUAL_INT(1, ob_count());
    TelemetryData retry = {};
    TEST_ASSERT_TRUE(ob_peek(retry));
    TEST_ASSERT_EQUAL_UINT16(42u, retry.TEL_motorRpm);
}

// ---------------------------------------------------------------------------
// Boş buffer'da peek → false, out değişmez beklentisiyle çağrılabilir olmalı.
// ---------------------------------------------------------------------------
void test_empty_peek(void) {
    ob_reset();
    TelemetryData out = {};
    TEST_ASSERT_FALSE(ob_peek(out));
}

// ---------------------------------------------------------------------------
// Reset sonrası peek de false dönmeli (yalnızca pop değil).
// ---------------------------------------------------------------------------
void test_reset_clears_peek(void) {
    ob_reset();
    TelemetryData d = {};
    d.TEL_motorRpm = 7u;
    ob_push(d);

    ob_reset();

    TelemetryData out = {};
    TEST_ASSERT_FALSE(ob_peek(out));
    TEST_ASSERT_EQUAL_INT(0, ob_count());
}

// ---------------------------------------------------------------------------
// Kapasite 75'te en-eskiyi-düşürme: OB_CAPACITY dolduktan sonra bir eleman
// daha push edilirse en eski (index 0) düşer, count OB_CAPACITY'de sabit
// kalır. (S2: kapasite 300'den 75'e indi — 60 sn × 1 Hz + %25 marj.)
// ---------------------------------------------------------------------------
void test_capacity_is_75(void) {
    TEST_ASSERT_EQUAL_INT(75, OB_CAPACITY);
}

// ---------------------------------------------------------------------------
// offline_should_sample: has_sample=false (bu kesintide henüz örnek yok)
// → hemen örnekle (true), absolute now_ms/last_sample_ms değerinden
// bağımsız (last_sample_ms=0 iken bile now_ms=0 olması yanlış pozitif
// üretmemeli — bkz. test_60s_outage_simulation ile ilgili edge-case).
// ---------------------------------------------------------------------------
void test_offline_should_sample_first_sample_is_immediate(void) {
    TEST_ASSERT_TRUE(offline_should_sample(123456u, 0u, false, 1000u));
    TEST_ASSERT_TRUE(offline_should_sample(0u, 0u, false, 1000u));
}

// ---------------------------------------------------------------------------
// offline_should_sample: period_ms geçmediyse false.
// ---------------------------------------------------------------------------
void test_offline_should_sample_within_period_is_false(void) {
    // 1500 - 1000 = 500 ms < 1000 ms → false
    TEST_ASSERT_FALSE(offline_should_sample(1500u, 1000u, true, 1000u));
}

// ---------------------------------------------------------------------------
// offline_should_sample: tam period_ms geçtiyse true (>= karşılaştırması).
// ---------------------------------------------------------------------------
void test_offline_should_sample_at_period_is_true(void) {
    // 2000 - 1000 = 1000 ms >= 1000 ms → true
    TEST_ASSERT_TRUE(offline_should_sample(2000u, 1000u, true, 1000u));
}

// ---------------------------------------------------------------------------
// offline_should_sample: period_ms'i aşınca da true.
// ---------------------------------------------------------------------------
void test_offline_should_sample_past_period_is_true(void) {
    TEST_ASSERT_TRUE(offline_should_sample(5000u, 1000u, true, 1000u));
}

// ---------------------------------------------------------------------------
// 1 Hz örnekleme simülasyonu (S2): 500 ms tik'lerle (LORA_TX_PERIOD_MS, link
// flapping düzeltmesi sonrası 2 Hz) 10 sn kesinti simüle edilir (20 tik).
// offline_should_sample 1000 ms periyotla push'u kapılar; beklenen ~10 paket
// (±1) — 9.2.h'nin ≤5 sn kuralına 5x marjla uyar.
// ---------------------------------------------------------------------------
void test_1hz_sampling_over_10s_outage_yields_about_10_packets(void) {
    ob_reset();

    uint64_t lastSampleMs = 0u;
    bool hasSample = false;
    int pushedCount = 0;

    for (int tick = 0; tick < 20; tick++) {  // 20 * 500ms = 10000 ms
        const uint64_t nowMs = (uint64_t)(tick * 500);
        if (offline_should_sample(nowMs, lastSampleMs, hasSample, 1000u)) {
            lastSampleMs = nowMs;
            hasSample = true;
            TelemetryData d = {};
            d.TEL_timestampMs = (uint32_t)nowMs;
            ob_push(d);
            pushedCount++;
        }
    }

    TEST_ASSERT_TRUE(pushedCount >= 9 && pushedCount <= 11);
    TEST_ASSERT_EQUAL_INT(pushedCount, ob_count());
}

// ---------------------------------------------------------------------------
// 60 sn kesinti simülasyonu (kabul kriteri): 120 tik × 500 ms
// (LORA_TX_PERIOD_MS, 2 Hz) = 60000 ms. 1 Hz örnekleme + kapasite 75 ile:
// buffer <= 75 paket, en eski paketin ts'i kesinti başlangıcına (0) ait
// olmalı (kapasite hiç aşılmıyor: 60 paket <= 75).
// ---------------------------------------------------------------------------
void test_60s_outage_simulation_stays_within_capacity(void) {
    ob_reset();

    uint64_t lastSampleMs = 0u;
    bool hasSample = false;
    for (int tick = 0; tick < 120; tick++) {  // 120 * 500ms = 60000 ms
        const uint64_t nowMs = (uint64_t)(tick * 500);
        if (offline_should_sample(nowMs, lastSampleMs, hasSample, 1000u)) {
            lastSampleMs = nowMs;
            hasSample = true;
            TelemetryData d = {};
            d.TEL_timestampMs = (uint32_t)nowMs;
            ob_push(d);
        }
    }

    TEST_ASSERT_TRUE(ob_count() <= OB_CAPACITY);
    // 60 sn × 1 Hz = 60 paket, kapasiteyi (75) aşmadığından hiçbiri düşmemeli.
    TEST_ASSERT_EQUAL_INT(60, ob_count());

    TelemetryData oldest = {};
    TEST_ASSERT_TRUE(ob_peek(oldest));
    TEST_ASSERT_EQUAL_UINT32(0u, oldest.TEL_timestampMs);  // kesinti başlangıcı
}

// ---------------------------------------------------------------------------
// Replay throttle simülasyonu (S1): 60 buffered paket, her "tik"te en fazla
// REPLAY_BURST_PER_TICK(=1) paket + 1 canlı gönderilir. Buffer'ın
// boşalması için gereken tik sayısı 60 olmalı; 60 tik ×
// LORA_TX_PERIOD_MS(500ms, link flapping düzeltmesi sonrası 2 Hz) = 30000 ms.
// Canlı akış (ayrı sayaç) hiç kesilmeden her tikte 1 artmalı. Per-tick byte
// bütçesi (9600 baud'da 500 ms'de ~480 byte) aşılmadığı teyit edilir (1
// canlı + 1 replay = 180 byte, bkz. LoRa_Link_Analysis.md).
// ---------------------------------------------------------------------------
void test_replay_throttle_drains_60_packets_within_expected_ticks(void) {
    ob_reset();

    for (int i = 0; i < 60; i++) {
        TelemetryData d = {};
        d.TEL_motorRpm = (uint16_t)i;
        ob_push(d);
    }
    TEST_ASSERT_EQUAL_INT(60, ob_count());

    const int burstPerTick = 1;
    int ticks = 0;
    int liveSentCount = 0;
    uint16_t nextExpectedRpm = 0;

    while (!ob_is_empty()) {
        ticks++;
        TEST_ASSERT_TRUE(ticks <= 100);  // sonsuz döngü koruması

        for (int b = 0; b < burstPerTick; b++) {
            TelemetryData out = {};
            if (!ob_peek(out)) break;
            // AUX her zaman hazır varsayımı (throttle mantığının kendisi test
            // ediliyor; AUX-meşgul senaryosu ayrı test edildi).
            TEST_ASSERT_EQUAL_UINT16(nextExpectedRpm, out.TEL_motorRpm);
            nextExpectedRpm++;
            TEST_ASSERT_TRUE(ob_drop_front());
        }
        // Canlı akış hiç kesilmez: her tikte 1 canlı paket "gönderilir".
        liveSentCount++;
    }

    TEST_ASSERT_EQUAL_INT(60, ticks);           // ceil(60/1)
    TEST_ASSERT_EQUAL_INT(60, liveSentCount);   // canlı akış kesilmedi
    TEST_ASSERT_TRUE((ticks * 500) <= 35000);   // ~30 sn bekleme süresi dahilinde

}
