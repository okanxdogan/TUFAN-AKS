#pragma once
#include <cstdint>
#include "SystemConfig.h"
#include "Telemetry.h"

namespace VcuLogic {

enum class VcuState : uint8_t {
    INIT = 0,
    IDLE = 1,
    READY = 2,  // Contactors closed, HV bus live
    DRIVE = 3,
    EMERGENCY_STOP = 4,
    FAULT = 5
};

enum class VcuEvent : uint8_t {
    NONE = 0,
    START_REQUEST = 1,
    DRIVE_ENABLE = 2,
    EMERGENCY_STOP = 3,
    FAULT_DETECTED = 4,
    RESET = 5
};

// ---------------------------------------------------------------------------
// Pure safety predicates
// ---------------------------------------------------------------------------
// Donanım veya global state'ten bağımsız; doğrudan argümana bakar. Inline
// tutulduğu için saf mantık testlerinde VcuLogic.cpp linklenmeden
// çağrılabilir.
//
// EK B GÜVEN KURALI: hasWarningCondition/hasCriticalCondition YALNIZCA
// doğrulanmış sinyallere bakar — pack voltajı (0xE000 byte[2:3], DOĞRULANDI)
// + BMS/motor freshness. Akım/sıcaklık kontrolleri, kaynak alanlar
// (TEL_bmsCurrentCentiMa, TEL_bmsTempHighestC) hiçbir CAN ID'den parse
// edilmediği için karar mantığından ÇIKARILDI; saha kalibrasyonu sonrası
// aşağıdaki saf yardımcılar üzerinden yeniden bağlanacak.

// TODO: source signal not yet verified — TEL_bmsCurrentCentiMa hiç
// yazılmıyor (hep 0). Bu iki yardımcı, ölçek doğrulanana kadar karar
// mantığına BAĞLI DEĞİLDİR; yalnız birim testleri ve gelecekteki bağlama
// için tutuluyor.
inline bool isCurrentCritical(int32_t bmsCurrentCentiMa) {
    return bmsCurrentCentiMa >= BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_MA ||
           bmsCurrentCentiMa <= -BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_MA;
}

inline bool isCurrentWarning(int32_t bmsCurrentCentiMa) {
    return bmsCurrentCentiMa >= BMS_WARN_MAX_CHARGE_CURRENT_CENTI_MA ||
           bmsCurrentCentiMa <= -BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_MA;
}

inline bool hasWarningCondition(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return false;

    // Yalnızca DOĞRULANMIŞ pack voltajı (WARN bandı).
    // TODO: source signal not yet verified — sıcaklık/akım warn kontrolleri
    // kaynak sinyaller doğrulanınca eklenecek.
    return VCU_data.TEL_bmsPackVoltageDeciV <=
               BMS_WARN_MIN_PACK_VOLTAGE_DECI_V ||
           VCU_data.TEL_bmsPackVoltageDeciV >=
               BMS_WARN_MAX_PACK_VOLTAGE_DECI_V;
}

inline bool hasCriticalCondition(const TelemetryData& VCU_data,
                                 VcuState currentState) {
    if (VCU_data.TEL_motorErrorFlags != 0 || VCU_data.TEL_bmsSystemState == 4)
        return true;

    if (VCU_data.TEL_motorTimeoutActive && currentState != VcuState::IDLE)
        return true;

    // Post-reception BMS freshness kaybı — motor timeout ile aynı politika:
    // IDLE'da tolere edilir, READY/DRIVE'da kritik (HV bus canlıyken bayat
    // BMS verisiyle sürüşe devam edilmez).
    if (VCU_data.TEL_bmsTimeoutActive && currentState != VcuState::IDLE)
        return true;

    if (!VCU_data.TEL_bmsDataValid)
        return false;

    // Yalnızca DOĞRULANMIŞ pack voltajı (CRITICAL bandı — 24S LiFePO4 spec).
    // TODO: source signal not yet verified — sıcaklık/akım/hücre-voltaj
    // kritik kontrolleri kaynak sinyaller doğrulanınca eklenecek.
    return VCU_data.TEL_bmsPackVoltageDeciV <=
               BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V ||
           VCU_data.TEL_bmsPackVoltageDeciV >=
               BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V;
}

inline bool isResetInterlockSatisfied(const TelemetryData& VCU_data,
                                      VcuState currentState) {
    if (VCU_data.TEL_motorErrorFlags != 0 || VCU_data.TEL_bmsSystemState == 4)
        return false;

    if (hasCriticalCondition(VCU_data, currentState))
        return false;

    return true;
}

// ---------------------------------------------------------------------------
// Stateful API
// ---------------------------------------------------------------------------
void init();
void run();
void postEvent(VcuEvent event);
VcuState getState();
void setTelemetryData(const TelemetryData& TEL_data);

#ifdef VCU_LOGIC_TESTABLE
// Yalnız native test build'inde aktif. Tüm modül-içi state'i (durum, timer,
// son telemetri, queue) sıfırlar; setUp() içinde çağrılarak testler arası
// izolasyon sağlanır. Üretim build'inde tanımlı değildir.
void resetForTest();
#endif

}  // namespace VcuLogic
