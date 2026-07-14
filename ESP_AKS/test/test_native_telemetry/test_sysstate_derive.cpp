#include <unity.h>

#include "SysStateDerive.h"

// ===========================================================================
// HİPOTEZ: SysStateDerive — akımdan türetilmiş sysState (bkz. SysStateDerive.h,
// Documents/CAN_Message_Table.md "0x0000E003"). Bu dosya, bayraktan (SYSSTATE_
// DERIVE_FROM_CURRENT) bağımsız olan çekirdek matematiği (Impl + üretim
// sarmalayıcısı) VE varsayılan derleme ortamında (flag=0) applyIfEnabled'ın
// gerçekten NO-OP olduğunu doğrular. flag=1 davranışı (uygulama + ezmeme
// kuralı) ayrı bir derleme birimindedir — bkz. test_native_sysstate_derive_enabled.
// ===========================================================================

// Bant içinde (|akım| <= bant) -> IDLE(2).
void test_derive_impl_zero_current_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(2, SysStateDerive::deriveFromCurrentImpl(0, 50));
}

// Tam sınırda (+bant, -bant) -> HALA IDLE (kapsayıcı sınır: |akım| <= bant).
void test_derive_impl_at_positive_band_boundary_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(2, SysStateDerive::deriveFromCurrentImpl(50, 50));
}

void test_derive_impl_at_negative_band_boundary_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(2, SysStateDerive::deriveFromCurrentImpl(-50, 50));
}

// Sınırın hemen ÜSTÜNDE/ALTINDA -> Charge(3) / Discharge(1).
void test_derive_impl_just_above_positive_band_is_charge(void) {
    TEST_ASSERT_EQUAL_UINT8(3, SysStateDerive::deriveFromCurrentImpl(51, 50));
}

void test_derive_impl_just_below_negative_band_is_discharge(void) {
    TEST_ASSERT_EQUAL_UINT8(1, SysStateDerive::deriveFromCurrentImpl(-51, 50));
}

// Bandın çok üstünde/altında da aynı sınıflandırma geçerli.
void test_derive_impl_large_positive_current_is_charge(void) {
    TEST_ASSERT_EQUAL_UINT8(3, SysStateDerive::deriveFromCurrentImpl(990, 50));
}

void test_derive_impl_large_negative_current_is_discharge(void) {
    TEST_ASSERT_EQUAL_UINT8(1, SysStateDerive::deriveFromCurrentImpl(-2000, 50));
}

// Üretim sarmalayıcısı (deriveFromCurrent), SYSSTATE_CURRENT_IDLE_BAND_CENTI_A
// üretim sabitini kullanır — sınır davranışı Impl ile birebir aynı olmalı.
void test_derive_production_wrapper_matches_impl_at_production_band(void) {
    TEST_ASSERT_EQUAL_UINT8(
        2, SysStateDerive::deriveFromCurrent(SYSSTATE_CURRENT_IDLE_BAND_CENTI_A));
    TEST_ASSERT_EQUAL_UINT8(
        3,
        SysStateDerive::deriveFromCurrent(SYSSTATE_CURRENT_IDLE_BAND_CENTI_A + 1));
    TEST_ASSERT_EQUAL_UINT8(
        1,
        SysStateDerive::deriveFromCurrent(-SYSSTATE_CURRENT_IDLE_BAND_CENTI_A - 1));
}

// ---------------------------------------------------------------------------
// applyIfEnabled — varsayılan derleme ortamında SYSSTATE_DERIVE_FROM_CURRENT
// tanımsız/0'dır (SystemConfig.h #ifndef guard). Bu native test binary'si bu
// bayrağı override ETMEZ; bu yüzden burada yalnızca "flag=0 iken davranış
// birebir korunur (dokunulmaz)" doğrulanabilir. flag=1 davranışı ayrı bir
// derleme birimi gerektirir (bkz. test_native_sysstate_derive_enabled).
// ---------------------------------------------------------------------------
void test_apply_is_noop_when_flag_disabled_even_if_sysstate_zero(void) {
    static_assert(SYSSTATE_DERIVE_FROM_CURRENT == 0,
                  "Bu test SYSSTATE_DERIVE_FROM_CURRENT=0 varsayar (varsayilan "
                  "native derleme ortami)");

    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;          // "henuz parse edilmedi" durumu
    d.TEL_bmsCurrentCentiA = 990;      // acikca "Charge" sayilacak bir akim

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(0, d.TEL_bmsSystemState);  // DOKUNULMADI
}
