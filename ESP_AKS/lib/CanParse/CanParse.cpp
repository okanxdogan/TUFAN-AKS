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

// 0xE000 — alan bazında güven seviyeleri için bkz. CanParse.h ve
// Documents/CAN_Message_Table.md.
bool parseLbBmsE000(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8)
        return false;

    // DOĞRULANDI: byte[0:1] = Pack Current, int16_t, Çarpan 0.1A.
    // raw (0.1A birimli) × 10 = centi-Amper (0.01 A) → TEL_bmsCurrentCentiA;
    // SystemConfig.h akım eşikleri (BMS_*_CENTI_A) aynı birimde (G5 hizalaması).
    int16_t raw_current = static_cast<int16_t>((msg.data[0] << 8) | msg.data[1]);
    out.TEL_bmsCurrentCentiA = static_cast<int32_t>(raw_current) * 10;

    // DOĞRULANDI: byte[2:3] = Pack Voltage, uint16_t, Çarpan 0.1V
    out.TEL_bmsPackVoltageDeciV = static_cast<uint16_t>((msg.data[2] << 8) | msg.data[3]);

    // DOĞRULANDI: byte[4:5] = SoC 1, uint16_t, Çarpan 0.01%
    out.TEL_bmsSocHundredths = static_cast<uint16_t>((msg.data[4] << 8) | msg.data[5]);

    // DOĞRULANDI: byte[6:7] = SoC 2, uint16_t, Çarpan 0.01%
    out.TEL_bmsSoc2Hundredths = static_cast<uint16_t>((msg.data[6] << 8) | msg.data[7]);

    out.TEL_bmsDataValid = true;
    return true;
}

// 0x1806E5F4 — Charger komut frame'i (BMS -> Charger; AKS yalnızca dinler).
bool parseCharger1806E5F4(const twai_message_t& msg, ChargerCommand& out) {
    if (msg.data_length_code < 4)
        return false;

    // DOĞRULANDI: byte[0:1] = şarj voltaj hedefi, big-endian uint16, ×0.1 V
    out.chargeVoltageSetpointDeciV =
        static_cast<uint16_t>((msg.data[0] << 8) | msg.data[1]);

    // DOĞRULANDI: byte[2:3] = şarj akım hedefi, big-endian uint16, ×0.1 A
    out.chargeCurrentSetpointDeciA =
        static_cast<uint16_t>((msg.data[2] << 8) | msg.data[3]);

    return true;
}

// --- Aşağıdaki stub fonksiyonlar (0xE002–0xE005, 0xE032, 0xE033), CAN
// --- sniffer loglarında görülen ancak alan anlamı henüz DOĞRULANMAMIŞ
// --- ID'ler içindir. Ham byte'ları kabul eder ama TelemetryData'ya anlam
// --- yüklenmez; gerçek anlam çözüldükçe içleri doldurulacaktır.
// --- İSTİSNA: hemen aşağıdaki 0xE001 bu kapsamda DEĞİL — sıcaklık alanları
// --- (byte[6:7]) DOĞRULANDI ve parse ediliyor; yalnız byte[0:5] bilinmiyor.

// 0xE001 — Sıcaklık ve Hücre Özeti DOĞRULANDI (bkz. CAN_Message_Table.md)
bool parseLbBmsE001(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8)
        return false;

    // YENİ: byte[0:1]=min, byte[2:3]=max, byte[4:5]=avg (raw/10 = mV)
    out.TEL_bmsCellVoltageMinDeciMv = (msg.data[0] << 8) | msg.data[1];
    out.TEL_bmsCellVoltageMaxDeciMv = (msg.data[2] << 8) | msg.data[3];
    out.TEL_bmsCellVoltageAvgDeciMv = (msg.data[4] << 8) | msg.data[5];

    // DOĞRULANDI: byte[6] = Temp1, byte[7] = Temp2, int8_t, ofset yok, doğrudan °C.
    // Hangi kanalın daha sıcak olduğu garanti değil → max/min ile atanır.
    const int8_t temp1 = static_cast<int8_t>(msg.data[6]);
    const int8_t temp2 = static_cast<int8_t>(msg.data[7]);
    out.TEL_bmsTempHighestC = (temp1 >= temp2) ? temp1 : temp2;
    out.TEL_bmsTempLowestC  = (temp1 <= temp2) ? temp1 : temp2;

    return true;
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

bool parseLbBmsE015(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8) return false;
    out.TEL_bmsCellVoltages[0] = ((msg.data[0]<<8)|msg.data[1]) / 10;
    out.TEL_bmsCellVoltages[1] = ((msg.data[2]<<8)|msg.data[3]) / 10;
    out.TEL_bmsCellVoltages[2] = ((msg.data[4]<<8)|msg.data[5]) / 10;
    out.TEL_bmsCellVoltages[3] = ((msg.data[6]<<8)|msg.data[7]) / 10;
    return true;
}

bool parseLbBmsE016(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8) return false;
    out.TEL_bmsCellVoltages[4] = ((msg.data[0]<<8)|msg.data[1]) / 10;
    out.TEL_bmsCellVoltages[5] = ((msg.data[2]<<8)|msg.data[3]) / 10;
    out.TEL_bmsCellVoltages[6] = ((msg.data[4]<<8)|msg.data[5]) / 10;
    out.TEL_bmsCellVoltages[7] = ((msg.data[6]<<8)|msg.data[7]) / 10;
    return true;
}

bool parseLbBmsE017(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8) return false;
    out.TEL_bmsCellVoltages[8] = ((msg.data[0]<<8)|msg.data[1]) / 10;
    out.TEL_bmsCellVoltages[9] = ((msg.data[2]<<8)|msg.data[3]) / 10;
    out.TEL_bmsCellVoltages[10] = ((msg.data[4]<<8)|msg.data[5]) / 10;
    out.TEL_bmsCellVoltages[11] = ((msg.data[6]<<8)|msg.data[7]) / 10;
    return true;
}

bool parseLbBmsE018(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8) return false;
    out.TEL_bmsCellVoltages[12] = ((msg.data[0]<<8)|msg.data[1]) / 10;
    out.TEL_bmsCellVoltages[13] = ((msg.data[2]<<8)|msg.data[3]) / 10;
    out.TEL_bmsCellVoltages[14] = ((msg.data[4]<<8)|msg.data[5]) / 10;
    out.TEL_bmsCellVoltages[15] = ((msg.data[6]<<8)|msg.data[7]) / 10;
    return true;
}

bool parseLbBmsE019(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8) return false;
    out.TEL_bmsCellVoltages[16] = ((msg.data[0]<<8)|msg.data[1]) / 10;
    out.TEL_bmsCellVoltages[17] = ((msg.data[2]<<8)|msg.data[3]) / 10;
    out.TEL_bmsCellVoltages[18] = ((msg.data[4]<<8)|msg.data[5]) / 10;
    out.TEL_bmsCellVoltages[19] = ((msg.data[6]<<8)|msg.data[7]) / 10;
    return true;
}

bool parseLbBmsE020(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8) return false;
    out.TEL_bmsCellVoltages[20] = ((msg.data[0]<<8)|msg.data[1]) / 10;
    out.TEL_bmsCellVoltages[21] = ((msg.data[2]<<8)|msg.data[3]) / 10;
    out.TEL_bmsCellVoltages[22] = ((msg.data[4]<<8)|msg.data[5]) / 10;
    out.TEL_bmsCellVoltages[23] = ((msg.data[6]<<8)|msg.data[7]) / 10;
    return true;
}

BmsPackVoltageFault checkPackVoltageFault(uint16_t packVoltageDeciV,
                                          uint16_t criticalMinDeciV,
                                          uint16_t criticalMaxDeciV) {
    if (packVoltageDeciV <= criticalMinDeciV)
        return BmsPackVoltageFault::UNDERVOLTAGE;
    if (packVoltageDeciV >= criticalMaxDeciV)
        return BmsPackVoltageFault::OVERVOLTAGE;
    return BmsPackVoltageFault::NONE;
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