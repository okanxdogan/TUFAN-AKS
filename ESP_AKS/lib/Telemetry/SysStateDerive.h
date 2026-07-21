#pragma once
#include <cstdint>

#include "SystemConfig.h"  // SYSSTATE_DERIVE_FROM_CURRENT, SYSSTATE_CURRENT_IDLE_BAND_CENTI_A
#include "VehicleData.h"   // TelemetryData

// SysStateDerive — HİPOTEZ tabanlı, DOĞRULANMIŞ akım sinyalinden (0xE000
// byte[0:1], TEL_bmsCurrentCentiA) türetilmiş bir `sysState` tahmini.
//
// NEDEN: UKS `sysState` alanı (TEL 12. alan) hiçbir CAN ID'den gerçek parse
// almıyor (bkz. Documents/UKS_LoRa_Protocol.md "DOĞRULANACAK") —
// TelemetrySanitize::sanitizeSystemState(0) bunu FAULT(4) yapar, UKS
// ekranında BMS her zaman FAULT görünür. 0xE003 byte[0:1]'in gerçek sysState
// olabileceğine dair bir HİPOTEZ var (bkz. Documents/CAN_Message_Table.md
// "0x0000E003") ama bağımsız teyidi yok (iki PCAN oturumu da boşta geçti).
// Bu dosya, o hipotez teyit edilene kadar kullanılabilecek, DAHA DÜŞÜK
// iddialı, akım tabanlı bir ara-çözüm sunar — yalnızca Discharge/IDLE/Charge
// ayrımı yapar (FAULT girdisi YOK, aşağıya bkz.).
//
// EK B GÜVEN KURALI: bu türetilmiş değer YALNIZCA UKS telemetri gösterimi
// içindir — VCU karar mantığına (FAULT/kontaktör) BAĞLANMAZ. Çağıran taraf
// (main.cpp LoRa_txSend) bunu yalnızca LoRa TX paketleme yolunda, VcuLogic'in
// okuduğu paylaşılan TelemetryData kopyasına DOKUNMADAN uygular.
//
// FAULT(4) NEDEN YOK: akımdan "hata" çıkarılamaz (akım normal aralıkta olsa
// bile BMS başka bir nedenle FAULT'ta olabilir, ya da tam tersi) — bu,
// hipotezin kapsamının ÖTESİNDE bir iddia olurdu. E032/E033 (alarm/uyarı
// bitfield adayı, bkz. CAN_Message_Table.md) doğrulanırsa FAULT girdisi
// BURAYA (aşağıdaki deriveFromCurrentImpl çağrısından önce/sonra bir kontrol
// olarak) bağlanabilir — bu, o doğrulama tamamlanana kadar bilinçli olarak
// AÇIK bırakılmış bir genişletme noktasıdır.
//
// HİSTEREZİS: bilinçli olarak EKLENMEDİ. Gerekçe: (1) bu değer yalnızca UKS
// operatör ekranındaki bir GÖSTERİM alanını besler, hiçbir kontaktör/FAULT
// kararını etkilemez — bant sınırında nadir bir 2↔3 titremesi güvenlik
// sonucu doğurmaz, yalnızca kozmetik bir görüntü kararsızlığıdır; (2)
// histerezis EKLEMEK bu saf/stateless fonksiyonu STATEFUL yapardı (önceki
// durumu bir yerde saklamak gerekir), bu da bir HİPOTEZ için gereğinden
// fazla karmaşıklık/test yüzeyi eklerdi; (3) TX periyodu zaten 2 Hz
// (LORA_TX_PERIOD_MS=500) — insan operatörün fark edeceği bir çırpınma
// oranı değil. Gerçek parse (E003 teyit edilirse) veya ekip histerezis
// isterse bu karar gözden geçirilebilir.
namespace SysStateDerive {

// UKS sysState sözleşmesi: 1=Discharge, 2=IDLE, 3=Charge, 4=FAULT (burada
// üretilmez). idleBandCentiA parametreli (test edilebilir) çekirdek —
// üretim kodu aşağıdaki deriveFromCurrent() sarmalayıcısını kullanır
// (rpmToSpeedKmhX10Impl/rpmToSpeedKmhX10 ile aynı desen, bkz. Telemetry.h).
inline uint8_t deriveFromCurrentImpl(int32_t bmsCurrentCentiA,
                                     int32_t idleBandCentiA) {
    if (bmsCurrentCentiA > idleBandCentiA) return 3;   // Charge
    if (bmsCurrentCentiA < -idleBandCentiA) return 1;  // Discharge
    return 2;                                          // IDLE (|akım| <= bant)
}

inline uint8_t deriveFromCurrent(int32_t bmsCurrentCentiA) {
    return deriveFromCurrentImpl(bmsCurrentCentiA,
                                 SYSSTATE_CURRENT_IDLE_BAND_CENTI_A);
}

// Tek çağrı noktası: main.cpp LoRa_txSend içinde, sanitizeForUplink'ten
// ÖNCE çağrılır. SYSSTATE_DERIVE_FROM_CURRENT==0 iken (varsayılan) hiçbir
// şey YAPMAZ — davranış birebir korunur. ==1 iken de yalnızca
// TEL_bmsSystemState HALA 0 ise (gerçek parse henüz eklenmemişse) uygular;
// gerçek bir parse eklenip alan doldurulmuşsa (!=0) ONU EZMEZ.
inline void applyIfEnabled(TelemetryData& d) {
#if SYSSTATE_DERIVE_FROM_CURRENT
    if (d.TEL_bmsSystemState == 0) {
        d.TEL_bmsSystemState = deriveFromCurrent(d.TEL_bmsCurrentCentiA);
    }
#else
    (void)d;
#endif
}

}  // namespace SysStateDerive
