#pragma once

#include <cstdint>

#include "VehicleParams.h"  // WHEEL_DIAMETER_M / GEAR_RATIO / MOTOR_RPM_IS_WHEEL_RPM — TEK doğruluk kaynağı

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
static inline uint16_t rpmToSpeedKmhX10Impl(uint16_t rpm, float wheelDiameterM,
                                            float gearRatio,
                                            bool motorRpmIsWheelRpm) {
    const float wheelRpm =
        motorRpmIsWheelRpm ? (float)rpm : ((float)rpm / gearRatio);
    const float km_h = wheelRpm * 3.14159265f * wheelDiameterM * 60.0f / 1000.0f;
    const float spd_x10 = km_h * 10.0f;
    if (spd_x10 >= (float)TEL_SPD_X10_MAX) return TEL_SPD_X10_MAX;
    return (uint16_t)spd_x10;
}

static inline uint16_t rpmToSpeedKmhX10(uint16_t rpm) {
    return rpmToSpeedKmhX10Impl(rpm, WHEEL_DIAMETER_M, GEAR_RATIO,
                                MOTOR_RPM_IS_WHEEL_RPM);
}

struct TelemetryData {
    uint16_t TEL_motorRpm;
    int16_t TEL_motorTorqueFeedback;
    uint8_t TEL_motorErrorFlags;
    bool TEL_motorDataValid;
    bool TEL_motorTimeoutActive;

    // Lithium Balance c-BMS — alanlar henüz çözülmemiş ID'lerden gelecek
    // Bu alanlar TelemetryData yapısında kalıyor çünkü telemetri, HMI ve
    // VcuLogic tüketici kodları bunları kullanıyor. İlgili CAN ID'lerin
    // reverse-engineering'i tamamlandıkça parse edilecek.
    uint16_t TEL_bmsCellVoltageMaxDeciMv;  // DOĞRULANMADI — kaynak ID bilinmiyor
    uint16_t TEL_bmsCellVoltageMinDeciMv;  // DOĞRULANMADI — kaynak ID bilinmiyor
    int8_t TEL_bmsTempHighestC;            // DOĞRULANMADI — kaynak ID bilinmiyor
    int8_t TEL_bmsTempLowestC;             // DOĞRULANMADI — kaynak ID bilinmiyor
    uint8_t TEL_bmsSystemState;            // DOĞRULANMADI — kaynak ID bilinmiyor

    // Lithium Balance c-BMS — CAN ID 0xE000 byte[2:3] (DOĞRULANDI)
    uint16_t TEL_bmsPackVoltageDeciV;  // raw * 0.1 = V — DOĞRULANDI
    int32_t TEL_bmsCurrentCentiMa;     // DOĞRULANMADI — kaynak ID bilinmiyor
    uint16_t TEL_bmsSocHundredths;     // DOĞRULANMADI — kaynak ID bilinmiyor

    bool TEL_bmsDataValid;

    uint32_t TEL_timestampMs   = 0;   // ms since boot — stamped when packet is created
    uint16_t TEL_speedKmhX10  = 0;   // vehicle speed ×10 km/h, filled via rpmToSpeedKmhX10()
};

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
