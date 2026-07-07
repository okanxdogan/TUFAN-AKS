// G1 interlock — MOTOR_DRIVER_PRESENT=1 derlemesi.
//
// isReadyEntryPermitted() saf/inline bir header predicate'i oldugundan,
// MOTOR_DRIVER_PRESENT=1 dalinin davranisi yalnizca bayragi 1 olarak goren bir
// derleme biriminde dogrulanabilir. Native test binary'si VcuLogic.cpp'yi
// (dolayisiyla run() durum makinesini) bayrak 0 ile derler; bu yuzden motor
// verisi sartini ayri bir binary'de, dogrudan predicate uzerinden test ediyoruz
// (predicate saf: run()'a / global state'e ihtiyac yok, link gerekmez).
#define MOTOR_DRIVER_PRESENT 1

#include <unity.h>

#include "VcuLogic.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;

// Bayragin gercekten 1 oldugunu (SystemConfig.h #ifndef guard'i ezmediginden)
// dogrula — aksi halde asagidaki testler yaniltici sekilde gecerdi.
static_assert(MOTOR_DRIVER_PRESENT == 1,
              "Bu binary MOTOR_DRIVER_PRESENT=1 ile derlenmeli");

// MOTOR_DRIVER_PRESENT=1 iken: bmsDataValid=true + kritik/uyari yok olsa bile
// motorDataValid=false ise READY girisi REDDEDILIR.
void test_ready_entry_rejected_when_motor_data_invalid(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorDataValid = false;

    TEST_ASSERT_FALSE(VcuLogic::isReadyEntryPermitted(d));
}

// MOTOR_DRIVER_PRESENT=1 iken taze motor verisi varsa (ve gerisi temizse)
// READY girisine izin verilir.
void test_ready_entry_permitted_when_motor_data_valid(void) {
    TelemetryData d = makeTelemetryDataValid();  // motorDataValid=true

    TEST_ASSERT_TRUE(VcuLogic::isReadyEntryPermitted(d));
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_ready_entry_rejected_when_motor_data_invalid);
    RUN_TEST(test_ready_entry_permitted_when_motor_data_valid);
    return UNITY_END();
}
