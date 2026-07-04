#include "CanParse.h"
#include "VehicleParams.h"
#include <math.h>

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

// RPM verisinden aracın hızını km/h cinsinden hesaplayan fonksiyon
float calculate_vehicle_speed_kmh(int32_t motor_rpm) {
    float wheel_rpm = 0.0f;

    // RPM zaten tekerlek hızı mı yoksa motor mili hızı mı kontrolü
    if (MOTOR_RPM_IS_WHEEL_RPM) {
        wheel_rpm = (float)motor_rpm;
    } else {
        wheel_rpm = (float)motor_rpm / GEAR_RATIO;
    }

    // Çevre (m) = pi * D
    float wheel_circumference = M_PI * WHEEL_DIAMETER_M;

    // Hız (m/dakika) = Tekerlek RPM * Çevre
    float speed_m_per_min = wheel_rpm * wheel_circumference;

    // Hız (km/h) = (m/dakika) * 60 / 1000 -> yani 0.06 ile çarpmak
    float speed_kmh = speed_m_per_min * 0.06f;

    // Geri vites veya hatalı durumlar için mutlak değer veya sınır kontrolü eklenebilir
    if (speed_kmh < 0.0f) {
        speed_kmh = -speed_kmh; 
    }

    return speed_kmh;
}