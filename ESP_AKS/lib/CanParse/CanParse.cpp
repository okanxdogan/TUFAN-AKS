#include "CanParse.h"

namespace CanParse {

bool parseMotorStatus(const twai_message_t& msg, MotorStatus& out) {
    if (msg.data_length_code < 4)
        return false;

    out.rpm = static_cast<uint16_t>((msg.data[0] << 8) | msg.data[1]);
    out.torqueFeedback =
        static_cast<int16_t>((msg.data[2] << 8) | msg.data[3]);
    out.errorFlags = (msg.data_length_code >= 5) ? msg.data[4] : 0;
    out.isValid = true;
    return true;
}

// =========================================================================
// Lithium Balance c-BMS — 29-bit Extended CAN ID'ler
// =========================================================================

// 0xE000 — alan bazında güven seviyeleri için bkz. CanParse.h ve
// Documents/CAN_Message_Table.md.
bool parseLbBmsE000(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 6)
        return false;

    // DOĞRULANDI (2 sniffer oturumu): byte[2:3] = Pack Voltage,
    // big-endian uint16, raw * 0.1 = V
    out.TEL_bmsPackVoltageDeciV =
        static_cast<uint16_t>((msg.data[2] << 8) | msg.data[3]);

    // HIPOTEZ-orta: byte[0:1] = pack akımı adayı, big-endian int16
    // (idle oturumda yalnızca -1/-2 gözlendi). UNVERIFIED — scale unknown:
    // ölçek katsayısı UYGULANMAZ, ham değer ayrı raw alanda tutulur ve
    // TEL_bmsCurrentCentiMa'ya YAZILMAZ.
    out.TEL_bmsE000RawCurrent =
        static_cast<int16_t>((msg.data[0] << 8) | msg.data[1]);

    // HIPOTEZ-düşük: byte[4:5] = kapasite/enerji sayacı adayı (oturumda
    // monoton azalıyordu). UNVERIFIED — scale unknown; ham tutulur.
    out.TEL_bmsE000RawCounter1 =
        static_cast<uint16_t>((msg.data[4] << 8) | msg.data[5]);

    // HIPOTEZ-düşük: byte[6:7] = ikinci kapasite/sayaç adayı. UNVERIFIED —
    // scale unknown; ham tutulur. Mevcut DLC ≥ 6 sözleşmesi korunduğu için
    // yalnızca DLC ≥ 8 ise okunur, aksi halde deterministik 0 yazılır.
    out.TEL_bmsE000RawCounter2 =
        (msg.data_length_code >= 8)
            ? static_cast<uint16_t>((msg.data[6] << 8) | msg.data[7])
            : 0;

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