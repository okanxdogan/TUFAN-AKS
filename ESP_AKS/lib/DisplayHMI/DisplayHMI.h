#pragma once
#include <cstddef>
#include <cstdint>

#include "HMIHelpers.h"

// HMI_VcuState ve saf metin/komut helper'ları HMIHelpers.h'ye taşındı; bu
// header geriye uyumluluk için onu yeniden expose eder. DisplayHMI sınıfı
// yalnızca init / cache / RX state'ini yönetir.
struct HMI_DisplayData {
    uint16_t HMI_currentSpeed;
    uint8_t HMI_currentBattery;
    uint16_t HMI_motorRpm;
    int16_t HMI_motorTorqueFeedback;
    uint8_t HMI_motorErrorFlags;
    bool HMI_motorDataValid;
    bool HMI_motorTimeoutActive;
    int16_t HMI_bmsTemperatureC;
    uint16_t HMI_bmsPackVoltageDeciV;
    uint16_t HMI_bmsCellVoltageMaxMv;  // gerçek BMS max hücre gerilimi, mV
    uint16_t HMI_bmsCellVoltageMinMv;  // gerçek BMS min hücre gerilimi, mV
    bool HMI_contactorClosed;
    HMI_VcuState HMI_vcuState;
};

class DisplayHMI {
   private:
    bool HMI_isInitialized;
    bool HMI_hasCachedScreen;
    HMI_DisplayData HMI_lastScreenData;

    void HMI_drainRxBuffer();

   public:
    DisplayHMI();
    bool begin();
    void updateScreen(const HMI_DisplayData& HMI_data);
    bool readTouchCommand(uint8_t& HMI_command);
};
