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

// Charger komut frame'i (CAN ID 0x1806E5F4, BMS -> Charger) çıktısı.
// GEÇİCİ çıktı tipi: TelemetryData'ya charger setpoint alanlarının eklenmesi
// sonraki adıma bırakıldı; şimdilik parser bu struct'a yazar.
// Her iki alan da DOĞRULANDI (sniffer Oturum 2 + J1939 şarj protokolü).
struct ChargerCommand {
    uint16_t chargeVoltageSetpointDeciV;  // byte[0:1] — raw × 0.1 = V — DOĞRULANDI
    uint16_t chargeCurrentSetpointDeciA;  // byte[2:3] — raw × 0.1 = A — DOĞRULANDI
};

namespace CanParse {

// Motor status frame (CAN ID 0x200). DLC ≥ 4 olmalı; aksi halde false döner ve
// `out` değiştirilmez. DLC ≥ 5 ise data[4] errorFlags olarak alınır, aksi
// halde 0. Başarıda `out.isValid = true` set edilir.
bool parseMotorStatus(const twai_message_t& msg, MotorStatus& out);

// =========================================================================
// Lithium Balance c-BMS — 29-bit Extended CAN ID'ler
// =========================================================================

// 0xE000 — alan bazında güven seviyeleri (bkz. Documents/CAN_Message_Table.md):
//   byte[2:3] = Pack Voltage, big-endian uint16, raw * 0.1 = V
//               — DOĞRULANDI (2 bağımsız sniffer oturumu)
//   byte[0:1] = pack akımı adayı, big-endian int16 — HIPOTEZ-orta,
//               UNVERIFIED — scale unknown; HAM olarak TEL_bmsE000RawCurrent'a
//               yazılır, TEL_bmsCurrentCentiMa'ya YAZILMAZ.
//   byte[4:5] = kapasite/sayaç adayı, big-endian uint16 — HIPOTEZ-düşük,
//               UNVERIFIED — scale unknown; HAM olarak TEL_bmsE000RawCounter1.
//   byte[6:7] = kapasite/sayaç adayı, big-endian uint16 — HIPOTEZ-düşük,
//               UNVERIFIED — scale unknown; HAM olarak TEL_bmsE000RawCounter2
//               (yalnızca DLC ≥ 8 ise okunur, aksi halde 0).
// DLC ≥ 6 olmalı; aksi halde false döner (mevcut sözleşme korunur).
// Yazılan alanlar: TEL_bmsPackVoltageDeciV, TEL_bmsE000Raw*, TEL_bmsDataValid (=true).
bool parseLbBmsE000(const twai_message_t& msg, TelemetryData& out);

// 0x1806E5F4 — Charger komut frame'i (BMS -> Charger; AKS yalnızca dinler).
// DOĞRULANDI (sniffer Oturum 2; J1939: PGN 0x1806, DA 0xE5, SA 0xF4):
//   byte[0:1] = şarj voltaj hedefi, big-endian uint16, raw * 0.1 = V
//   byte[2:3] = şarj akım hedefi, big-endian uint16, raw * 0.1 = A
// DLC < 4 ise false döner ve `out` değiştirilmez.
bool parseCharger1806E5F4(const twai_message_t& msg, ChargerCommand& out);

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

// Pack voltajı güvenlik eşiği sonucu (yalnızca DOĞRULANMIŞ packV alanına
// uygulanır — bkz. SystemConfig.h "Phase 2 Safety Thresholds").
enum class BmsPackVoltageFault : uint8_t {
    NONE = 0,
    UNDERVOLTAGE = 1,
    OVERVOLTAGE = 2,
};

// Saf eşik kontrolü: packVoltageDeciV <= criticalMinDeciV -> UNDERVOLTAGE,
// >= criticalMaxDeciV -> OVERVOLTAGE, aksi halde NONE. Eşikler parametrik —
// üretim kodu SystemConfig.h CRITICAL sabitlerini geçirir, native testler
// kendi değerlerini verebilir. (<=/>= semantiği VcuLogic hasCriticalCondition
// ile aynıdır.)
BmsPackVoltageFault checkPackVoltageFault(uint16_t packVoltageDeciV,
                                          uint16_t criticalMinDeciV,
                                          uint16_t criticalMaxDeciV);

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
