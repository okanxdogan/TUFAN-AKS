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

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_below_max_retries);
    RUN_TEST(test_at_and_above_max_gives_up);
    RUN_TEST(test_with_production_constant);
    RUN_TEST(test_nonpositive_max_gives_up_immediately);
    return UNITY_END();
}
