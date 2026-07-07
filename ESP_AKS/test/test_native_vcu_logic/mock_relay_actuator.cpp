// M2: VcuLogic'e enjekte edilen IRelayActuator mock'u. Gerçek SPI/RelayManager
// yok; çağrılar global sayaçlara işlenir, testler bunları doğrular.
#include "mock_relay_actuator.h"

unsigned g_fake_relay_allOn_count = 0;
unsigned g_fake_relay_allOff_count = 0;
unsigned g_fake_relay_allOff_silent_count = 0;

bool     g_fake_relay_actuatorFault = false;
unsigned g_fake_relay_clearFault_count = 0;
unsigned g_fake_relay_verifyIfDue_count = 0;

unsigned g_fake_call_seq = 0;
unsigned g_fake_relay_allOff_firstSeq = 0;

MockRelayActuator g_mockRelay;

void fake_relay_reset(void) {
    g_fake_relay_allOn_count = 0;
    g_fake_relay_allOff_count = 0;
    g_fake_relay_allOff_silent_count = 0;
    g_fake_relay_actuatorFault = false;
    g_fake_relay_clearFault_count = 0;
    g_fake_relay_verifyIfDue_count = 0;
    g_fake_call_seq = 0;
    g_fake_relay_allOff_firstSeq = 0;
}

void MockRelayActuator::closeAllContactors() {
    ++g_fake_relay_allOn_count;
}

void MockRelayActuator::openAllContactors(bool silent) {
    ++g_fake_relay_allOff_count;
    if (g_fake_relay_allOff_firstSeq == 0)
        g_fake_relay_allOff_firstSeq = ++g_fake_call_seq;  // G2: sıra kaydı
    if (silent)
        ++g_fake_relay_allOff_silent_count;
}

// --- G3: geri-okuma / actuator fault yolu (mock) ---
// Gerçek SPI geri-okuma yok; fault durumu testler tarafından
// g_fake_relay_actuatorFault ile enjekte edilir. VcuLogic bunları her tick
// okur (verifyIfDue + hasActuatorFault).
void MockRelayActuator::verifyIfDue(uint32_t /*nowMs*/) {
    ++g_fake_relay_verifyIfDue_count;
}

bool MockRelayActuator::hasActuatorFault() const {
    return g_fake_relay_actuatorFault;
}

void MockRelayActuator::clearActuatorFault() {
    g_fake_relay_actuatorFault = false;
    ++g_fake_relay_clearFault_count;
}
