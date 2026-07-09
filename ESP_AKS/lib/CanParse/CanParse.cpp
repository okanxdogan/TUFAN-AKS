#include "CanParse.h"

namespace CanParse {

bool parseMotorStatus(const twai_message_t& msg, MotorStatus& out) {
    // MSTest/mock_motor ile doğrulanmış 8-byte payload:
    //   data[0] = RPM High Byte
    //   data[1] = RPM Low Byte
    //   data[2] = Rezerve (0x00)
    //   data[3] = Voltaj (raw * 0.1 = V, ör: 240 → 24.0 V)
    //   data[4] = Rezerve (0x00)
    //   data[5] = Rezerve (0x00)
    //   data[6] = Rezerve (0x00)
    //   data[7] = Hata bayrakları / motor durumu (0x01=çalışıyor, 0x00=durdu)
    if (msg.data_length_code < 8)
        return false;

    out.rpm = static_cast<int16_t>((msg.data[0] << 8) | msg.data[1]);
    out.motorVoltageDeciV = static_cast<uint16_t>((msg.data[2] << 8) | msg.data[3]);
    out.errorFlags = msg.data[7] & 0xFE; // 0x01 'çalışıyor' bitidir, hata sayılmaz
    out.isRunning = (msg.data[7] & 0x01) != 0;
    out.isValid = true;
    return true;
}

// =========================================================================
// Lithium Balance c-BMS — 29-bit Extended CAN ID'ler
// =========================================================================

// 0xE000 — DOĞRULANDI: byte[2:3] = Pack Voltage (big-endian uint16, deciV)
// byte[0:1] ve byte[4:5] henüz bilinmiyor — TelemetryData'ya yazılmıyor.
bool parseLbBmsE000(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 6)
        return false;

    // ÇÖZÜLMÜŞ: byte[2:3] = Pack Voltage, big-endian uint16, raw * 0.1 = V
    out.TEL_bmsPackVoltageDeciV =
        static_cast<uint16_t>((msg.data[2] << 8) | msg.data[3]);

    // byte[0:1]: BİLİNMİYOR — TelemetryData'ya yazılmıyor
    // byte[4:5]: BİLİNMİYOR — TelemetryData'ya yazılmıyor

    out.TEL_bmsDataValid = true;
    return true;
}

// --- Aşağıdaki fonksiyonlar, CAN sniffer loglarında görülen ancak alan
// --- anlamı henüz DOĞRULANMAMIŞ ID'ler içindir. Ham byte'ları kabul eder
// --- ama TelemetryData'ya anlam yüklenmez. İleride gerçek anlam çözüldükçe
// --- bu fonksiyonların içi doldurulacaktır.

// 0xE001 — TODO: alan anlamı doğrulanmadı
bool parseLbBmsE001(const twai_message_t& msg, TelemetryData& out) {
    (void)out;
    // TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
    // TelemetryData'ya HİÇBİR alan yazılmıyor.
    return msg.data_length_code > 0;
}

// 0xE002 — TODO: alan anlamı doğrulanmadı
bool parseLbBmsE002(const twai_message_t& msg, TelemetryData& out) {
    (void)out;
    // TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
    return msg.data_length_code > 0;
}

// 0xE003 — TODO: alan anlamı doğrulanmadı
bool parseLbBmsE003(const twai_message_t& msg, TelemetryData& out) {
    (void)out;
    // TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
    return msg.data_length_code > 0;
}

// 0xE004 — TODO: alan anlamı doğrulanmadı
bool parseLbBmsE004(const twai_message_t& msg, TelemetryData& out) {
    (void)out;
    // TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
    return msg.data_length_code > 0;
}

// 0xE005 — TODO: alan anlamı doğrulanmadı
bool parseLbBmsE005(const twai_message_t& msg, TelemetryData& out) {
    (void)out;
    // TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
    return msg.data_length_code > 0;
}

// 0xE032 — TODO: alan anlamı doğrulanmadı
bool parseLbBmsE032(const twai_message_t& msg, TelemetryData& out) {
    (void)out;
    // TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
    return msg.data_length_code > 0;
}

// 0xE033 — TODO: alan anlamı doğrulanmadı
bool parseLbBmsE033(const twai_message_t& msg, TelemetryData& out) {
    (void)out;
    // TODO: alan anlamı doğrulanmadı, ham byte'lar loglanıyor
    return msg.data_length_code > 0;
}

bool isMotorStatusTimedOut(bool hasSeen,
                           bool lastValid,
                           TickType_t now,
                           TickType_t lastTick,
                           TickType_t timeoutTicks) {
    if (!hasSeen || !lastValid)
        return false;
    return static_cast<TickType_t>(now - lastTick) >= timeoutTicks;
}

bool isBmsStatusTimedOut(bool hasSeen,
                         bool lastValid,
                         TickType_t now,
                         TickType_t lastTick,
                         TickType_t timeoutTicks) {
    if (!hasSeen || !lastValid)
        return false;
    return static_cast<TickType_t>(now - lastTick) >= timeoutTicks;
}

}  // namespace CanParse
