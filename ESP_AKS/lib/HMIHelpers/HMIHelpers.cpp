#include "HMIHelpers.h"

#include <cstring>

#include "SystemConfig.h"
#include "driver/uart.h"

void HMI_sendEndBytes(void) {
    const uint8_t HMI_endBytes[3] = {0xFF, 0xFF, 0xFF};
    uart_write_bytes(HMI_UART_NUM, reinterpret_cast<const char*>(HMI_endBytes),
                     3);
}

void HMI_sendNumericIfChanged(const char* HMI_component, int32_t HMI_value,
                              int32_t HMI_lastValue, bool HMI_force) {
    if (!HMI_force && HMI_value == HMI_lastValue)
        return;

    char HMI_command[48];
    const int HMI_commandLen =
        snprintf(HMI_command, sizeof(HMI_command), "%s.val=%ld", HMI_component,
                 static_cast<long>(HMI_value));
    if (HMI_commandLen <= 0)
        return;

    uart_write_bytes(HMI_UART_NUM, HMI_command, HMI_commandLen);
    HMI_sendEndBytes();
}

void HMI_sendPicIfChanged(const char* HMI_component, int32_t HMI_picId,
                          int32_t HMI_lastPicId, bool HMI_force) {
    if (!HMI_force && HMI_picId == HMI_lastPicId)
        return;

    char HMI_command[48];
    const int HMI_commandLen = HMI_formatPicCommand(
        HMI_command, sizeof(HMI_command), HMI_component, HMI_picId);
    if (HMI_commandLen <= 0)
        return;

    uart_write_bytes(HMI_UART_NUM, HMI_command, HMI_commandLen);
    HMI_sendEndBytes();
}

void HMI_sendTextIfChanged(const char* HMI_component, const char* HMI_value,
                           const char* HMI_lastValue, bool HMI_force) {
    if (!HMI_force && std::strcmp(HMI_value, HMI_lastValue) == 0)
        return;

    char HMI_command[64];
    const int HMI_commandLen =
        snprintf(HMI_command, sizeof(HMI_command), "%s.txt=\"%s\"",
                 HMI_component, HMI_value);
    if (HMI_commandLen <= 0)
        return;

    uart_write_bytes(HMI_UART_NUM, HMI_command, HMI_commandLen);
    HMI_sendEndBytes();
}
