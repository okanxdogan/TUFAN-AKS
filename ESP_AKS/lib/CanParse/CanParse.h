#pragma once
//
// Saf CAN payload parser'ları — donanım, mutex ve global state bağımlılığı
// yoktur. Bayt dizisini struct'a dönüştürürler. CanManager'ın handleXxx
// metodları bu fonksiyonları çağırır; native testler aynı fonksiyonları
// doğrudan çağırarak DLC kontrolü, big-endian dönüşüm ve signed cast
// mantığını izole eder.
//
#include <cstdint>
#include "VehicleData.h"  // TelemetryData (M3: LoRa/VehicleParams bağımlılığı yok)
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"

// Motor sürücüsünden CAN üzerinden gelen anlık motor durumu.
// MSTest/mock_motor ile doğrulanmış bayt dizilimi:
//   data[0:1] = RPM (big-endian int16)
//   data[2:3] = Voltaj (big-endian uint16, raw * 0.1 = V, ör: 240 → 24.0 V)
//   data[4:6] = Rezerve (kullanılmıyor)
//   data[7]   = Hata bayrakları / motor durumu (0x01=çalışıyor, 0x00=durdu)
struct MotorStatus {
    int16_t  rpm;
    uint16_t motorVoltageDeciV;   // raw * 0.1 = V (720 = 72.0 V, 16-bit — data[2:3])
    uint8_t  errorFlags;          // data[7] & 0xFE: sadece gerçek hata bayrakları
    bool     isRunning;           // data[7] & 0x01: motor çalışma durumu
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

// Ham deci-mV (0.1 mV çözünürlük, ör. 33579 = 3357.9 mV) → mV, EN YAKINA
// YUVARLAYARAK (kesme DEĞİL): 33579 → 3358, 33550 → 3355. Eski `/ 10` kesmesi
// ekranda ±1 mV delta hatası bırakıyordu. raw uint16 olduğundan +5 taşmasını
// önlemek için uint32'ye yükseltme ŞART.
inline uint16_t deciMvToMv(uint16_t raw) {
    return (uint16_t)(((uint32_t)raw + 5U) / 10U);
}

// Motor status frame (CAN ID 0x200 = CAN_ID_MOTOR_STATUS, 11-bit STD).
// Kaynak: motor sürücüsü (MOTOR_DRIVER_PRESENT=1 olduğunda) VEYA
// hall-effect hız sensörü ünitesi (esp32-canbus-speed-sensor, yalnızca
// data[0:1]=RPM doldurur, data[2:7]=0x00 — bkz.
// Documents/MOTOR_ENTEGRASYON_NOTU.md).
// DLC ≥ 8 olmalı; aksi halde false döner ve `out` değiştirilmez.
//   data[0:1] = RPM (big-endian uint16)
//   data[2:3] = Voltaj (big-endian uint16, raw * 0.1 = V)
//   data[7]   = Hata bayrakları
// Başarıda `out.isValid = true` set edilir.
bool parseMotorStatus(const twai_message_t& msg, MotorStatus& out);

// =========================================================================
// Lithium Balance c-BMS — 29-bit Extended CAN ID'ler
// =========================================================================

// 0xE000 — tüm alanlar DOĞRULANDI (bkz. Documents/CAN_Message_Table.md):
//   byte[0:1] = Pack Current, big-endian int16 — DOĞRULANDI, çarpan 0.1A
//   byte[2:3] = Pack Voltage, big-endian uint16, raw * 0.1 = V — DOĞRULANDI
//   byte[4:5] = SoC 1, big-endian uint16, raw * 0.01 = % — DOĞRULANDI
//   byte[6:7] = SoC 2, big-endian uint16, raw * 0.01 = % — DOĞRULANDI
// DLC ≥ 8 olmalı; aksi halde false döner.
// Yazılan alanlar: TEL_bmsPackVoltageDeciV, TEL_bmsCurrentCentiA,
//   TEL_bmsSocHundredths, TEL_bmsSoc2Hundredths, TEL_bmsDataValid (=true).
bool parseLbBmsE000(const twai_message_t& msg, TelemetryData& out);

// 0x1806E5F4 — Charger komut frame'i (BMS -> Charger; AKS yalnızca dinler).
// DOĞRULANDI (sniffer Oturum 2; J1939: PGN 0x1806, DA 0xE5, SA 0xF4):
//   byte[0:1] = şarj voltaj hedefi, big-endian uint16, raw * 0.1 = V
//   byte[2:3] = şarj akım hedefi, big-endian uint16, raw * 0.1 = A
// DLC < 4 ise false döner ve `out` değiştirilmez.
bool parseCharger1806E5F4(const twai_message_t& msg, ChargerCommand& out);

// 0xE001 — Sıcaklık ve Hücre Özeti DOĞRULANDI (bkz. CAN_Message_Table.md)
//   byte[0:1]=min hücre mV, byte[2:3]=max hücre mV, byte[4:5]=avg
//   hücre mV (DOĞRULANDI), byte[6:7]=sıcaklık (DOĞRULANDI)
// TEL_bmsTempHighestC = max(temp1, temp2), TEL_bmsTempLowestC = min(temp1, temp2).
bool parseLbBmsE001(const twai_message_t& msg, TelemetryData& out);

// 0xE002 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE002(const twai_message_t& msg, TelemetryData& out);

// 0xE003 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE003(const twai_message_t& msg, TelemetryData& out);

// 0xE004 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE004(const twai_message_t& msg, TelemetryData& out);

// 0xE005 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE005(const twai_message_t& msg, TelemetryData& out);

// 0xE006 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE006(const twai_message_t& msg, TelemetryData& out);

// 0xE032 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE032(const twai_message_t& msg, TelemetryData& out);

// 0xE033 — TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
bool parseLbBmsE033(const twai_message_t& msg, TelemetryData& out);

// 0xE015 — hücre 0-3 voltajı, DLC>=8, DOĞRULANDI
bool parseLbBmsE015(const twai_message_t& msg, TelemetryData& out);
// 0xE016 — hücre 4-7 voltajı, DLC>=8, DOĞRULANDI
bool parseLbBmsE016(const twai_message_t& msg, TelemetryData& out);
// 0xE017 — hücre 8-11 voltajı, DLC>=8, DOĞRULANDI
bool parseLbBmsE017(const twai_message_t& msg, TelemetryData& out);
// 0xE018 — hücre 12-15 voltajı, DLC>=8, DOĞRULANDI
bool parseLbBmsE018(const twai_message_t& msg, TelemetryData& out);
// 0xE019 — hücre 16-19 voltajı, DLC>=8, DOĞRULANDI
bool parseLbBmsE019(const twai_message_t& msg, TelemetryData& out);
// 0xE020 — hücre 20-23 voltajı, DLC>=8, DOĞRULANDI
bool parseLbBmsE020(const twai_message_t& msg, TelemetryData& out);

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
