#pragma once
#include <cstdint>

#include "BmsAlgo.h"      // BMS_CELL_UNDERVOLT/OVERVOLT_WARN/CRIT_DECI_MV
#include "SystemConfig.h" // DERATING_*, BMS_WARN_/CRITICAL_* esikleri
#include "VehicleData.h"  // TelemetryData (M3)

// DeratingPolicy — AÇIK İŞ B12 iskeleti: WARN bandındaki sinyallerden 0..100
// bir "tork-izin yüzdesi" hesaplar. Saf, donanım/FreeRTOS bağımsız — native
// test edilebilir (VcuLogic.cpp'yi linklemeden çağrılabilir, VcuLogic.h'nin
// saf predicate'leriyle aynı desen: hasWarningCondition/hasCriticalCondition).
//
// KAPSAM: bu dosya yalnızca bir SAYI üretir. Motor sürücüsü elimizde YOK —
// bu yüzden burada üretilen yüzde HİÇBİR twai/CanManager/TorqueRequestQueue
// çağrısına BAĞLANMAZ (bkz. VcuLogic.cpp run() — yalnızca loglanır).
// Motor sürücüsü tork komut yolu (setTorqueSink/G2) gerçek frame üretmeye
// başladığında, bu yüzdenin torku nasıl sınırlayacağı (çarpan mı, üst sınır
// mı) AYRI bir tasarım kararı olacak — bkz. VcuLogic.cpp'deki entegrasyon
// noktası yorumu.
//
// SİNYAL SETİ: hasWarningCondition (VcuLogic.h) ile AYNI doğrulanmış
// sinyaller — pack voltajı, akım, en yüksek hücre sıcaklığı, (varsa) 24
// hücre min/max voltajı. EK B GÜVEN KURALI burada da geçerlidir: yalnızca
// DOĞRULANMIŞ sinyaller kullanılır.
//
// KADEME TANIMI ("yaklaşma" ne demek?): bir sinyal WARN eşiğini geçtiğinde
// DERATING_TORQUE_PERCENT_WARNING (50) uygulanır. Sinyal, WARN eşiğinden
// CRITICAL eşiğine giden ARALIĞIN DERATING_APPROACHING_CRITICAL_FRACTION_
// PERCENT (90) kadarını tükettiyse DERATING_TORQUE_PERCENT_APPROACHING_
// CRITICAL (20) uygulanır. Bilinçli tasarım kararı: "eşiğin ham değerinin
// %90'ı" YERİNE "WARN->CRITICAL aralığının %90'ı" kullanılır — çünkü
// buradaki eşikler sıfırdan uzak mutlak fiziksel niceliklerdir (ör. pack
// aşırı gerilim WARN=852/CRITICAL=876 deciV); ham değerin %90'ı (788.4)
// WARN eşiğinin (852) ALTINA düşer ve "yaklaşma" WARN'dan ÖNCE tetiklenir —
// fiziksel olarak tersine döner. Aralık-yüzdesi tüm sinyal tiplerinde
// (MIN/MAX yönlü) tutarlı ve monoton çalışır.
//
// HİSTEREZİS: bilinçle EKLENMEDİ (SysStateDerive.h'deki aynı kararla aynı
// gerekçe): bu değer ŞU AN yalnızca bir log satırını besliyor, hiçbir
// kontaktör/tork kararını etkilemiyor — bant sınırında nadir bir titreme
// yalnızca log gürültüsüdür, güvenlik sonucu doğurmaz. Gerçek bir tork
// komutuna bağlanacağı gün (yukarıdaki KAPSAM notu) bu karar gözden
// geçirilmeli — sürekli 50<->20 arası çırpınan bir tork limiti sürüş
// hissini bozabilir; o zaman histerezis eklenmesi ÖNERİLİR.
namespace DeratingPolicy {

namespace detail {

// Üst sınırlı bir sinyal (büyüdükçe kötüleşir: sıcaklık, şarj akımı, pack
// aşırı gerilim, hücre aşırı gerilim) için kademe. warnThreshold <
// critThreshold varsayılır.
inline uint8_t tierFromUpperBound(int32_t value, int32_t warnThreshold,
                                  int32_t critThreshold) {
    if (value < warnThreshold)
        return DERATING_TORQUE_PERCENT_NOMINAL;
    const int32_t span = critThreshold - warnThreshold;
    const int32_t approachAt =
        warnThreshold + (span * DERATING_APPROACHING_CRITICAL_FRACTION_PERCENT) / 100;
    if (value >= approachAt)
        return DERATING_TORQUE_PERCENT_APPROACHING_CRITICAL;
    return DERATING_TORQUE_PERCENT_WARNING;
}

// Alt sınırlı bir sinyal (küçüldükçe kötüleşir: pack düşük gerilim, hücre
// düşük gerilim) için kademe. warnThreshold > critThreshold varsayılır.
inline uint8_t tierFromLowerBound(int32_t value, int32_t warnThreshold,
                                  int32_t critThreshold) {
    if (value > warnThreshold)
        return DERATING_TORQUE_PERCENT_NOMINAL;
    const int32_t span = warnThreshold - critThreshold;
    const int32_t approachAt =
        warnThreshold - (span * DERATING_APPROACHING_CRITICAL_FRACTION_PERCENT) / 100;
    if (value <= approachAt)
        return DERATING_TORQUE_PERCENT_APPROACHING_CRITICAL;
    return DERATING_TORQUE_PERCENT_WARNING;
}

// Akım iki yönlü (+ şarj / - deşarj), her yönün kendi WARN/CRITICAL çifti
// var — büyüklüğe (magnitude) çevirip tierFromUpperBound'a devreder.
inline uint8_t currentTier(int32_t bmsCurrentCentiA) {
    if (bmsCurrentCentiA >= 0) {
        return tierFromUpperBound(bmsCurrentCentiA,
                                  BMS_WARN_MAX_CHARGE_CURRENT_CENTI_A,
                                  BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A);
    }
    return tierFromUpperBound(-bmsCurrentCentiA,
                              BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_A,
                              BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A);
}

}  // namespace detail

// Ana giriş noktası: TelemetryData'dan 0..100 tork-izin yüzdesi. Tüm
// değerlendirilen sinyaller arasından EN KISITLAYICI (en düşük yüzde)
// olan uygulanır (worst-case kazanır — hasWarningCondition'daki OR
// mantığının derating tarafındaki MIN karşılığı).
//
// bmsValid=false -> NOMINAL (100, nötr): karar verilecek DOĞRULANMIŞ veri
// yok; bu durumda zaten hasCriticalCondition/timeout yolları ayrı ele alır
// (bkz. VcuLogic.cpp run() — bu fonksiyon yalnız hasWarningCondition==true
// dalında çağrılır, o dal zaten TEL_bmsDataValid==true gerektirir; bu
// nötr-dönüş savunma amaçlıdır, fonksiyon başka bir bağlamdan da
// çağrılabilir diye).
inline uint8_t computeTorqueAllowPercent(const TelemetryData& d) {
    if (!d.TEL_bmsDataValid)
        return DERATING_TORQUE_PERCENT_NOMINAL;

    uint8_t percent = DERATING_TORQUE_PERCENT_NOMINAL;

    const uint8_t tempTier =
        detail::tierFromUpperBound(d.TEL_bmsTempHighestC, BMS_WARN_MAX_TEMP_C,
                                   BMS_CRITICAL_MAX_TEMP_C);
    if (tempTier < percent) percent = tempTier;

    const uint8_t currentTier = detail::currentTier(d.TEL_bmsCurrentCentiA);
    if (currentTier < percent) percent = currentTier;

    const uint8_t packUnderTier = detail::tierFromLowerBound(
        d.TEL_bmsPackVoltageDeciV, BMS_WARN_MIN_PACK_VOLTAGE_DECI_V,
        BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V);
    if (packUnderTier < percent) percent = packUnderTier;

    const uint8_t packOverTier = detail::tierFromUpperBound(
        d.TEL_bmsPackVoltageDeciV, BMS_WARN_MAX_PACK_VOLTAGE_DECI_V,
        BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V);
    if (packOverTier < percent) percent = packOverTier;

    // Hücre voltajı — yalnızca tüm 24 hücre verisi tazeyse (hasWarningCondition
    // ile aynı kapı, bkz. VcuLogic.h). TEL_bmsCellVoltageMin/MaxDeciMv deci-mV
    // ölçeğindedir; eşikler de deci-mV (BMS_CELL_*_DECI_MV, bkz. BmsAlgo.h
    // "deci-mV ESDEĞERLERİ" — GÜVENLİK-EŞİĞİ DÜZELTMESİ 2026-07-13, önceden
    // burada mV-ölçekli makrolarla karşılaştırılıyordu; bkz. Threshold_
    // Ownership.md için önceki/sonraki davranış özeti).
    if (d.TEL_cellVoltageDataValid) {
        const uint8_t cellUnderTier = detail::tierFromLowerBound(
            d.TEL_bmsCellVoltageMinDeciMv, BMS_CELL_UNDERVOLT_WARN_DECI_MV,
            BMS_CELL_UNDERVOLT_CRIT_DECI_MV);
        if (cellUnderTier < percent) percent = cellUnderTier;

        const uint8_t cellOverTier = detail::tierFromUpperBound(
            d.TEL_bmsCellVoltageMaxDeciMv, BMS_CELL_OVERVOLT_WARN_DECI_MV,
            BMS_CELL_OVERVOLT_CRIT_DECI_MV);
        if (cellOverTier < percent) percent = cellOverTier;
    }

    return percent;
}

}  // namespace DeratingPolicy
