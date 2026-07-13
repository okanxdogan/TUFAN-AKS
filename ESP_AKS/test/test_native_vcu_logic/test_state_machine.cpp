#include <unity.h>

#include "MotorTorque.h"
#include "TorqueRequestQueue.h"
#include "VcuLogic.h"
#include "fake_freertos.h"
#include "mock_relay_actuator.h"
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
    VcuLogic::init(g_mockRelay);
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

    VcuLogic::init(g_mockRelay);

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
// G1 interlock: BMS verisi hic gelmemisken (bmsDataValid=false) START gelirse
// READY'ye GECILMEZ — HV bus batarya hakkinda sifir bilgiyle enerjilenmez.
// ---------------------------------------------------------------------------
void test_idle_start_rejected_when_bms_never_valid(void) {
    primeIdle();

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;  // BMS CAN'a hic frame gelmemis (pre-reception)
    VcuLogic::setTelemetryData(d);

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);
}

// ---------------------------------------------------------------------------
// G1 interlock: bmsDataValid=true + kritik yok + uyari yok → READY'ye gecer.
// ---------------------------------------------------------------------------
void test_idle_start_permitted_when_bms_valid_and_clean(void) {
    primeIdle();

    TelemetryData d = makeTelemetryDataValid();  // bmsDataValid=true, temiz
    VcuLogic::setTelemetryData(d);

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOn_count);
}

// ---------------------------------------------------------------------------
// G1 interlock: bmsDataValid=true ama uyari kosulu aktifken (WARN bandi pack
// voltaji) START gelirse READY'ye GECILMEZ, IDLE'da kalir.
// ---------------------------------------------------------------------------
void test_idle_start_rejected_when_warning_active(void) {
    primeIdle();

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 715;  // <= 720 dV WARN low (kritik degil, >600)
    VcuLogic::setTelemetryData(d);

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);
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
// R1 — FAULT bypass: olay kuyruğu DOLU olsa bile FAULT_DETECTED kaybolmaz.
// FAULT_DETECTED, E-STOP gibi atomic bir bayrak da set eder; run() her tick
// (kuyruğa bakmadan, en tepede) bu bayrağı okur. Mevcut E-STOP bypass'ının
// paralel senaryosu: kuyruğu doldur → fault gönder → bir sonraki tick FAULT.
// ---------------------------------------------------------------------------
void test_fault_pending_processed_when_queue_full(void) {
    primeIdle();

    // Olay kuyruğunu kapasitesine kadar doldur (init xQueueCreate(8)); arada
    // run() ÇAĞIRMA ki drenaj olmasın. Kuyruk artık TAMAMEN dolu.
    for (int i = 0; i < 8; i++)
        VcuLogic::postEvent(VcuEvent::START_REQUEST);

    // FAULT_DETECTED kuyruğu bypass edip atomic bayrağı set eder (E-STOP deseni),
    // bu yüzden kuyruk dolu olsa bile fault kaybolmaz.
    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);

    // Tek tick: bayrak yolu (kuyruk drenajından ÖNCE, en tepede) FAULT'a alır.
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

// READY'deyken kritik sıcaklık (≥70 °C, BMS_CRITICAL_MAX_TEMP_C) gelirse
// aynı otomatik yol FAULT'a alır — sistem kendini kapatır.
void test_ready_to_fault_on_critical_temp(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 70;  // tam eşikte — >= semantiği FAULT tetikler
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

void test_idle_with_unverified_bms_system_state_stays_idle(void) {
    primeIdle();
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsSystemState = 4;  // FAULT shaped output
    d.TEL_bmsDataValid = false;
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

void test_fault_latches_contactors_off_once_and_reasserts(void) {
    primeIdle();
    fake_relay_reset();

    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);
    VcuLogic::run(); // t=0
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOff_count);

    VcuLogic::run(); // t=20
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOff_count);
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOff_silent_count);

    // Simulate runs up to 980ms (48 ticks of 20ms = 960ms + 20ms = 980ms)
    // Actually, at t=20, it ran. Next is t=40.
    for(int i=0; i<48; i++) {
        VcuLogic::run();
    }
    // Now t=980
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOff_count);

    VcuLogic::run(); // t=1000
    // At t=1000, 1000 - 0 = 1000, so it logs and silently re-asserts
    TEST_ASSERT_EQUAL_UINT(2, g_fake_relay_allOff_count);
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOff_silent_count);
}

// ===========================================================================
// G3 — Actuator (röle sürücüsü) fault durum makinesi entegrasyonu.
// RelayManager kalıcı atomic bir bayrak tutar; VcuLogic her tick okur.
// ===========================================================================

// run() her tick RelayManager::verifyIfDue çağırmalı (periyodik doğrulama).
void test_run_calls_verifyIfDue_each_tick(void) {
    primeIdle();
    unsigned before = g_fake_relay_verifyIfDue_count;
    VcuLogic::run();
    VcuLogic::run();
    TEST_ASSERT_EQUAL_UINT(before + 2, g_fake_relay_verifyIfDue_count);
}

// Actuator fault aktifken START gelirse READY'ye GEÇİLMEZ (guard).
void test_idle_start_rejected_when_actuator_fault(void) {
    primeIdle();
    g_fake_relay_actuatorFault = true;  // röle geri-okuma uyuşmazlığı enjekte et

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);
}

// READY'deyken actuator fault gelirse mevcut fault yoluna (FAULT) girer.
void test_ready_to_fault_on_actuator_fault(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    g_fake_relay_actuatorFault = true;
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

// DRIVE'dayken actuator fault gelirse de FAULT'a girer.
void test_drive_to_fault_on_actuator_fault(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    VcuLogic::postEvent(VcuEvent::DRIVE_ENABLE);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::DRIVE),
                          static_cast<int>(VcuLogic::getState()));

    g_fake_relay_actuatorFault = true;
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

// FAULT'tan RESET, RelayManager::clearActuatorFault'u çağırmalı.
void test_reset_from_fault_clears_actuator_fault(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();  // READY
    g_fake_relay_actuatorFault = true;
    VcuLogic::run();  // → FAULT
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));

    unsigned clearsBefore = g_fake_relay_clearFault_count;
    VcuLogic::setTelemetryData(makeTelemetryDataValid());  // interlock temiz
    VcuLogic::postEvent(VcuEvent::RESET);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(clearsBefore + 1, g_fake_relay_clearFault_count);
    TEST_ASSERT_FALSE(g_fake_relay_actuatorFault);
}

// ===========================================================================
// G2 — E-STOP/FAULT güvenli kapanış sırası: sıfır-tork ÖNCE, kontaktör açma
// SONRA. Torque isteği bir spy sink ile, kontaktör açma fake allOff ile
// kaydedilir; paylaşılan monoton sayaç sırayı doğrular.
// ===========================================================================

namespace {
unsigned s_torqueCount = 0;
unsigned s_torqueFirstSeq = 0;
uint16_t s_lastTorque = 0xFFFF;

void torqueSpy(uint16_t torque) {
    ++s_torqueCount;
    s_lastTorque = torque;
    if (s_torqueFirstSeq == 0)
        s_torqueFirstSeq = ++g_fake_call_seq;
}

// Spy + sıra sayaçlarını temiz başlat (primeIdle'daki init allOff'unu da izole
// etmek için fake_relay_reset burada TEKRAR çağrılır).
void primeEstopOrder() {
    s_torqueCount = 0;
    s_torqueFirstSeq = 0;
    s_lastTorque = 0xFFFF;
    fake_relay_reset();               // g_fake_call_seq / allOff_firstSeq = 0
    VcuLogic::setTorqueSink(torqueSpy);
}
}  // namespace

void test_estop_requests_zero_torque_before_opening_contactors(void) {
    primeIdle();
    primeEstopOrder();

    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();  // transitionTo ESTOP (handler bu tick çalışmaz)
    VcuLogic::run();  // handler: (1) torque(0) → (2) delay doldu → (3) allOff

    // (a) sıfır tork tam olarak bir kez ve değer 0 ile istendi (E-STOP spam yok)
    TEST_ASSERT_EQUAL_UINT(1, s_torqueCount);
    TEST_ASSERT_EQUAL_UINT16(0, s_lastTorque);
    // (b) hem tork hem röle açma gerçekleşti
    TEST_ASSERT_TRUE(s_torqueFirstSeq > 0);
    TEST_ASSERT_TRUE(g_fake_relay_allOff_firstSeq > 0);
    // (c) SIRA: torque(0) kontaktör açmadan ÖNCE
    TEST_ASSERT_TRUE(s_torqueFirstSeq < g_fake_relay_allOff_firstSeq);
}

void test_fault_requests_zero_torque_before_opening_contactors(void) {
    primeIdle();
    primeEstopOrder();

    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);
    VcuLogic::run();  // transitionTo FAULT (t=0, henüz açma yok)
    VcuLogic::run();  // handler: torque(0) → allOff

    TEST_ASSERT_EQUAL_UINT(1, s_torqueCount);
    TEST_ASSERT_EQUAL_UINT16(0, s_lastTorque);
    TEST_ASSERT_TRUE(s_torqueFirstSeq > 0);
    TEST_ASSERT_TRUE(g_fake_relay_allOff_firstSeq > 0);
    TEST_ASSERT_TRUE(s_torqueFirstSeq < g_fake_relay_allOff_firstSeq);
}

// Sink bağlı değilken E-STOP çökmemeli (istek sessizce yok sayılır) ve
// kontaktörler yine de açılmalı.
void test_estop_without_torque_sink_still_opens_contactors(void) {
    primeIdle();
    fake_relay_reset();
    VcuLogic::setTorqueSink(nullptr);

    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();
    VcuLogic::run();

    TEST_ASSERT_TRUE(g_fake_relay_allOff_count > 0);
}

// ===========================================================================
// G2 thread-safety hazırlığı: torque isteği artık CanManager tarafında bir
// TorqueRequestQueue'ya YAZILIR (VCU task'inden, senkron/anında); gerçek
// twai_transmit ise CAN task döngüsünde (processRxMessages ile aynı yerde)
// queue.drainPending() ile ÇEKİLİP yapılacak (bkz.
// lib/CanManager/TorqueRequestQueue.h, Documents/MOTOR_ENTEGRASYON_NOTU.md
// madde 4). Bu test, kuyruklama EKLENSE BİLE VcuLogic'in E-STOP sırasının
// (sıfır-tork isteği ÖNCE, kontaktör açma SONRA) bozulmadığını VE isteğin
// CAN task'inin drain'ini beklemeden kuyruğa senkron olarak ulaştığını
// doğrular — CanManager natively test edilemediğinden (lib_ignore, gerçek
// twai bağımlılığı) burada testteki sink, gerçek CanManager::
// sendTorqueCommand'ın yapacağı gibi doğrudan bir TorqueRequestQueue'ya push
// eder.
// ===========================================================================
namespace {
TorqueRequestQueue s_canTorqueQueue;  // CanManager::s_torqueQueue'nun test eşdeğeri

void torqueSinkToCanQueue(uint16_t torque) {
    s_canTorqueQueue.push(torque);
    if (s_torqueFirstSeq == 0)
        s_torqueFirstSeq = ++g_fake_call_seq;
}
}  // namespace

void test_estop_zero_torque_reaches_can_queue_before_contactor_open(void) {
    primeIdle();
    primeEstopOrder();
    s_canTorqueQueue.resetForTest();  // temiz kuyruk
    VcuLogic::setTorqueSink(torqueSinkToCanQueue);

    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();  // transitionTo ESTOP (handler bu tick çalışmaz)
    VcuLogic::run();  // handler: (1) torque(0) -> queue.push -> (2) delay doldu -> (3) allOff

    // (a) İstek queue'ya ULAŞTI — CAN task henüz hiç drain ETMEMİŞKEN bile
    // (yani "CAN task'i dışından" gelen istek kaybolmadan kuyrukta bekliyor).
    TEST_ASSERT_TRUE(s_canTorqueQueue.hasPending());

    // (b) SIRA: kuyruğa yazma (VCU task, t=0) kontaktör açmadan (t=delay)
    // ÖNCE gerçekleşti — kuyruklama EKLENMESİ bu sırayı BOZMADI.
    TEST_ASSERT_TRUE(s_torqueFirstSeq > 0);
    TEST_ASSERT_TRUE(g_fake_relay_allOff_firstSeq > 0);
    TEST_ASSERT_TRUE(s_torqueFirstSeq < g_fake_relay_allOff_firstSeq);

    // (c) CAN task tik'i simülasyonu: drain edilince doğru değer (0) çıkar;
    // tek-seferlik tüketim sonrası ikinci drain artık bekleyen bulmaz.
    uint16_t drained = 0xFFFFu;
    TEST_ASSERT_TRUE(s_canTorqueQueue.drainPending(drained));
    TEST_ASSERT_EQUAL_UINT16(0, drained);
    TEST_ASSERT_FALSE(s_canTorqueQueue.hasPending());
}

// Bayrak 0 (varsayılan native build) iken torque frame'i ÜRETİLMEZ: gate saf
// yardımcısı false döner (CanManager::sendTorqueCommand bunu kullanır).
void test_flag0_torque_frame_disabled(void) {
    TEST_ASSERT_FALSE(MotorTorque::frameEnabled());
}
