#include <unity.h>

#include "SystemConfig.h"    // LORA_UART_MAX_INIT_ATTEMPTS
#include "UartInitRetry.h"

// ===========================================================================
// G11 — UART init retry-durum mantığı (saf uart_init_retry_decision).
// N denemeden ÖNCE RETRY, N'e ULAŞINCA GIVE_UP_DISABLED (telemetrisiz mod).
// Bu, sonsuz retry tuzağının kırıldığını doğrular.
// ===========================================================================

// İlk başarısızlıklar (max altında) → tekrar dene.
void test_below_max_retries(void) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UartInitDecision::RETRY),
                          static_cast<int>(uart_init_retry_decision(1, 5)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UartInitDecision::RETRY),
                          static_cast<int>(uart_init_retry_decision(4, 5)));
}

// max'e ULAŞINCA (>=) → vazgeç (telemetrisiz mod). SONSUZ döngü YOK.
void test_at_and_above_max_gives_up(void) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UartInitDecision::GIVE_UP_DISABLED),
                          static_cast<int>(uart_init_retry_decision(5, 5)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UartInitDecision::GIVE_UP_DISABLED),
                          static_cast<int>(uart_init_retry_decision(6, 5)));
}

// Üretim sabitiyle: max-1 hâlâ retry, max vazgeç (sonlu döngü garantisi).
void test_with_production_constant(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(UartInitDecision::RETRY),
        static_cast<int>(uart_init_retry_decision(
            LORA_UART_MAX_INIT_ATTEMPTS - 1, LORA_UART_MAX_INIT_ATTEMPTS)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(UartInitDecision::GIVE_UP_DISABLED),
        static_cast<int>(uart_init_retry_decision(
            LORA_UART_MAX_INIT_ATTEMPTS, LORA_UART_MAX_INIT_ATTEMPTS)));
}

// Savunma: maxAttempts <= 0 ise hemen vazgeç (sonsuz döngü olamaz).
void test_nonpositive_max_gives_up_immediately(void) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UartInitDecision::GIVE_UP_DISABLED),
                          static_cast<int>(uart_init_retry_decision(0, 0)));
}

// ===========================================================================
// G11-b — GIVE_UP_DISABLED sonrası periyodik yeniden deneme kararı
// (lora_task_retry_due). Sürücü telemetrisiz moda GIRDIKTEN sonra, görev
// (vTask_LoRa_UKS) artık SONSUZA DEK kilitli kalmamalı — bu saf fonksiyon
// "şimdi tekrar deneme zamanı mı geldi" kararını verir.
// ===========================================================================

// Aralık henüz dolmadı → henüz retry zamanı DEĞİL.
void test_retry_not_due_before_interval_elapses(void) {
    TEST_ASSERT_FALSE(lora_task_retry_due(1000, 0, 30000));
    TEST_ASSERT_FALSE(lora_task_retry_due(29999, 0, 30000));
}

// Aralık tam dolunca (>=) → retry zamanı geldi.
void test_retry_due_at_and_after_interval(void) {
    TEST_ASSERT_TRUE(lora_task_retry_due(30000, 0, 30000));
    TEST_ASSERT_TRUE(lora_task_retry_due(30001, 0, 30000));
    TEST_ASSERT_TRUE(lora_task_retry_due(999999, 0, 30000));
}

// Üretim sabitiyle (LORA_INIT_RETRY_INTERVAL_MS): aynı sınır davranışı.
void test_retry_due_with_production_constant(void) {
    TEST_ASSERT_FALSE(lora_task_retry_due(
        LORA_INIT_RETRY_INTERVAL_MS - 1, 0, LORA_INIT_RETRY_INTERVAL_MS));
    TEST_ASSERT_TRUE(lora_task_retry_due(
        LORA_INIT_RETRY_INTERVAL_MS, 0, LORA_INIT_RETRY_INTERVAL_MS));
}

// lastAttemptMs sıfır değilse de aynı fark mantığı çalışır (gerçek kullanım:
// LO_lastInitAttemptMs = LO_hal.nowMs() anlık boot zamanından ileri bir değer).
void test_retry_due_with_nonzero_last_attempt(void) {
    TEST_ASSERT_FALSE(lora_task_retry_due(105000, 100000, 30000));  // 5 sn gecti
    TEST_ASSERT_TRUE(lora_task_retry_due(130000, 100000, 30000));   // tam 30 sn
    TEST_ASSERT_TRUE(lora_task_retry_due(200000, 100000, 30000));   // fazlasiyla gecti
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_below_max_retries);
    RUN_TEST(test_at_and_above_max_gives_up);
    RUN_TEST(test_with_production_constant);
    RUN_TEST(test_nonpositive_max_gives_up_immediately);

    RUN_TEST(test_retry_not_due_before_interval_elapses);
    RUN_TEST(test_retry_due_at_and_after_interval);
    RUN_TEST(test_retry_due_with_production_constant);
    RUN_TEST(test_retry_due_with_nonzero_last_attempt);
    return UNITY_END();
}
