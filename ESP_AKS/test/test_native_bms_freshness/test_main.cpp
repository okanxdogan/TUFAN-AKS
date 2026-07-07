#include <unity.h>

#include "BmsFreshness.h"
#include "SystemConfig.h"  // CAN_BMS_STATUS_TIMEOUT_MS

// ===========================================================================
// G12 — BMS mesaj-ID bazlı tazelik (saf bms_evaluate_freshness).
// Ana senaryo: E000 akıyor + E001 kesildi → timeout süresi sonunda sıcaklık
// verisi (dolayısıyla TEL_bmsDataValid) GEÇERSİZ sayılır ve timeoutActive olur.
// Tick birimi agnostiktir; testte ms-benzeri sayılar kullanılır (timeout=500).
// ===========================================================================

static constexpr uint32_t TO = CAN_BMS_STATUS_TIMEOUT_MS;  // 500

// Her iki ID de taze → geçerli, timeout yok.
void test_both_fresh_is_valid(void) {
    // now=1000; E000 ve E001 son 1000'de görüldü (fark 0 < 500).
    BmsFreshnessResult r =
        bms_evaluate_freshness(true, 1000, true, 1000, 1000, TO);
    TEST_ASSERT_TRUE(r.dataValid);
    TEST_ASSERT_FALSE(r.timeoutActive);
}

// ANA SENARYO: E000 akıyor (taze) ama E001 kesildi (timeout aşıldı) →
// veri GEÇERSİZ + timeoutActive (bayat sıcaklık artık görünür).
void test_e000_flowing_e001_stale_invalidates(void) {
    // now = last_e001 + TO (tam timeout) → E001 bayat; E000 now'da tazelendi.
    const uint32_t lastE001 = 1000;
    const uint32_t now = lastE001 + TO;  // 1500: (now-lastE001)=500 >= 500 → bayat
    BmsFreshnessResult r =
        bms_evaluate_freshness(/*sawE000=*/true, /*lastE000=*/now,
                               /*sawE001=*/true, /*lastE001=*/lastE001, now, TO);
    TEST_ASSERT_FALSE(r.dataValid);      // sıcaklık bayat → BMS verisi geçersiz
    TEST_ASSERT_TRUE(r.timeoutActive);   // görülmüş ID bayatladı → kritik yola eskale
}

// Timeout SINIRI (strictly): fark == TO-1 hâlâ taze, == TO bayat.
void test_e001_timeout_boundary(void) {
    const uint32_t lastE001 = 1000;
    // fark = TO-1 → hâlâ taze
    BmsFreshnessResult fresh = bms_evaluate_freshness(
        true, lastE001 + (TO - 1), true, lastE001, lastE001 + (TO - 1), TO);
    TEST_ASSERT_TRUE(fresh.dataValid);
    TEST_ASSERT_FALSE(fresh.timeoutActive);
    // fark = TO → bayat
    BmsFreshnessResult stale = bms_evaluate_freshness(
        true, lastE001 + TO, true, lastE001, lastE001 + TO, TO);
    TEST_ASSERT_FALSE(stale.dataValid);
    TEST_ASSERT_TRUE(stale.timeoutActive);
}

// Simetri: E000 bayat + E001 taze → yine geçersiz + timeoutActive.
void test_e000_stale_e001_fresh_invalidates(void) {
    const uint32_t lastE000 = 1000;
    const uint32_t now = lastE000 + TO;
    BmsFreshnessResult r =
        bms_evaluate_freshness(true, lastE000, true, now, now, TO);
    TEST_ASSERT_FALSE(r.dataValid);
    TEST_ASSERT_TRUE(r.timeoutActive);
}

// Pre-reception: E001 HİÇ görülmedi → geçersiz AMA timeoutActive DEĞİL
// (görülmemiş ID bayatlamış sayılmaz; IDLE'da tolere edilir).
void test_e001_never_seen_invalid_but_no_timeout(void) {
    BmsFreshnessResult r =
        bms_evaluate_freshness(/*sawE000=*/true, 1000, /*sawE001=*/false, 0,
                               1000, TO);
    TEST_ASSERT_FALSE(r.dataValid);
    TEST_ASSERT_FALSE(r.timeoutActive);
}

// Hiçbiri görülmedi (boot) → geçersiz, timeout yok.
void test_none_seen_invalid_no_timeout(void) {
    BmsFreshnessResult r = bms_evaluate_freshness(false, 0, false, 0, 1000, TO);
    TEST_ASSERT_FALSE(r.dataValid);
    TEST_ASSERT_FALSE(r.timeoutActive);
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_both_fresh_is_valid);
    RUN_TEST(test_e000_flowing_e001_stale_invalidates);
    RUN_TEST(test_e001_timeout_boundary);
    RUN_TEST(test_e000_stale_e001_fresh_invalidates);
    RUN_TEST(test_e001_never_seen_invalid_but_no_timeout);
    RUN_TEST(test_none_seen_invalid_no_timeout);
    return UNITY_END();
}
