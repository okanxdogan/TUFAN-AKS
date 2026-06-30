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

bool parseSolionBmsA(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 7)
        return false;

    out.TEL_bmsCellVoltageMaxDeciMv =
        static_cast<uint16_t>((msg.data[0] << 8) | msg.data[1]);
    out.TEL_bmsCellVoltageMinDeciMv =
        static_cast<uint16_t>((msg.data[2] << 8) | msg.data[3]);
    out.TEL_bmsTempHighestC = static_cast<int8_t>(msg.data[4]);
    out.TEL_bmsTempLowestC  = static_cast<int8_t>(msg.data[5]);
    out.TEL_bmsSystemState  = msg.data[6];
    out.TEL_bmsDataValid    = true;
    return true;
}

bool parseSolionBmsB(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 8)
        return false;

    out.TEL_bmsPackVoltageDeciV =
        static_cast<uint16_t>((msg.data[0] << 8) | msg.data[1]);
    out.TEL_bmsCurrentCentiMa = static_cast<int32_t>(
        (static_cast<uint32_t>(msg.data[2]) << 24) |
        (static_cast<uint32_t>(msg.data[3]) << 16) |
        (static_cast<uint32_t>(msg.data[4]) << 8)  |
         static_cast<uint32_t>(msg.data[5]));
    out.TEL_bmsSocHundredths =
        static_cast<uint16_t>((msg.data[6] << 8) | msg.data[7]);
    out.TEL_bmsDataValid = true;
    return true;
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
