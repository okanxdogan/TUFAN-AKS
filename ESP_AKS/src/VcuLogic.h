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
// (TEL_bmsCurrentCentiA, TEL_bmsTempHighestC) hiçbir CAN ID'den parse
// edilmediği için karar mantığından ÇIKARILDI; saha kalibrasyonu sonrası
// aşağıdaki saf yardımcılar üzerinden yeniden bağlanacak.

// TODO: source signal not yet verified — TEL_bmsCurrentCentiA hiç
// yazılmıyor (hep 0). Bu iki yardımcı, ölçek doğrulanana kadar karar
// mantığına BAĞLI DEĞİLDİR; yalnız birim testleri ve gelecekteki bağlama
// için tutuluyor.
inline bool isCurrentCritical(int32_t bmsCurrentCentiA) {
    return bmsCurrentCentiA >= BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A ||
           bmsCurrentCentiA <= -BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A;
}

inline bool isCurrentWarning(int32_t bmsCurrentCentiA) {
    return bmsCurrentCentiA >= BMS_WARN_MAX_CHARGE_CURRENT_CENTI_A ||
           bmsCurrentCentiA <= -BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_A;
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
    if (VCU_data.TEL_motorErrorFlags != 0 || (VCU_data.TEL_bmsDataValid && VCU_data.TEL_bmsSystemState == 4))
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
    if (VCU_data.TEL_motorErrorFlags != 0 || (VCU_data.TEL_bmsDataValid && VCU_data.TEL_bmsSystemState == 4))
        return false;

    if (hasCriticalCondition(VCU_data, currentState))
        return false;

    return true;
}

// IDLE → READY giriş interlock'u. READY'ye geçiş 10 kontaktörü kapatıp HV
// bus'ı enerjilendirdiğinden, batarya hakkında DOĞRULANMIŞ ve TAZE veri
// olmadan bu geçişe izin verilmez. Saf/donanımsız: yalnızca argümana bakar,
// hiçbir global okumaz. READY girişi her zaman IDLE'dan yapıldığından
// kritik/uyarı kontrolleri IDLE bağlamında değerlendirilir (IDLE'da motor/BMS
// timeout'u kritik sayılmaz; bunun yerine burada TEL_bmsDataValid şartı aranır).
inline bool isReadyEntryPermitted(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return false;

    if (hasCriticalCondition(VCU_data, VcuState::IDLE))
        return false;

    if (hasWarningCondition(VCU_data))
        return false;

#if MOTOR_DRIVER_PRESENT
    // Motor sürücüsü araçta: READY interlock'u taze motor verisi de arar.
    // Bayrak 0 iken (sürücü henüz yok) motor timeout'u READY girişini bloklamaz.
    if (!VCU_data.TEL_motorDataValid)
        return false;
#endif

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

// Torque komut yolu (motor sürücüsü entegrasyon iskeleti). E-STOP/FAULT
// güvenli kapanış sırasında VcuLogic torque(0) ister; bu isteği donanıma
// (CanManager::sendTorqueCommand) yönlendiren sink'i main.cpp bağlar.
// VcuLogic, CanManager'a doğrudan bağımlı olmasın diye (native testler
// CanManager'ı linklemez) function-pointer hook kullanılır. Sink bağlı
// değilse istek sessizce yok sayılır. Native testler buraya bir spy takıp
// çağrı sırasını doğrular.
using TorqueSink = void (*)(uint16_t torque);
void setTorqueSink(TorqueSink sink);

#ifdef VCU_LOGIC_TESTABLE
// Yalnız native test build'inde aktif. Tüm modül-içi state'i (durum, timer,
// son telemetri, queue) sıfırlar; setUp() içinde çağrılarak testler arası
// izolasyon sağlanır. Üretim build'inde tanımlı değildir.
void resetForTest();
#endif

}  // namespace VcuLogic
