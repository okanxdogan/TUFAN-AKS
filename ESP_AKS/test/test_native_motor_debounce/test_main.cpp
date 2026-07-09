#include <unity.h>

#include "MotorFaultDebounce.h"
#include "SystemConfig.h"  // MOTOR_ERROR_DEBOUNCE_FRAMES

// ===========================================================================
// G9 — Motor error-flag debounce (saf motorErrorFaultConfirmed).
// Senaryolar: 1-2 hatalı frame → FAULT yok; 3 ardışık → FAULT; araya temiz
// frame girerse sayaç sıfırlanır. Fonksiyon MOTOR_DRIVER_PRESENT'ten bağımsız.
// ===========================================================================

// 1-2 ardışık hatalı frame (N=3): henüz onay YOK, sayaç doğru artar.
void test_one_or_two_error_frames_not_confirmed(void) {
    uint16_t c = 0;
    TEST_ASSERT_FALSE(motorErrorFaultConfirmed(0x01, c, 3));  // frame 1
    TEST_ASSERT_EQUAL_UINT16(1, c);
    TEST_ASSERT_FALSE(motorErrorFaultConfirmed(0x01, c, 3));  // frame 2
    TEST_ASSERT_EQUAL_UINT16(2, c);
}

// 3 ardışık hatalı frame → fault ONAYLANIR.
void test_three_consecutive_error_frames_confirm(void) {
    uint16_t c = 0;
    (void)motorErrorFaultConfirmed(0x01, c, 3);              // 1
    (void)motorErrorFaultConfirmed(0x01, c, 3);              // 2
    TEST_ASSERT_TRUE(motorErrorFaultConfirmed(0x01, c, 3));  // 3 → onay
    TEST_ASSERT_EQUAL_UINT16(3, c);
}

// Araya TEMİZ (errorFlags==0) frame girerse sayaç sıfırlanır → onay ertelenir.
void test_clean_frame_resets_counter(void) {
    uint16_t c = 0;
    (void)motorErrorFaultConfirmed(0x01, c, 3);              // 1
    (void)motorErrorFaultConfirmed(0x01, c, 3);              // 2
    TEST_ASSERT_FALSE(motorErrorFaultConfirmed(0x00, c, 3)); // temiz → reset
    TEST_ASSERT_EQUAL_UINT16(0, c);

    // Zincir kırıldığı için 2 hatalı daha yine onay üretmez...
    TEST_ASSERT_FALSE(motorErrorFaultConfirmed(0x01, c, 3)); // 1
    TEST_ASSERT_FALSE(motorErrorFaultConfirmed(0x01, c, 3)); // 2
    // ...ancak 3. ardışık hatalı frame onaylar.
    TEST_ASSERT_TRUE(motorErrorFaultConfirmed(0x01, c, 3));  // 3 → onay
}

// Onaylandıktan sonra ardışık hatalar onaylı kalır; tek temiz frame düşürür.
void test_confirmed_persists_until_clean_frame(void) {
    uint16_t c = 0;
    for (int i = 0; i < 3; ++i)
        (void)motorErrorFaultConfirmed(0x02, c, 3);
    TEST_ASSERT_TRUE(motorErrorFaultConfirmed(0x02, c, 3));   // 4. hâlâ onaylı
    TEST_ASSERT_FALSE(motorErrorFaultConfirmed(0x00, c, 3));  // temiz → düşer
    TEST_ASSERT_EQUAL_UINT16(0, c);
}

// Üretim sabiti ile (MOTOR_ERROR_DEBOUNCE_FRAMES): N-1 frame yok, N. frame var.
void test_with_production_constant(void) {
    uint16_t c = 0;
    bool confirmed = false;
    for (uint16_t i = 0; i < MOTOR_ERROR_DEBOUNCE_FRAMES - 1; ++i)
        confirmed = motorErrorFaultConfirmed(0x08, c, MOTOR_ERROR_DEBOUNCE_FRAMES);
    TEST_ASSERT_FALSE(confirmed);  // N-1 ardışık → onay yok
    confirmed = motorErrorFaultConfirmed(0x08, c, MOTOR_ERROR_DEBOUNCE_FRAMES);
    TEST_ASSERT_TRUE(confirmed);   // N. ardışık → onay
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_one_or_two_error_frames_not_confirmed);
    RUN_TEST(test_three_consecutive_error_frames_confirm);
    RUN_TEST(test_clean_frame_resets_counter);
    RUN_TEST(test_confirmed_persists_until_clean_frame);
    RUN_TEST(test_with_production_constant);
    return UNITY_END();
}
