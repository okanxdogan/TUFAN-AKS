#include <unity.h>

#include "HMIHelpers.h"

// =========================================================================
// "Veri yok" gösterimi — HMI_batteryDisplayValue / HMI_temperatureDisplayValue
//
// Kaynak sinyaller DOĞRULANDI:
//   SoC → 0xE000 byte[4:5], Temp → 0xE001 byte[6:7].
// TEL_bmsDataValid=false ise sentinel (255 / -127) gönderilir.
// Bu testler sentinel yolunu, dönüşüm/clamp yolunu ve SOURCE_VERIFIED
// bayraklarının true olduğunu doğrular.
// =========================================================================

// --- Kaynak DOĞRULANMADI (false) durumu testleri ---

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

void test_production_source_verified_flags_are_true(void) {
    // Sinyaller doğrulandığı için artık TRUE olmalılar.
    TEST_ASSERT_TRUE(HMI_SOC_SOURCE_VERIFIED);
    TEST_ASSERT_TRUE(HMI_TEMP_SOURCE_VERIFIED);
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

// =========================================================================
// Nextion float (xfloat) ölçekleme — packv (×10) ve packa (×1)
// packv/packa 2 ondalıklı float bileşeni; .val = gerçek_değer×100 olmalı.
// =========================================================================

void test_packv_decivolt_scaled_to_xfloat(void) {
    // 800 deciV = 80.0 V -> 8000 -> "80.00"
    TEST_ASSERT_EQUAL_INT32(8000, HMI_packVoltageToXfloat(800));
    // 526 deciV = 52.6 V -> 5260 -> "52.60"
    TEST_ASSERT_EQUAL_INT32(5260, HMI_packVoltageToXfloat(526));
    TEST_ASSERT_EQUAL_INT32(0, HMI_packVoltageToXfloat(0));
    // Üst sınır uint16 (65535 deciV) int32'ye taşmadan sığar
    TEST_ASSERT_EQUAL_INT32(655350, HMI_packVoltageToXfloat(65535));
}

void test_packa_centiamp_passes_through_as_xfloat(void) {
    // centiA zaten A×100 -> ek ölçekleme yok
    // 1250 centiA = 12.5 A -> "12.50"
    TEST_ASSERT_EQUAL_INT32(1250, HMI_packCurrentToXfloat(1250));
    // Negatif (deşarj) korunur: -2000 centiA = -20.0 A -> "-20.00"
    TEST_ASSERT_EQUAL_INT32(-2000, HMI_packCurrentToXfloat(-2000));
    TEST_ASSERT_EQUAL_INT32(0, HMI_packCurrentToXfloat(0));
}
