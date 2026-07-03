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

// =========================================================================
// Lithium Balance c-BMS — 29-bit Extended CAN ID'ler
// =========================================================================

// 0xE000 — DOĞRULANDI (reverse-engineering + CAN sniffer ile)
// Çözülmüş alanlar:
//   byte[2:3] = Pack Voltage, big-endian uint16, raw * 0.1 = V
//   byte[0:1] ve byte[4:5] henüz bilinmiyor — parse EDİLMİYOR.
// DLC ≥ 6 olmalı; aksi halde false döner.
// Yazılan alanlar: TEL_bmsPackVoltageDeciV, TEL_bmsDataValid (=true).
bool parseLbBmsE000(const twai_message_t& msg, TelemetryData& out);

// 0xE001 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
// İleride alan anlamı çözüldüğünde gerçek parse eklenir.
bool parseLbBmsE001(const twai_message_t& msg, TelemetryData& out);

// 0xE002 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE002(const twai_message_t& msg, TelemetryData& out);

// 0xE003 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE003(const twai_message_t& msg, TelemetryData& out);

// 0xE004 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE004(const twai_message_t& msg, TelemetryData& out);

// 0xE005 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE005(const twai_message_t& msg, TelemetryData& out);

// 0xE032 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE032(const twai_message_t& msg, TelemetryData& out);

// 0xE033 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE033(const twai_message_t& msg, TelemetryData& out);

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
