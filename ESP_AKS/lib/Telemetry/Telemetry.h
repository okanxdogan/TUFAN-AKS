#pragma once

#include <cstdint>

// --- Hız dönüşüm sabitleri (araç geometrisi netleştikten sonra güncelle) ---
#define WHEEL_DIAMETER_M 0.5f   // TODO: gerçek tekerlek çapı (m)
#define GEAR_RATIO       1.0f   // TODO: gerçek dişli oranı

// km/h = rpm * PI * D * 60 / (GEAR_RATIO * 1000)   →   x10 hassasiyet
static inline uint16_t rpmToSpeedKmhX10(uint16_t rpm) {
    const float km_h = (float)rpm * 3.14159265f * WHEEL_DIAMETER_M
                       * 60.0f / (GEAR_RATIO * 1000.0f);
    const float spd_x10 = km_h * 10.0f;
    if (spd_x10 >= 65535.0f) return 65535u;
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

    uint32_t TEL_timestampMs   = 0;   // boot'tan beri ms — paket oluşturulduğu anda damgalanır
    uint16_t TEL_speedKmhX10  = 0;   // araç hızı ×10 km/h, rpmToSpeedKmhX10() ile doldurulur
};

class Telemetry {
   public:
    Telemetry();
    bool begin();
    void sendStatus(const TelemetryData& TEL_data);

   private:
    bool TEL_isInitialized;
    uint32_t TEL_sequenceCounter;
};
