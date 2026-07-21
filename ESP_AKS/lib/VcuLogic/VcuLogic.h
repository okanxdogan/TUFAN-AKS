#pragma once
#include <cstdint>
#include "IRelayActuator.h"
#include "SystemConfig.h"
#include "VehicleData.h"  // TelemetryData (M3)
#include "BmsAlgo.h"

// --- Görev 1: Sıcaklık eşiği derleme-zamanı kilitleri ---
// Şartname Bölüm 3, 6.e.iii: 55 uyarı / 70 kapanma, 15°C sabit aralık.
// Bu başlık hem SystemConfig.h'yi (VCU karar eşikleri) hem BmsAlgo.h'yi
// (HMI/ekran eşikleri) gördüğünden iki setin birbirinden kopmadığı kilit
// BURADA doğrulanır (BmsAlgo.h saf kalsın diye SystemConfig.h'yi include
// etmez; 15 °C aralık kilidi SystemConfig.h'nin kendisindedir).
static_assert(BMS_TEMP_OVERTEMP_WARN_C == BMS_WARN_MAX_TEMP_C,
              "HMI uyari sicaklik esigi (BmsAlgo.h) SystemConfig.h "
              "BMS_WARN_MAX_TEMP_C=55 ile ayni olmali (sartname 6.e.iii).");
static_assert(BMS_TEMP_OVERTEMP_CRIT_C == BMS_CRITICAL_MAX_TEMP_C,
              "HMI kritik sicaklik esigi (BmsAlgo.h) SystemConfig.h "
              "BMS_CRITICAL_MAX_TEMP_C=70 ile ayni olmali (sartname 6.e.iii).");

namespace VcuLogic {

enum class VcuState : uint8_t {
    INIT = 0,
    IDLE = 1,
    READY = 2,  // Contactors closed, HV bus live
    DRIVE = 3,
    EMERGENCY_STOP = 4,
    FAULT = 5
};

enum class VcuEvent : uint8_t {
    NONE = 0,
    START_REQUEST = 1,
    DRIVE_ENABLE = 2,
    EMERGENCY_STOP = 3,
    FAULT_DETECTED = 4,
    RESET = 5
};

// ---------------------------------------------------------------------------
// Pure safety predicates
// ---------------------------------------------------------------------------
// Donanım veya global state'ten bağımsız; doğrudan argümana bakar. Inline
// tutulduğu için saf mantık testlerinde VcuLogic.cpp linklenmeden
// çağrılabilir.
//
// EK B GÜVEN KURALI: hasWarningCondition/hasCriticalCondition YALNIZCA
// doğrulanmış sinyallere bakar — pack voltajı (0xE000 byte[2:3], DOĞRULANDI),
// akım (0xE000 byte[0:1], DOĞRULANDI — saha gözlemi Temmuz 2026), en yüksek
// hücre sıcaklığı (0xE001 byte[6:7], DOĞRULANDI), 24 hücre voltajı (E015-E020,
// DOĞRULANDI — yalnızca TEL_cellVoltageDataValid iken degerlendirilir) +
// BMS/motor freshness (BMS freshness G12 ile E000+E001 ID bazında ayrı ayrı
// izlenir; hücre voltajı freshness'ı CAN_cellVoltageSeenMask/E015-E020 ile
// ayrı izlenir, bkz. TEL_cellVoltageTimeoutActive).
// AÇIK İŞ: TEL_bmsSystemState==4 kontrolü kodda durur ama alan hiçbir CAN
// ID'den parse edilmediği için kaynak bağlanana kadar ETKİSİZDİR (aşağıya bkz.).

// Akım sinyali DOĞRULANDI (0xE000 byte[0:1], ×0.1A → centi-A, işaret: + şarj
// / − deşarj) ve TEL_bmsCurrentCentiA'ya parse ediliyor. Bu iki yardımcı
// hasWarningCondition/hasCriticalCondition'a BAĞLIDIR (yalnız
// TEL_bmsDataValid iken değerlendirilir). Eşikler CONFIG — bkz.
// SystemConfig.h (şarj 11/13 A saha kalibrasyonu, ekip onayı bekliyor).
inline bool isCurrentCritical(int32_t bmsCurrentCentiA) {
    return bmsCurrentCentiA >= BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A ||
           bmsCurrentCentiA <= -BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A;
}

inline bool isCurrentWarning(int32_t bmsCurrentCentiA) {
    return bmsCurrentCentiA >= BMS_WARN_MAX_CHARGE_CURRENT_CENTI_A ||
           bmsCurrentCentiA <= -BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_A;
}

// Sıcaklık politikası: 55 °C ve üzeri UYARI, 70 °C ve üzeri KRİTİK (sistem
// kendini kapatır — FAULT). Semantik >= : eşik değerinin KENDİSİ tetikler.
// Kaynak sinyal DOĞRULANDI (0xE001 byte[6:7] → TEL_bmsTempHighestC) ve bu
// yardımcılar hasWarningCondition/hasCriticalCondition'a BAĞLIDIR (yalnız
// TEL_bmsDataValid iken değerlendirilir; E001 freshness'ını G12 garanti eder).
inline bool isTempCritical(int8_t bmsTempHighestC) {
    return bmsTempHighestC >= BMS_CRITICAL_MAX_TEMP_C;
}

inline bool isTempWarning(int8_t bmsTempHighestC) {
    return bmsTempHighestC >= BMS_WARN_MAX_TEMP_C;
}

#if RELAY_ROLES_ASSIGNED
// Flaşör (şartname 6.e.ii) durum kararı — SAF: mevcut flaşör durumu + taze
// telemetriden yeni istenen durumu döndürür. Histerezisli:
//   * sıcaklık >= 55 (BMS_WARN_MAX_TEMP_C)                → ON
//   * sıcaklık < 53 (55 − FLASHER_HYSTERESIS_C)           → OFF
//   * 53..54 bandı                                        → mevcut durum korunur
// BAYAT VERİ KURALI: bmsDataValid=false veya BMS timeout iken flaşöre
// DOKUNULMAZ (son durum korunur) — bayat veriyle ne yakma ne söndürme.
inline bool flasherDesiredState(bool currentOn, bool bmsDataValid,
                                bool bmsTimeoutActive, int8_t bmsTempHighestC) {
    if (!bmsDataValid || bmsTimeoutActive)
        return currentOn;
    if (isTempWarning(bmsTempHighestC))
        return true;
    if (bmsTempHighestC < BMS_WARN_MAX_TEMP_C - FLASHER_HYSTERESIS_C)
        return false;
    return currentOn;
}

// Soğutma fanı (şartname B3 7.a-b) durum kararı — flaşörün İKİZİ, SAF:
// mevcut fan durumu + taze telemetriden yeni istenen durumu döndürür.
// Histerezisli (FAN_ON_TEMP_C=40 / FAN_OFF_TEMP_C=35):
//   * sıcaklık >= FAN_ON_TEMP_C   → ON
//   * sıcaklık <= FAN_OFF_TEMP_C  → OFF
//   * arada (36..39)              → mevcut durum korunur
// BAYAT VERİ KURALI (EK B GÜVEN): bmsDataValid=false veya BMS timeout iken
// fana DOKUNULMAZ — karar yalnızca doğrulanmış TEL_bmsTempHighestC'den
// türetilir (bayat veriyle ne çalıştırma ne durdurma). Fan mantığı FAULT/
// E-STOP dahil her tick çalışır: fan kanalı bank maskesi DIŞINDA olduğundan
// güvenlik açması (allOff) fanı söndürmez (sıcak bataryanın soğutması kesilmez).
inline bool fanDesiredState(bool currentOn, bool bmsDataValid,
                            bool bmsTimeoutActive, int8_t bmsTempHighestC) {
    if (!bmsDataValid || bmsTimeoutActive)
        return currentOn;
    if (bmsTempHighestC >= FAN_ON_TEMP_C)
        return true;
    if (bmsTempHighestC <= FAN_OFF_TEMP_C)
        return false;
    return currentOn;
}

// Far (şartname B2 9.19.c) durum kararı — SAF: her geçerli ekran komutunda
// TOGGLE. BMS verisinden tamamen BAĞIMSIZ (yalnız mevcut duruma bakar); fan/
// flaşörden farklı olarak sıcaklık/telemetri okumaz. Far kanalı bank maskesi
// DIŞINDA olduğundan FAULT/E-STOP/READY girişindeki allOff/allOn far durumunu
// DEĞİŞTİRMEZ; boot'ta OFF (s_headlightOn=false).
inline bool headlightDesiredState(bool currentOn) {
    return !currentOn;
}
#endif  // RELAY_ROLES_ASSIGNED

inline bool hasWarningCondition(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return false;

    // Hücre voltaj WARN kontrolü (freshness E015-E020 mask'ından, değerler
    // 0xE001'den — bkz. TEL_cellVoltageDataValid tanımı VehicleData.h).
    // Yalnızca tüm 24 hücre verisi tazeyse. TEL_bmsCellVoltageMin/MaxDeciMv
    // deci-mV ölçeğindedir — eşikler de deci-mV (BMS_CELL_*_DECI_MV, bkz.
    // BmsAlgo.h). GÜVENLİK-EŞİĞİ DÜZELTMESİ (2026-07-13): önceden mV-ölçekli
    // makrolarla (BMS_CELL_*_MV) karşılaştırılıyordu — gerçekçi bir hücre
    // voltajıyla (deci-mV ~28000-40000) bu, overvolt dalının HER ZAMAN,
    // undervolt dalının ise NEREDEYSE HİÇ tetiklenmemesi anlamına geliyordu.
    if (VCU_data.TEL_cellVoltageDataValid) {
        // Min hücre WARN altında mı?
        if (VCU_data.TEL_bmsCellVoltageMinDeciMv < BMS_CELL_UNDERVOLT_WARN_DECI_MV) return true;
        // Max hücre WARN üstünde mı?
        if (VCU_data.TEL_bmsCellVoltageMaxDeciMv > BMS_CELL_OVERVOLT_WARN_DECI_MV) return true;
    }

    // Yalnızca DOĞRULANMIŞ sinyaller: pack voltajı (WARN bandı) + en yüksek
    // hücre sıcaklığı (≥55 °C UYARI) + akım (şarj ≥11 A / deşarj ≥9 A WARN).
    return VCU_data.TEL_bmsPackVoltageDeciV <=
               BMS_WARN_MIN_PACK_VOLTAGE_DECI_V ||
           VCU_data.TEL_bmsPackVoltageDeciV >=
               BMS_WARN_MAX_PACK_VOLTAGE_DECI_V ||
           isTempWarning(VCU_data.TEL_bmsTempHighestC) ||
           isCurrentWarning(VCU_data.TEL_bmsCurrentCentiA);
}

inline bool hasCriticalCondition(const TelemetryData& VCU_data,
                                 VcuState currentState) {
    // NOT: TEL_bmsSystemState hiçbir CAN ID'den parse EDİLMİYOR (üretimde hep
    // 0 kalır) — ==4 kontrolü kaynak bağlanana kadar ETKİSİZ. Bkz.
    // Documents/UKS_LoRa_Protocol.md "DOĞRULANACAK".
    if (VCU_data.TEL_motorErrorFlags != 0 || (VCU_data.TEL_bmsDataValid && VCU_data.TEL_bmsSystemState == 4))
        return true;

    if (VCU_data.TEL_motorTimeoutActive && currentState != VcuState::IDLE)
        return true;

    // Post-reception BMS freshness kaybı — motor timeout ile aynı politika:
    // IDLE'da tolere edilir, READY/DRIVE'da kritik (HV bus canlıyken bayat
    // BMS verisiyle sürüşe devam edilmez).
    if (VCU_data.TEL_bmsTimeoutActive && currentState != VcuState::IDLE)
        return true;

    if (!VCU_data.TEL_bmsDataValid)
        return false;

    // Yalnızca DOĞRULANMIŞ sinyaller: pack voltajı (CRITICAL bandı — 24S
    // LiFePO4 spec) + en yüksek hücre sıcaklığı (≥70 °C FAULT) + akım
    // (şarj ≥13 A / deşarj ≥15 A FAULT). Kritik koşullar
    // isReadyEntryPermitted üzerinden READY girişini de bloklar.
    // Hücre voltaj CRITICAL kontrolü — deci-mV alan, deci-mV eşik (bkz.
    // hasWarningCondition yorumu ve BmsAlgo.h "deci-mV ESDEĞERLERİ").
    if (VCU_data.TEL_cellVoltageDataValid) {
        if (VCU_data.TEL_bmsCellVoltageMinDeciMv < BMS_CELL_UNDERVOLT_CRIT_DECI_MV) return true;
        if (VCU_data.TEL_bmsCellVoltageMaxDeciMv > BMS_CELL_OVERVOLT_CRIT_DECI_MV) return true;
    }

    return VCU_data.TEL_bmsPackVoltageDeciV <=
               BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V ||
           VCU_data.TEL_bmsPackVoltageDeciV >=
               BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V ||
           isTempCritical(VCU_data.TEL_bmsTempHighestC) ||
           isCurrentCritical(VCU_data.TEL_bmsCurrentCentiA);
}

inline bool isResetInterlockSatisfied(const TelemetryData& VCU_data,
                                      VcuState currentState) {
    // NOT: TEL_bmsSystemState==4 kontrolü kaynak bağlanana kadar etkisiz
    // (alan üretimde parse edilmiyor, hep 0 — bkz. hasCriticalCondition notu).
    if (VCU_data.TEL_motorErrorFlags != 0 || (VCU_data.TEL_bmsDataValid && VCU_data.TEL_bmsSystemState == 4))
        return false;

    if (hasCriticalCondition(VCU_data, currentState))
        return false;

    return true;
}

// IDLE → READY giriş interlock'u. READY'ye geçiş 10 kontaktörü kapatıp HV
// bus'ı enerjilendirdiğinden, batarya hakkında DOĞRULANMIŞ ve TAZE veri
// olmadan bu geçişe izin verilmez. Saf/donanımsız: yalnızca argümana bakar,
// hiçbir global okumaz. READY girişi her zaman IDLE'dan yapıldığından
// kritik/uyarı kontrolleri IDLE bağlamında değerlendirilir (IDLE'da motor/BMS
// timeout'u kritik sayılmaz; bunun yerine burada TEL_bmsDataValid şartı aranır).
inline bool isReadyEntryPermitted(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return false;

#if RELAY_ROLES_ASSIGNED
    // Şartname 8.2.a.iii: şarj modunda (charger CAN akışı taze) S1 kapalı /
    // S2 açıktır — charger aktifken READY (sürüş bankını kapatma) YASAK.
    // TEL_chargerActive, CAN_chargerValid'den türetilir (freshness dahil);
    // charger bağlı değilken/bayatken false olur ve READY serbest kalır.
    if (VCU_data.TEL_chargerActive)
        return false;
#endif

    if (hasCriticalCondition(VCU_data, VcuState::IDLE))
        return false;

    if (hasWarningCondition(VCU_data))
        return false;

#if MOTOR_DRIVER_PRESENT
    // Motor sürücüsü araçta: READY interlock'u taze motor verisi de arar.
    // Bayrak 0 iken (sürücü henüz yok) motor timeout'u READY girişini bloklamaz.
    if (!VCU_data.TEL_motorDataValid)
        return false;
#endif

    return true;
}

// ---------------------------------------------------------------------------
// Stateful API
// ---------------------------------------------------------------------------
// init() aktüatör katmanını REFERANS ile enjekte eder (M2). VcuLogic somut
// RelayManager'a bağlı değildir; üretimde main.cpp bir adapter, testler bir
// mock geçirir. Referans init() süresince ve sonrasında geçerli kalmalıdır
// (üretimde singleton adapter, testte statik mock — ikisi de kalıcı ömürlü).
void init(IRelayActuator& relays);
void run();
void postEvent(VcuEvent event);
VcuState getState();
void setTelemetryData(const TelemetryData& TEL_data);

#if RELAY_ROLES_ASSIGNED
// Far toggle isteği (şartname B2 9.19.c). Ekran HMI_CMD_HEADLIGHT_TOGGLE
// komutunu gönderince main.cpp bunu çağırır (HMI task bağlamı). E-STOP/FAULT
// bypass deseniyle aynı: yalnız atomic "toggle beklemede" bayrağını set eder;
// gerçek TOGGLE + röle sürüşü run() içinde (VCU task, tek röle yazarı) yapılır.
// Böylece far durumu FAULT/E-STOP dahil her durumda korunur (bank dışı kanal).
void toggleHeadlight();
#endif

// Torque komut yolu (motor sürücüsü entegrasyon iskeleti). E-STOP/FAULT
// güvenli kapanış sırasında VcuLogic torque(0) ister; bu isteği donanıma
// (CanManager::sendTorqueCommand) yönlendiren sink'i main.cpp bağlar.
// VcuLogic, CanManager'a doğrudan bağımlı olmasın diye (native testler
// CanManager'ı linklemez) function-pointer hook kullanılır. Sink bağlı
// değilse istek sessizce yok sayılır. Native testler buraya bir spy takıp
// çağrı sırasını doğrular.
using TorqueSink = void (*)(uint16_t torque);
void setTorqueSink(TorqueSink sink);

#ifdef NATIVE_BUILD
// Yalnız native test build'inde aktif (RelayManager::resetForTest ile aynı
// NATIVE_BUILD deseni). Tüm modül-içi state'i (durum, timer, son telemetri,
// queue, enjekte edilen aktüatör pointer'ı) sıfırlar; setUp()/prime içinde
// çağrılarak testler arası izolasyon sağlanır. Üretim build'inde tanımlı
// değildir.
void resetForTest();
#endif

}  // namespace VcuLogic
