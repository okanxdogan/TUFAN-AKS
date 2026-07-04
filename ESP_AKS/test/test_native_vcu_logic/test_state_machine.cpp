#include <unity.h>

#include "VcuLogic.h"
#include "fake_freertos.h"
#include "fake_relay_manager.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::VcuEvent;
using VcuLogic::VcuState;

namespace {

// Tüm testler bu helper ile temiz başlar — modülün statik state'i,
// queue ve relay sayaçları sıfırlanır, init() ile IDLE'ye geçilir.
void primeIdle() {
    VcuLogic::resetForTest();
    fake_freertos_reset();
    fake_relay_reset();
    VcuLogic::init();
    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

}  // namespace

// ---------------------------------------------------------------------------
// init() bittikten sonra durum IDLE olmalı; allOff() de bir kez çağrılmış
// olmalı (init içinde safety olarak yapılıyor).
// ---------------------------------------------------------------------------
void test_init_transitions_to_idle_and_calls_allOff(void) {
    VcuLogic::resetForTest();
    fake_freertos_reset();
    fake_relay_reset();

    VcuLogic::init();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOff_count);
}

// ---------------------------------------------------------------------------
// IDLE → READY on START_REQUEST.  handleReady() ilk run'da allOn() çağırır.
// ---------------------------------------------------------------------------
void test_idle_to_ready_on_start_request(void) {
    primeIdle();

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOn_count);
}

// ---------------------------------------------------------------------------
// READY → DRIVE on DRIVE_ENABLE.
// ---------------------------------------------------------------------------
void test_ready_to_drive_on_drive_enable(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::postEvent(VcuEvent::DRIVE_ENABLE);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::DRIVE),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// EMERGENCY_STOP olayı her durumdan ESTOP'a geçirir.
// ---------------------------------------------------------------------------
void test_idle_to_emergency_stop(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));
}

void test_drive_to_emergency_stop(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    VcuLogic::postEvent(VcuEvent::DRIVE_ENABLE);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::DRIVE),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// FAULT_DETECTED olayı her durumdan FAULT'a geçirir.
// ---------------------------------------------------------------------------
void test_idle_to_fault_on_fault_event(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

void test_ready_to_fault_on_fault_event(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// READY/DRIVE'da kritik telemetri otomatik FAULT'a tetikler (run() içinde
// hasCriticalCondition() kontrolü).
// ---------------------------------------------------------------------------
void test_ready_to_fault_on_critical_telemetry(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 590;  // < 600 dV — undervoltage critical (DOĞRULANMIŞ sinyal)
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

void test_drive_to_fault_on_bms_timeout(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    VcuLogic::postEvent(VcuEvent::DRIVE_ENABLE);
    VcuLogic::run();

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTimeoutActive = true;  // post-reception E000 freshness kaybı
    d.TEL_bmsDataValid = false;
    VcuLogic::setTelemetryData(d);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// Reset path — interlock OK iken FAULT'tan IDLE'ye dönüş.
// ---------------------------------------------------------------------------
void test_fault_to_idle_on_reset_when_clean(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    VcuLogic::postEvent(VcuEvent::RESET);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

void test_emergency_stop_to_idle_on_reset_when_clean(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    VcuLogic::postEvent(VcuEvent::RESET);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// Reset interlock kötü iken FAULT'ta kalır (motor error flag set).
// ---------------------------------------------------------------------------
void test_fault_stays_on_reset_when_motor_error(void) {
    primeIdle();
    TelemetryData faulty = makeTelemetryDataValid();
    faulty.TEL_motorErrorFlags = 0x01;
    VcuLogic::setTelemetryData(faulty);
    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));

    // Hata hâlâ devam ederken RESET denenirse interlock reddetmeli.
    VcuLogic::postEvent(VcuEvent::RESET);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// IDLE'da RESET no-op olmalı (RESET sadece FAULT/ESTOP'ta anlamlı).
// ---------------------------------------------------------------------------
void test_idle_reset_is_noop(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::RESET);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// IDLE'da motor timeout aktifse FAULT'a geçmemeli (timeout IDLE'de yok sayılır).
// ---------------------------------------------------------------------------
void test_idle_with_motor_timeout_stays_idle(void) {
    primeIdle();
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    VcuLogic::setTelemetryData(d);

    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

// ---------------------------------------------------------------------------
// EMERGENCY_STOP girdikten sonra VCU_CONTACTOR_OPEN_DELAY_MS=20 kadar
// beklenir; ikinci run() (timer ≥ 20ms) sonrası allOff() tetiklenmeli.
// ---------------------------------------------------------------------------
void test_emergency_stop_opens_contactors_after_delay(void) {
    primeIdle();
    // init() içinde 1 allOff zaten oldu — sayacı sıfırla ki ESTOP'taki çağrı
    // izole edilebilsin.
    fake_relay_reset();

    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();   // transitionTo: stateTimer=0; handleEStop: stateTimer=0, allOff tetiklenmez
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOff_count);

    VcuLogic::run();   // stateTimer=20 → allOff
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOff_count);
}
