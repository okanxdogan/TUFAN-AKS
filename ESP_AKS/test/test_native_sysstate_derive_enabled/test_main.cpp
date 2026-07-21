// HİPOTEZ (bkz. SysStateDerive.h, Documents/CAN_Message_Table.md "0x0000E003")
// — SYSSTATE_DERIVE_FROM_CURRENT=1 derlemesi.
//
// applyIfEnabled() saf/inline bir header fonksiyonu olduğundan, flag=1
// dalının davranışı yalnızca bayrağı 1 olarak gören bir derleme biriminde
// doğrulanabilir (aynı desen: test_native_ready_motor/test_main.cpp,
// MOTOR_DRIVER_PRESENT=1). Varsayılan native derleme ortamı bu bayrağı 0
// görür (bkz. test_native_telemetry/test_sysstate_derive.cpp).
#define SYSSTATE_DERIVE_FROM_CURRENT 1

#include <unity.h>

#include "SysStateDerive.h"

// Bayrağın gerçekten 1 olduğunu (SystemConfig.h #ifndef guard'ı ezmediğinden)
// doğrula — aksi halde aşağıdaki testler yanıltıcı şekilde geçerdi.
static_assert(SYSSTATE_DERIVE_FROM_CURRENT == 1,
              "Bu binary SYSSTATE_DERIVE_FROM_CURRENT=1 ile derlenmeli");

// flag=1 + TEL_bmsSystemState==0 + acikca sarj sayilacak akim -> Charge(3).
void test_apply_derives_charge_when_sysstate_zero_and_current_above_band(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;
    d.TEL_bmsCurrentCentiA = 990;  // 9.9 A sarj

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(3, d.TEL_bmsSystemState);
}

// flag=1 + TEL_bmsSystemState==0 + acikca desarj sayilacak akim -> Discharge(1).
void test_apply_derives_discharge_when_sysstate_zero_and_current_below_band(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;
    d.TEL_bmsCurrentCentiA = -150;  // -1.5 A desarj

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(1, d.TEL_bmsSystemState);
}

// flag=1 + TEL_bmsSystemState==0 + bant icinde akim -> IDLE(2).
void test_apply_derives_idle_when_sysstate_zero_and_current_within_band(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;
    d.TEL_bmsCurrentCentiA = -10;  // -0.1 A — bosta gozlemlenen tipik deger

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(2, d.TEL_bmsSystemState);
}

// flag=1 AMA TEL_bmsSystemState ZATEN 0'dan farkli (gercek parse eklenmis
// gibi davranan bir deger) -> EZILMEMELI, dokunulmadan kalmali.
void test_apply_does_not_overwrite_when_sysstate_already_nonzero(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 2;         // "zaten gercek parse ile dolduruldu" varsayimi
    d.TEL_bmsCurrentCentiA = 990;     // derive edilseydi Charge(3) cikardi

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(2, d.TEL_bmsSystemState);  // DEGISMEDI
}

// FAULT(4) da "zaten dolu" sayilir — derive FAULT uretmedigi icin bu deger
// asla derive ciktisiyla karisamaz, ama yine de EZILMEMELI kuralini kanitlar.
void test_apply_does_not_overwrite_existing_fault_state(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 4;
    d.TEL_bmsCurrentCentiA = 0;

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(4, d.TEL_bmsSystemState);
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_derives_charge_when_sysstate_zero_and_current_above_band);
    RUN_TEST(test_apply_derives_discharge_when_sysstate_zero_and_current_below_band);
    RUN_TEST(test_apply_derives_idle_when_sysstate_zero_and_current_within_band);
    RUN_TEST(test_apply_does_not_overwrite_when_sysstate_already_nonzero);
    RUN_TEST(test_apply_does_not_overwrite_existing_fault_state);
    return UNITY_END();
}
