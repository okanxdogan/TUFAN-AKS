#include <unity.h>

#include <climits>

#include "bms_edge_helpers.h"

// Edge case grubu: SICAKLIK ve AKIM uç değerleri (int16/int32 sınırları)
// Sadece BmsModel.h (BmsPackData) + test-local doğrulayıcılara bağlıdır.

using namespace bmsedge;

// --- Aşırı sıcaklık (>70 °C) -----------------------------------------------

void test_overtemp_detected(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellTempC[5] = 71;  // eşiğin (70) üstü
    TEST_ASSERT_TRUE(hasOvertemp(p));
}

void test_temp_at_threshold_not_overtemp(void) {
    BmsPackData p = makeUniform(3650, 70, 0, true);  // tam eşikte
    TEST_ASSERT_FALSE(hasOvertemp(p));  // "> 70" kesin büyükten ötürü FALSE
}

void test_overtemp_at_index_23(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    p.cellTempC[BMS_CELL_COUNT - 1] = 85;
    TEST_ASSERT_TRUE(hasOvertemp(p));
}

// --- Aşırı soğuk (< -20 °C / negatif) --------------------------------------

void test_undertemp_negative_detected(void) {
    BmsPackData p = makeUniform(3650, -25, 0, true);
    TEST_ASSERT_TRUE(hasUndertemp(p));
}

void test_temp_at_lower_threshold_not_undertemp(void) {
    BmsPackData p = makeUniform(3650, -20, 0, true);  // tam eşikte
    TEST_ASSERT_FALSE(hasUndertemp(p));  // "< -20" kesin küçükten ötürü FALSE
}

void test_int16_temp_min_extreme(void) {
    // int16 alt sınırı struct'ta saklanabilmeli ve undertemp olarak görülmeli.
    BmsPackData p = makeUniform(3650, INT16_MIN, 0, true);  // -32768 °C
    TEST_ASSERT_EQUAL_INT16(-32768, p.cellTempC[0]);
    TEST_ASSERT_TRUE(hasUndertemp(p));
}

void test_int16_temp_max_extreme(void) {
    BmsPackData p = makeUniform(3650, INT16_MAX, 0, true);  // 32767 °C
    TEST_ASSERT_EQUAL_INT16(32767, p.cellTempC[0]);
    TEST_ASSERT_TRUE(hasOvertemp(p));
}

// --- packCurrentMa aşırı pozitif/negatif (int32 sınırına yakın) -----------

void test_current_max_int32_stored(void) {
    BmsPackData p = makeUniform(3650, 25, INT32_MAX, true);
    TEST_ASSERT_EQUAL_INT32(2147483647, p.packCurrentMa);
}

void test_current_min_int32_stored(void) {
    BmsPackData p = makeUniform(3650, 25, INT32_MIN, true);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, p.packCurrentMa);
}

void test_current_sign_discharge_negative(void) {
    // Sözleşme: + şarj / - deşarj. İşaret korunmalı.
    BmsPackData p = makeUniform(3650, 25, -150000, true);
    TEST_ASSERT_TRUE(p.packCurrentMa < 0);
}

void test_current_zero_is_idle(void) {
    BmsPackData p = makeUniform(3650, 25, 0, true);
    TEST_ASSERT_EQUAL_INT32(0, p.packCurrentMa);
}
