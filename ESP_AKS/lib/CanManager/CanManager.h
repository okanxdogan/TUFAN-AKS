#pragma once

#include <cstdint>
#include "CanParse.h"
#include "Telemetry.h"
#include "TelemetrySanitize.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// MotorStatus struct'ı CanParse.h içinde tanımlı (saf parser'larla paylaşılır).

enum class CAN_Event : uint8_t {
    NONE = 0,
    FAULT_DETECTED = 1,
};

using CAN_EventCallback = void (*)(CAN_Event CAN_event, void* CAN_context);

class CanManager {
   public:
    CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin);

    bool begin();
    void setEventCallback(CAN_EventCallback CAN_callback, void* CAN_context);

    // Send torque command to motor driver (CAN ID: 0x100)
    bool sendTorqueCommand(uint16_t torqueValue);

    // Dispatch one received message — call this in the CAN task loop
    void processRxMessages();

    // Thread-safe read of latest motor status
    MotorStatus getMotorStatus() const;

    // Returns raw internal telemetry. Sanitization happens only at the uplink gate in vTask_LoRa_UKS.
    TelemetryData getTelemetryData() const;

    // Thread-safe read of last charger command (0x1806E5F4, DOĞRULANDI).
    // `out` her zaman son görülen setpoint'lerle doldurulur; dönüş değeri
    // verinin taze olup olmadığını söyler (CAN_CHARGER_TIMEOUT_MS içinde
    // frame görüldüyse true). Charger akışı OPSİYONEL — false FAULT değildir.
    bool getChargerCommand(ChargerCommand& out) const;

   private:
    void handleMotorStatus(const twai_message_t& msg);

    // Lithium Balance c-BMS handler'ları
    void handleLbBmsE000(const twai_message_t& msg);        // packV, akım, SoC — DOĞRULANDI
    void handleLbBmsE001(const twai_message_t& msg);        // Sıcaklıklar — DOĞRULANDI
    void handleCharger1806E5F4(const twai_message_t& msg);  // setpoint'ler — DOĞRULANDI (AKS yalnızca dinler)
    void handleLbBmsStub(const twai_message_t& msg, uint32_t canId);  // diğer ID'ler — DOĞRULANMADI

    void updateMotorStatusValidity();
    void updateBmsValidity();
    void updateChargerValidity();
    void notifyFaultIfNeeded(uint8_t CAN_previousFlags, uint8_t CAN_currentFlags,
                             const char* CAN_faultSource);

    twai_general_config_t g_config;
    twai_timing_config_t t_config;
    twai_filter_config_t f_config;
    bool isInitialized = false;

    MotorStatus s_motorStatus = {};
    TelemetryData s_telemetryData = {};
    mutable SemaphoreHandle_t s_mutex = nullptr;

    TickType_t CAN_lastMotorStatusTick = 0;
    bool CAN_hasSeenMotorStatus = false;
    bool CAN_motorTimeoutLogged = false;

    bool CAN_busOffLogged = false;
    bool CAN_busRecoveredLogged = false;

    // BMS freshness tracking — E000 (packV, doğrulanmış veri kaynağı)
    TickType_t CAN_lastBmsE000Tick = 0;
    bool CAN_hasSeen_BmsE000 = false;
    bool CAN_bmsE000Valid = false;
    bool CAN_bmsTimeoutLogged = false;

    // Pack voltajı eşik ihlali bayrakları (bit0 = undervoltage,
    // bit1 = overvoltage). Motor errorFlags ile aynı edge-trigger deseni:
    // notifyFaultIfNeeded yalnızca değişimde CAN_Event yayınlar.
    uint8_t CAN_bmsPackFaultFlags = 0;

    // Charger freshness tracking — 0x1806E5F4 (OPSİYONEL akış).
    // Bayatlama yalnızca CAN_chargerValid'i düşürür; FAULT üretmez.
    ChargerCommand s_chargerCommand = {};
    TickType_t CAN_lastChargerTick = 0;
    bool CAN_hasSeenCharger = false;
    bool CAN_chargerValid = false;
    bool CAN_chargerStaleLogged = false;


    CAN_EventCallback CAN_eventCallback = nullptr;
    void* CAN_eventContext = nullptr;
};
