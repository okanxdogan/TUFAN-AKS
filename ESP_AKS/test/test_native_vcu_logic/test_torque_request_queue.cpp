#include <unity.h>

#include "TorqueRequestQueue.h"

// ===========================================================================
// G2 thread-safety hazırlığı — TorqueRequestQueue (SAF, VCU task -> CAN task
// tork isteği aktarımı). Bkz. Documents/MOTOR_ENTEGRASYON_NOTU.md madde 4.
// ===========================================================================

// Boş kuyrukta drain false döner, out'a dokunulmaz.
void test_torque_queue_drain_empty_returns_false(void) {
    TorqueRequestQueue q;
    uint16_t out = 0xBEEFu;

    TEST_ASSERT_FALSE(q.hasPending());
    TEST_ASSERT_FALSE(q.drainPending(out));
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, out);  // dokunulmadi
}

// push sonrası hasPending true, drain doğru değeri verir ve pending'i tüketir.
void test_torque_queue_push_then_drain_returns_value_once(void) {
    TorqueRequestQueue q;
    q.push(1234);

    TEST_ASSERT_TRUE(q.hasPending());

    uint16_t out = 0;
    TEST_ASSERT_TRUE(q.drainPending(out));
    TEST_ASSERT_EQUAL_UINT16(1234, out);

    // Tek-seferlik tüketim: ikinci drain artik false doner.
    TEST_ASSERT_FALSE(q.hasPending());
    uint16_t again = 0xBEEFu;
    TEST_ASSERT_FALSE(q.drainPending(again));
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, again);
}

// Zero-torque (E-STOP/FAULT'un asıl kullanım senaryosu) da doğru aktarılır.
void test_torque_queue_zero_value_is_a_valid_pending_request(void) {
    TorqueRequestQueue q;
    q.push(0);

    TEST_ASSERT_TRUE(q.hasPending());  // deger 0 olsa bile "pending" true
    uint16_t out = 0xBEEFu;
    TEST_ASSERT_TRUE(q.drainPending(out));
    TEST_ASSERT_EQUAL_UINT16(0, out);
}

// "En son değer kazanır": drain edilmeden art arda push -> yalnızca SON değer
// teslim edilir (ara değer sessizce üzerine yazılır, kayıp KABUL edilebilir).
void test_torque_queue_overwrites_undrained_value_with_latest(void) {
    TorqueRequestQueue q;
    q.push(50);
    q.push(0);  // ör. E-STOP'un ilk isteği araya girdi

    uint16_t out = 0xBEEFu;
    TEST_ASSERT_TRUE(q.drainPending(out));
    TEST_ASSERT_EQUAL_UINT16(0, out);  // yalnizca en son (0) teslim edilir
}

// drainPending sonrası yeni bir push tekrar pending yapar (drain "tek seferlik
// tüketici" olduğu için, drain edilmiş kuyruk yeniden kullanılabilir olmalı).
void test_torque_queue_push_after_drain_is_pending_again(void) {
    TorqueRequestQueue q;
    q.push(7);
    uint16_t out = 0;
    TEST_ASSERT_TRUE(q.drainPending(out));
    TEST_ASSERT_FALSE(q.hasPending());

    q.push(9);
    TEST_ASSERT_TRUE(q.hasPending());
    TEST_ASSERT_TRUE(q.drainPending(out));
    TEST_ASSERT_EQUAL_UINT16(9, out);
}
