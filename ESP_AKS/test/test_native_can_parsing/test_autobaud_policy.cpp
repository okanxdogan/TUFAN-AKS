#include <unity.h>

#include "AutobaudPolicy.h"
#include "SystemConfig.h"  // CAN_AUTOBAUD_RETRY_INTERVAL_MS

// ===========================================================================
// CAN Autobaud Retry Policy (saf autobaud_should_retry).
// Ana senaryo: begin() fallback'e (500kbps, doğrulanmamış) düştüğünde, ne
// zaman yeniden deneme tetiklenir / ne zaman kalıcı olarak durur.
// Tick birimi agnostiktir; testte ms-benzeri sayılar kullanılır
// (interval=CAN_AUTOBAUD_RETRY_INTERVAL_MS=5000).
// ===========================================================================

static constexpr uint32_t INTERVAL = CAN_AUTOBAUD_RETRY_INTERVAL_MS;  // 5000

// Pre-reception (hiçbir şey doğrulanmadı/alınmadı) + interval TAM dolunca →
// retry tetiklenir.
void test_pre_reception_interval_elapsed_retries(void) {
    TEST_ASSERT_TRUE(autobaud_should_retry(
        /*bitrateVerified=*/false, /*hasReceivedAnyFrame=*/false,
        /*now=*/INTERVAL, /*lastAttemptTick=*/0, INTERVAL));
}

// Interval henüz DOLMADI (sınırın 1 altı) → retry yok.
void test_pre_reception_interval_not_elapsed_no_retry(void) {
    TEST_ASSERT_FALSE(autobaud_should_retry(false, false, INTERVAL - 1, 0,
                                            INTERVAL));
}

// Sınır (tam eşitlik) → retry tetiklenir (>= semantiği, diğer freshness
// yardımcılarıyla — BmsFreshness/CanParse timeout — aynı desen).
void test_interval_boundary_triggers(void) {
    const uint32_t lastAttemptTick = 1000;
    const uint32_t now = lastAttemptTick + INTERVAL;  // fark tam INTERVAL
    TEST_ASSERT_TRUE(
        autobaud_should_retry(false, false, now, lastAttemptTick, INTERVAL));
}

// Bitrate DOĞRULANDI (retry başarıyla bitrate buldu VEYA boot auto-detect
// başarılıydı) → interval dolmuş olsa bile retry KALICI OLARAK durur.
void test_verified_never_retries_even_if_interval_elapsed(void) {
    TEST_ASSERT_FALSE(
        autobaud_should_retry(/*bitrateVerified=*/true,
                              /*hasReceivedAnyFrame=*/false,
                              /*now=*/1000000, /*lastAttemptTick=*/0,
                              INTERVAL));
}

// İlk geçerli frame ALINDI (fallback hızında bile) → bitrateVerified henüz
// set edilmemiş olsa dahi (savunma amaçlı ikinci bayrak) retry SONSUZA DEK
// durur.
void test_frame_received_never_retries_even_if_interval_elapsed(void) {
    TEST_ASSERT_FALSE(
        autobaud_should_retry(/*bitrateVerified=*/false,
                              /*hasReceivedAnyFrame=*/true,
                              /*now=*/1000000, /*lastAttemptTick=*/0,
                              INTERVAL));
}

// Her iki bayrak da true (normal doğrulanmış çalışma durumu) → retry yok.
void test_both_flags_true_no_retry(void) {
    TEST_ASSERT_FALSE(
        autobaud_should_retry(true, true, 1000000, 0, INTERVAL));
}

// Fallback'te sayaç taşması (tick wraparound) güvenli: lastAttemptTick
// UINT32_MAX'e yakın, now taştıktan sonra küçük bir değer — unsigned çıkarma
// doğru (sarmalanmamış) geçen süreyi vermeli.
void test_tick_wraparound_within_window_no_retry(void) {
    // lastAttemptTick = UINT32_MAX-2, now = 5 → gerçek fark = 8 < INTERVAL.
    const uint32_t lastAttemptTick = 0xFFFFFFFEu - 1;  // UINT32_MAX-2
    const uint32_t now = 5;
    TEST_ASSERT_FALSE(
        autobaud_should_retry(false, false, now, lastAttemptTick, INTERVAL));
}

void test_tick_wraparound_at_threshold_retries(void) {
    // lastAttemptTick = UINT32_MAX-2; now = lastAttemptTick + INTERVAL
    // (sarmalanarak) → gerçek fark tam INTERVAL → retry tetiklenir.
    const uint32_t lastAttemptTick = 0xFFFFFFFEu - 1;  // UINT32_MAX-2
    const uint32_t now = (uint32_t)(lastAttemptTick + INTERVAL);
    TEST_ASSERT_TRUE(
        autobaud_should_retry(false, false, now, lastAttemptTick, INTERVAL));
}

// Post-reception senaryo bu mekanizmanın KAPSAMI DIŞINDA: bir kez frame
// alınıp SONRADAN kesilse bile (hasReceivedAnyFrame hep true kalır, CanManager
// bunu asla false'a döndürmez) retry tetiklenmez — o BmsFreshness/
// TEL_bmsTimeoutActive yolunun sorumluluğudur.
void test_post_reception_timeout_is_out_of_scope(void) {
    TEST_ASSERT_FALSE(autobaud_should_retry(/*bitrateVerified=*/true,
                                            /*hasReceivedAnyFrame=*/true,
                                            /*now=*/999999999,
                                            /*lastAttemptTick=*/0, INTERVAL));
}
