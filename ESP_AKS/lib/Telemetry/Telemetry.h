#pragma once

#include <cstdint>

// --- Speed conversion constants (update once vehicle geometry is confirmed) ---
#define WHEEL_DIAMETER_M 0.5f   // TODO: actual wheel diameter (m)
#define GEAR_RATIO       1.0f   // TODO: actual gear ratio

#define TEL_SPD_X10_MAX 3000  // UKS telemetry.c sanity siniri ile senkron
                              // — degistirilecekse iki tarafta birlikte
                              // degistirilmeli

// km/h = rpm * PI * D * 60 / (GEAR_RATIO * 1000)   →   x10 hassasiyet
//
// NOT: TEL_SPD_X10_MAX'e clamp gercek hizi gizleyebilir, ama gercek
// WHEEL_DIAMETER_M / GEAR_RATIO degerleri girildiginde 300 km/h zaten
// fiziksel olarak erisilemez bir deger olacaktir. Mevcut placeholder
// degerlerle (D=0.5, GR=1.0) rpm~3184 ustunde bu sinir asilir ve clamp
// olmadan UKS Decode_Line paketin tamamini reddeder (Parse_Int f[18]
// 0..3000 sinirini asar).
static inline uint16_t rpmToSpeedKmhX10(uint16_t rpm) {
    const float km_h = (float)rpm * 3.14159265f * WHEEL_DIAMETER_M
                       * 60.0f / (GEAR_RATIO * 1000.0f);
    const float spd_x10 = km_h * 10.0f;
    if (spd_x10 >= (float)TEL_SPD_X10_MAX) return TEL_SPD_X10_MAX;
    return (uint16_t)spd_x10;
}

struct TelemetryData {
    uint16_t TEL_motorRpm;
    int16_t TEL_motorTorqueFeedback;
    uint8_t TEL_motorErrorFlags;
    bool TEL_motorDataValid;
    bool TEL_motorTimeoutActive;

    // Solion SK BMS — CAN ID 0x111
    uint16_t TEL_bmsCellVoltageMaxDeciMv;  // raw * 0.1 = mV
    uint16_t TEL_bmsCellVoltageMinDeciMv;  // raw * 0.1 = mV
    int8_t TEL_bmsTempHighestC;
    int8_t TEL_bmsTempLowestC;
    uint8_t TEL_bmsSystemState;  // 1=Discharge, 2=IDLE, 3=Charge, 4=FAULT

    // Solion SK BMS — CAN ID 0x112
    uint16_t TEL_bmsPackVoltageDeciV;  // raw * 0.1 = V
    int32_t TEL_bmsCurrentCentiMa;     // raw * 0.01 = mA (+charge, -discharge)
    uint16_t TEL_bmsSocHundredths;     // raw * 0.01 = %

    bool TEL_bmsDataValid;

    uint32_t TEL_timestampMs   = 0;   // ms since boot — stamped when packet is created
    uint16_t TEL_speedKmhX10  = 0;   // vehicle speed ×10 km/h, filled via rpmToSpeedKmhX10()
};

class Telemetry {
   public:
    Telemetry();
    bool begin();

    // Formats TelemetryData into TEKNOFEST-compliant packet and writes to UART.
    // Format: zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh\r\n
    void sendStatus(const TelemetryData& TEL_data);

   private:
    bool TEL_isInitialized;
};
