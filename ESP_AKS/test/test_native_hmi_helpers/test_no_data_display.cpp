#include <unity.h>

#include "HMIHelpers.h"

// =========================================================================
// "Veri yok" gösterimi — HMI_batteryDisplayValue / HMI_temperatureDisplayValue
//
// Kaynak sinyaller (TEL_bmsSocHundredths, TEL_bmsTempHighestC) DOĞRULANMADI;
// doğrulanana kadar ekrana sahte %0/0°C yerine sentinel (255 / -127)
// gönderilir. Bu testler hem bugünkü "kaynak doğrulanmadı" yolunu hem de
// ileride kaynak doğrulandığında devreye girecek dönüşüm/clamp yolunu
// kilitler.
// =========================================================================

// --- Bugünkü üretim durumu: kaynak DOĞRULANMADI -> her zaman sentinel ---

void test_battery_unverified_source_returns_no_data(void) {
    // Değer ve bmsValid ne olursa olsun sentinel dönmeli
    TEST_ASSERT_EQUAL_UINT8(HMI_BATTERY_NO_DATA,
                            HMI_batteryDisplayValue(false, true, 8000));
    TEST_ASSERT_EQUAL_UINT8(HMI_BATTERY_NO_DATA,
                            HMI_batteryDisplayValue(false, false, 0));
}

void test_temp_unverified_source_returns_no_data(void) {
    TEST_ASSERT_EQUAL_INT16(HMI_TEMP_NO_DATA,
                            HMI_temperatureDisplayValue(false, true, 25));
    TEST_ASSERT_EQUAL_INT16(HMI_TEMP_NO_DATA,
                            HMI_temperatureDisplayValue(false, false, 0));
}

void test_production_source_verified_flags_are_false(void) {
    // Üretim sabitleri: kaynaklar doğrulanana kadar false kalmalı.
    // Bu test, sabit yanlışlıkla true yapılırsa (sinyal doğrulanmadan
    // gerçek veri gösterimi açılırsa) kırmızıya düşer.
    TEST_ASSERT_FALSE(HMI_SOC_SOURCE_VERIFIED);
    TEST_ASSERT_FALSE(HMI_TEMP_SOURCE_VERIFIED);
}

// --- bmsDataValid=false -> kaynak doğrulanmış olsa bile sentinel ---

void test_battery_invalid_bms_returns_no_data(void) {
    TEST_ASSERT_EQUAL_UINT8(HMI_BATTERY_NO_DATA,
                            HMI_batteryDisplayValue(true, false, 8000));
}

void test_temp_invalid_bms_returns_no_data(void) {
    TEST_ASSERT_EQUAL_INT16(HMI_TEMP_NO_DATA,
                            HMI_temperatureDisplayValue(true, false, 25));
}

// --- Gelecek yolu: kaynak doğrulanmış + veri taze -> gerçek dönüşüm ---

void test_battery_verified_valid_converts_hundredths_to_percent(void) {
    // 8000 hundredths = %80.00 -> 80
    TEST_ASSERT_EQUAL_UINT8(80, HMI_batteryDisplayValue(true, true, 8000));
    TEST_ASSERT_EQUAL_UINT8(0, HMI_batteryDisplayValue(true, true, 99));
    TEST_ASSERT_EQUAL_UINT8(100, HMI_batteryDisplayValue(true, true, 10000));
}

void test_battery_verified_valid_clamps_above_100(void) {
    // Bozuk/aralık dışı SOC sentinelle (255) çakışmamalı — 100'e clamp
    TEST_ASSERT_EQUAL_UINT8(100, HMI_batteryDisplayValue(true, true, 65535));
}

void test_temp_verified_valid_passes_through(void) {
    TEST_ASSERT_EQUAL_INT16(25, HMI_temperatureDisplayValue(true, true, 25));
    TEST_ASSERT_EQUAL_INT16(-10, HMI_temperatureDisplayValue(true, true, -10));
}
