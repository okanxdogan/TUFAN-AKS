#pragma once
//
// Saf HMI helper'ları + Nextion UART komut yardımcıları.
// 4 saf metin dönüşümü inline; donanım bağımlılıkları yoktur.
// 3 UART helper'ı (sendEndBytes, sendNumericIfChanged, sendTextIfChanged)
// uart_write_bytes çağırdığı için CPP tarafında tanımlıdır.
// DisplayHMI sınıfı bu fonksiyonları çağırır; native testler aynı
// fonksiyonları doğrudan çağırarak metin formatlama ve cache mantığını
// izole eder.
//
#include <cstddef>
#include <cstdint>
#include <cstdio>

enum class HMI_VcuState : uint8_t {
    INIT = 0,
    IDLE = 1,
    READY = 2,
    DRIVE = 3,
    EMERGENCY_STOP = 4,
    FAULT = 5
};

inline const char* HMI_getStateText(HMI_VcuState HMI_state) {
    switch (HMI_state) {
        case HMI_VcuState::INIT:           return "INIT";
        case HMI_VcuState::IDLE:           return "IDLE";
        case HMI_VcuState::READY:          return "READY";
        case HMI_VcuState::DRIVE:          return "DRIVE";
        case HMI_VcuState::EMERGENCY_STOP: return "ESTOP";
        case HMI_VcuState::FAULT:          return "FAULT";
        default:                           return "UNK";
    }
}

inline void HMI_formatErrorText(uint8_t HMI_errorFlags, char* HMI_output,
                                size_t HMI_outputSize) {
    if (HMI_outputSize == 0)
        return;
    snprintf(HMI_output, HMI_outputSize, "0x%02X", HMI_errorFlags);
}

inline const char* HMI_getValidityText(bool HMI_dataValid,
                                       bool HMI_timeoutActive) {
    if (HMI_timeoutActive)
        return "TIMEOUT";
    return HMI_dataValid ? "VALID" : "INVALID";
}

inline const char* HMI_getContactorText(bool HMI_contactorClosed) {
    return HMI_contactorClosed ? "CLOSED" : "OPEN";
}

// --- Kaynak sinyal durumu ve "veri yok" gösterimi ---
//
// TEL_bmsSocHundredths → 0xE000 byte[4:5], DOĞRULANDI (SoC 1).
// TEL_bmsTempHighestC → 0xE001 byte[6:7], DOĞRULANDI (max(temp1,temp2)).
// TEL_bmsCellVoltage* → kaynak CAN ID BİLİNMİYOR.
//
// TEL_bmsDataValid=false (BMS hiç görülmedi / timeout) durumunda
// sentinel gönderilir — bayat/yok veri asla sayı gibi gösterilmez.
//
// Bkz. Documents/CAN_Message_Table.md (tek doğruluk kaynağı).
constexpr uint8_t HMI_BATTERY_NO_DATA = 255;
constexpr int16_t HMI_TEMP_NO_DATA = -127;
constexpr uint16_t HMI_CELL_VOLTAGE_NO_DATA = 65535;

constexpr bool HMI_SOC_SOURCE_VERIFIED = true;          // TEL_bmsSocHundredths — ÇÖZÜLDÜ (0xE000)
constexpr bool HMI_TEMP_SOURCE_VERIFIED = true;         // TEL_bmsTempHighestC — ÇÖZÜLDÜ (0xE001)
constexpr bool HMI_CELL_VOLTAGE_SOURCE_VERIFIED = true; // TEL_bmsCellVoltages[24], TEL_bmsCellVoltageMax/MinDeciMv DOĞRULANDI — kaynak: E015-E020 (hücre voltajı) + E001 byte[0:5] (min/max/avg)

inline uint8_t HMI_batteryDisplayValue(bool HMI_sourceVerified,
                                       bool HMI_bmsDataValid,
                                       uint16_t HMI_socHundredths) {
    if (!HMI_sourceVerified || !HMI_bmsDataValid)
        return HMI_BATTERY_NO_DATA;
    const uint16_t HMI_percent = HMI_socHundredths / 100U;
    return (HMI_percent > 100U) ? 100U : static_cast<uint8_t>(HMI_percent);
}

inline int16_t HMI_temperatureDisplayValue(bool HMI_sourceVerified,
                                           bool HMI_bmsDataValid,
                                           int16_t HMI_temperatureC) {
    if (!HMI_sourceVerified || !HMI_bmsDataValid)
        return HMI_TEMP_NO_DATA;
    return HMI_temperatureC;
}

// --- Nextion "float" (xfloat) ölçekleme — packv ve packa ---
//
// Nextion tarafında `packv` 1 ondalıklı ("00.0"), `packa` 2 ondalıklı
// ("00.00") float (xfloat) bileşenidir. xfloat, gönderilen tamsayı `.val`
// değerini ondalık hane sayısına göre yorumlar: 1 ondalıkta
// `gerçek_değer * 10`, 2 ondalıkta `gerçek_değer * 100`. Firmware `.val`'ı
// buna uygun ölçekte göndermelidir. (temp hâlâ tam sayı `number`
// bileşeni — ölçeklenmez.)
//
// packv kaynağı deciV (×0.1 V) taşır; bu ZATEN V*10'a eşittir → 1 ondalıklı
// xfloat için ek ölçekleme GEREKMEZ, değer doğrudan gönderilir.
//   örn. 800 deciV = 80.0 V → 800 → "80.0"
inline int32_t HMI_packVoltageToXfloat(uint16_t HMI_packVoltageDeciV) {
    return static_cast<int32_t>(HMI_packVoltageDeciV);
}

// packa kaynağı centiA (×0.01 A) taşır; bu ZATEN A*100'e eşittir → xfloat için
// ek ölçekleme GEREKMEZ, değer doğrudan gönderilir.
//   örn. 1250 centiA = 12.5 A → 1250 → "12.50"
inline int32_t HMI_packCurrentToXfloat(int32_t HMI_packCurrentCentiA) {
    return HMI_packCurrentCentiA;
}

// --- Nextion Picture bileşeni komut üreticisi (far.pic durum göstergesi) ---
//
// Far (şartname B2 9.19.c) durumu ekranda bir Picture bileşeniyle (objname
// "far") gösterilir; komut biçimi "<component>.pic=<picId>" (ör. "far.pic=1").
// picId = HMI_PIC_HEADLIGHT_ON / HMI_PIC_HEADLIGHT_OFF (SystemConfig.h, CONFIG).
//
// HMI_formatPicCommand SAF'tır: yalnız out buffer'a formatlar, UART'a DOKUNMAZ
// (native testler doğrudan doğrular). snprintf dönüşünü (yazılacak karakter
// sayısı) döndürür; <=0 veya >=outSize hata/kesme demektir (end-byte'lar HARİÇ).
inline int HMI_formatPicCommand(char* HMI_out, size_t HMI_outSize,
                                const char* HMI_component, int32_t HMI_picId) {
    return snprintf(HMI_out, HMI_outSize, "%s.pic=%ld", HMI_component,
                    static_cast<long>(HMI_picId));
}

// Nextion UART tarafı — implementasyonu HMIHelpers.cpp içindedir.
void HMI_sendEndBytes(void);

// Yalnızca yeni değer önceki cache'ten farklıysa (veya force=true) UART'a
// "{component}.val={value}\xFF\xFF\xFF" yazar.
void HMI_sendNumericIfChanged(const char* HMI_component, int32_t HMI_value,
                              int32_t HMI_lastValue, bool HMI_force);

// Aynı şekilde text varyantı: "{component}.txt=\"{value}\"\xFF\xFF\xFF".
void HMI_sendTextIfChanged(const char* HMI_component, const char* HMI_value,
                           const char* HMI_lastValue, bool HMI_force);

// Picture varyantı: yalnızca picId önceki cache'ten farklıysa (veya force=true)
// "{component}.pic={picId}\xFF\xFF\xFF" yazar (sendNumericIfChanged ikizi;
// komut üretimi saf HMI_formatPicCommand'a delege edilir).
void HMI_sendPicIfChanged(const char* HMI_component, int32_t HMI_picId,
                          int32_t HMI_lastPicId, bool HMI_force);
