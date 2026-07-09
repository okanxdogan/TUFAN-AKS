#pragma once

#include <cstdint>

#include "VehicleData.h"    // TelemetryData (saf veri sözleşmesi — M3)
#include "VehicleParams.h"  // WHEEL_DIAMETER_M / GEAR_RATIO / MOTOR_RPM_IS_WHEEL_RPM — TEK doğruluk kaynağı
// NOT (M3): VehicleParams bağımlılığı BİLEREK burada — yalnız aşağıdaki hız
// hesabı (rpmToSpeedKmhX10) buna ihtiyaç duyar. TelemetryData artık
// VehicleData.h'de olduğundan CanParse/OfflineBuffer/VcuLogic vb. bu LoRa
// header'ını (ve dolayısıyla VehicleParams'ı) ARTIK ÇEKMEZ.

#define TEL_SPD_X10_MAX 3000  // UKS telemetry.c sanity siniri ile senkron
                              // — degistirilecekse iki tarafta birlikte
                              // degistirilmeli

// km/h = rpm_teker * PI * D * 60 / 1000   →   x10 hassasiyet, burada
// rpm_teker = motorRpmIsWheelRpm ? rpm : rpm / gearRatio (bkz. VehicleParams.h
// MOTOR_RPM_IS_WHEEL_RPM). Native testler VehicleParams.h'deki (gerçek
// değerler geldiğinde değişecek) sabitlere değil, kendi yerel D/GR
// değerlerine bağlı kalabilsin diye D/GR/motorRpmIsWheelRpm parametrik
// tutuldu — üretim kodu aşağıdaki 1-parametreli rpmToSpeedKmhX10()
// sarmalayıcısını kullanır.
//
// NOT: TEL_SPD_X10_MAX'e clamp gercek hizi gizleyebilir, ama gercek
// WHEEL_DIAMETER_M / GEAR_RATIO degerleri girildiginde 300 km/h zaten
// fiziksel olarak erisilemez bir deger olacaktir. Mevcut placeholder
// degerlerle (D=0.5, GR=1.0) rpm~3184 ustunde bu sinir asilir ve clamp
// olmadan UKS Decode_Line paketin tamamini reddeder (Parse_Int f[18]
// 0..3000 sinirini asar).
static inline uint16_t rpmToSpeedKmhX10Impl(int16_t rpm, float wheelDiameterM,
                                            float gearRatio,
                                            bool motorRpmIsWheelRpm) {
    const float absRpm = (rpm < 0) ? (float)(-rpm) : (float)rpm;
    const float wheelRpm =
        motorRpmIsWheelRpm ? absRpm : (absRpm / gearRatio);
    const float km_h = wheelRpm * 3.14159265f * wheelDiameterM * 60.0f / 1000.0f;
    const float spd_x10 = km_h * 10.0f;
    if (spd_x10 >= (float)TEL_SPD_X10_MAX) return TEL_SPD_X10_MAX;
    return (uint16_t)spd_x10;
}

static inline uint16_t rpmToSpeedKmhX10(int16_t rpm) {
    return rpmToSpeedKmhX10Impl(rpm, WHEEL_DIAMETER_M, GEAR_RATIO,
                                MOTOR_RPM_IS_WHEEL_RPM);
}

// TelemetryData artık VehicleData.h'de (yukarıda include edildi) — M3.

class Telemetry {
   public:
    Telemetry();
    bool begin();

    // Formats TelemetryData into the AKS->UKS ASCII CSV frame and writes it
    // to UART. Format: TEL,ver,seq,rpm,torque,motorErr,motorValid,
    // motorTimeout,cellVMax,cellVMin,tempH,tempL,sysState,packV,current,soc,
    // bmsValid,tsMs,spdX10\r\n (19 alan; UKS Core/Src/telemetry.c
    // Decode_Line ile sozlesmeli, bkz. tools/e2e/contract.py).
    void sendStatus(const TelemetryData& TEL_data);

   private:
    bool TEL_isInitialized;
    uint32_t TEL_sequenceCounter;
};
