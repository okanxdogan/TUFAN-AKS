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
