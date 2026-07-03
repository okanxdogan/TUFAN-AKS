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
    TelemetryData getTelemetryData() const;

   private:
    void handleMotorStatus(const twai_message_t& msg);

    // Lithium Balance c-BMS handler'ları
    void handleLbBmsE000(const twai_message_t& msg);   // packV — DOĞRULANDI
    void handleLbBmsStub(const twai_message_t& msg, uint32_t canId);  // diğer ID'ler — DOĞRULANMADI

    void updateMotorStatusValidity();
    void updateBmsValidity();
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

    // BMS freshness tracking — E000 (packV, doğrulanmış veri kaynağı)
    TickType_t CAN_lastBmsE000Tick = 0;
    bool CAN_hasSeen_BmsE000 = false;
    bool CAN_bmsE000Valid = false;
    bool CAN_bmsTimeoutLogged = false;

    // UKS aralik-disi alan sanitizasyonu icin throttle'li WARN log
    // zaman damgalari (bkz. getTelemetryData / TelemetrySanitize.h).
    mutable TickType_t CAN_lastSysStateSanitizeWarnTick = 0;
    mutable TickType_t CAN_lastSocSanitizeWarnTick = 0;
    mutable TickType_t CAN_lastCurrentSanitizeWarnTick = 0;

    CAN_EventCallback CAN_eventCallback = nullptr;
    void* CAN_eventContext = nullptr;
};
