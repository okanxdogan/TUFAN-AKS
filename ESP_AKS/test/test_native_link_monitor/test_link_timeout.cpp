#include <unity.h>
#include "LinkMonitor.h"

// ---------------------------------------------------------------------------
// last_hb_ms == 0 (hiç heartbeat gelmedi) → timeout OLMAMALI
// ---------------------------------------------------------------------------
void test_never_received_heartbeat(void) {
    TEST_ASSERT_FALSE(link_check_timeout(99999u, 0u, 3000u));
}

// ---------------------------------------------------------------------------
// Fark < timeout → link hâlâ UP
// ---------------------------------------------------------------------------
void test_within_timeout(void) {
    // 5000 - 4000 = 1000 ms < 3000 ms → false
    TEST_ASSERT_FALSE(link_check_timeout(5000u, 4000u, 3000u));
}

// ---------------------------------------------------------------------------
// Fark > timeout → link DOWN
// ---------------------------------------------------------------------------
void test_past_timeout(void) {
    // 5000 - 1000 = 4000 ms > 3000 ms → true
    TEST_ASSERT_TRUE(link_check_timeout(5000u, 1000u, 3000u));
}

// ---------------------------------------------------------------------------
// Fark == timeout → eşitlik DOWN sayılmaz (kesinlikle büyük olmalı)
// ---------------------------------------------------------------------------
void test_exactly_at_timeout_is_not_down(void) {
    // 4000 - 1000 = 3000 ms, NOT > 3000 → false
    TEST_ASSERT_FALSE(link_check_timeout(4000u, 1000u, 3000u));
}

// ---------------------------------------------------------------------------
// UP → DOWN → UP senaryosu (pure function, çoklu çağrıyla simüle edilir)
// ---------------------------------------------------------------------------
void test_link_up_then_down_then_up(void) {
    uint64_t last_hb = 1000u;

    // t=1500: 500 ms < 3000 ms → UP
    TEST_ASSERT_FALSE(link_check_timeout(1500u, last_hb, 3000u));

    // t=5000: 4000 ms > 3000 ms → DOWN
    TEST_ASSERT_TRUE(link_check_timeout(5000u, last_hb, 3000u));

    // Yeni heartbeat t=5100
    last_hb = 5100u;

    // t=5200: 100 ms < 3000 ms → tekrar UP
    TEST_ASSERT_FALSE(link_check_timeout(5200u, last_hb, 3000u));
}
