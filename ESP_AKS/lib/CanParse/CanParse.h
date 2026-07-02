#pragma once
//
// Saf CAN payload parser'ları — donanım, mutex ve global state bağımlılığı
// yoktur. Bayt dizisini struct'a dönüştürürler. CanManager'ın handleXxx
// metodları bu fonksiyonları çağırır; native testler aynı fonksiyonları
// doğrudan çağırarak DLC kontrolü, big-endian dönüşüm ve signed cast
// mantığını izole eder.
//
#include <cstdint>
#include "Telemetry.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"

// Motor sürücüsünden CAN üzerinden gelen anlık motor durumu.
struct MotorStatus {
    uint16_t rpm;
    int16_t  torqueFeedback;
    uint8_t  errorFlags;
    bool     isValid;
};

namespace CanParse {

// Motor status frame (CAN ID 0x200). DLC ≥ 4 olmalı; aksi halde false döner ve
// `out` değiştirilmez. DLC ≥ 5 ise data[4] errorFlags olarak alınır, aksi
// halde 0. Başarıda `out.isValid = true` set edilir.
bool parseMotorStatus(const twai_message_t& msg, MotorStatus& out);

// Solion SK BMS — CAN ID 0x111 (29-bit Extended), DLC = 8, Big Endian.
// Yazılan alanlar: TEL_bmsCellVoltageMaxDeciMv, TEL_bmsCellVoltageMinDeciMv,
// TEL_bmsTempHighestC, TEL_bmsTempLowestC, TEL_bmsSystemState,
// TEL_bmsDataValid (=true). Başarısızlıkta (DLC < 7) false döner.
bool parseSolionBmsA(const twai_message_t& msg, TelemetryData& out);

// Solion SK BMS — CAN ID 0x112 (29-bit Extended), DLC = 8, Big Endian.
// Yazılan alanlar: TEL_bmsPackVoltageDeciV, TEL_bmsCurrentCentiMa,
// TEL_bmsSocHundredths, TEL_bmsDataValid (=true).
// Başarısızlıkta (DLC < 8) false döner.
bool parseSolionBmsB(const twai_message_t& msg, TelemetryData& out);

// Motor status timeout: en az bir paket görülmüş AND son veri valid AND
// (now - lastTick) >= timeoutTicks. Diğer durumlarda false.
// TickType_t unsigned olduğundan wraparound doğal aritmetikle desteklenir.
bool isMotorStatusTimedOut(bool hasSeen,
                           bool lastValid,
                           TickType_t now,
                           TickType_t lastTick,
                           TickType_t timeoutTicks);

// BMS timeout: isMotorStatusTimedOut ile aynı mantık, BMS Config/Live
// ID'lerinden her biri için ayrı ayrı çağrılır.
bool isBmsStatusTimedOut(bool hasSeen,
                         bool lastValid,
                         TickType_t now,
                         TickType_t lastTick,
                         TickType_t timeoutTicks);

}  // namespace CanParse
