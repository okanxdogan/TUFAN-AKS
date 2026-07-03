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

// ---------------------------------------------------------------------------
// Boot-grace (S3): boot+grace SÜRESİ İÇİNDE, hiç heartbeat gelmemişken
// (last_hb_ms==0) link DOWN sayılmamalı — plain link_check_timeout ile
// aynı "henüz erken" davranışı.
// ---------------------------------------------------------------------------
void test_boot_grace_not_down_within_grace_period(void) {
    // boot=0, now=4000 (< 5000 ms grace), hiç heartbeat yok
    TEST_ASSERT_FALSE(
        link_check_timeout_with_boot_grace(4000u, 0u, 3000u, 0u, 5000u));
}

// ---------------------------------------------------------------------------
// Boot-grace: grace süresi sonunda hâlâ heartbeat yoksa link DOWN sayılır
// (S3) — mevcut link_check_timeout, last_hb_ms==0 için hep false dönerdi.
// ---------------------------------------------------------------------------
void test_boot_grace_down_after_grace_expires_with_no_heartbeat(void) {
    // boot=0, now=5001 (> 5000 ms grace), hiç heartbeat yok
    TEST_ASSERT_TRUE(
        link_check_timeout_with_boot_grace(5001u, 0u, 3000u, 0u, 5000u));
}

// ---------------------------------------------------------------------------
// Boot-grace: tam grace anında (eşitlik) henüz DOWN sayılmamalı (> ile
// kesin geçme kuralı, link_check_timeout ile tutarlı).
// ---------------------------------------------------------------------------
void test_boot_grace_exactly_at_grace_is_not_down(void) {
    TEST_ASSERT_FALSE(
        link_check_timeout_with_boot_grace(5000u, 0u, 3000u, 0u, 5000u));
}

// ---------------------------------------------------------------------------
// Boot-grace: ilk heartbeat gelir gelmez normal link_check_timeout
// davranışına geçilir — grace süresi geçmiş olsa bile heartbeat varsa
// timeout hesaplaması last_hb_ms'e göre yapılır.
// ---------------------------------------------------------------------------
void test_boot_grace_uses_normal_timeout_once_heartbeat_seen(void) {
    // boot=0, grace=5000 zaten geçti, ama heartbeat t=6000'de geldi.
    // now=6500: 500 ms < 3000 ms timeout → UP.
    TEST_ASSERT_FALSE(
        link_check_timeout_with_boot_grace(6500u, 6000u, 3000u, 0u, 5000u));
}

// ---------------------------------------------------------------------------
// Boot-grace: heartbeat geldikten sonra normal timeout kuralı hâlâ
// uygulanır — heartbeat eski kalırsa yine DOWN olur.
// ---------------------------------------------------------------------------
void test_boot_grace_still_times_out_after_stale_heartbeat(void) {
    // boot=0, heartbeat t=6000, now=10000: 4000 ms > 3000 ms → DOWN.
    TEST_ASSERT_TRUE(
        link_check_timeout_with_boot_grace(10000u, 6000u, 3000u, 0u, 5000u));
}

// ---------------------------------------------------------------------------
// Boot-grace: boot_ms sıfır olmayan (gerçek uptime) bir değerse de fark
// doğru hesaplanmalı — mutlak zamana değil boot'tan geçen süreye bakılır.
// ---------------------------------------------------------------------------
void test_boot_grace_relative_to_nonzero_boot_time(void) {
    // boot=100000, now=104999 (grace içinde, 4999 ms < 5000 ms) → DOWN değil
    TEST_ASSERT_FALSE(link_check_timeout_with_boot_grace(104999u, 0u, 3000u,
                                                          100000u, 5000u));
    // now=105001 (grace aşıldı, 5001 ms > 5000 ms) → DOWN
    TEST_ASSERT_TRUE(link_check_timeout_with_boot_grace(105001u, 0u, 3000u,
                                                         100000u, 5000u));
}
