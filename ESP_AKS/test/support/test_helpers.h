#pragma once
#include "Telemetry.h"

namespace test_helpers {

// "Tüm değerler güvenli aralıkta, hiçbir uyarı/kritik koşul tetiklenmez"
// referans bir TelemetryData üretir. Tek bir alanı edit edip threshold
// kenarlarını izole etmek kolaylaşsın diye tüm alanlar nominal seçilir.
//
// Notlar:
// - SOC, sıcaklık ve voltaj VCU eşiklerinin tam orta bandında (25°C, 8.0V).
// - Akım sıfır → ne charge ne discharge warning/critical.
// - Tüm error flag'ler 0; data valid; timeout aktif değil.
inline TelemetryData makeTelemetryDataValid() {
    TelemetryData d{};
    d.TEL_motorRpm = 0;
    d.TEL_motorTorqueFeedback = 0;
    d.TEL_motorErrorFlags = 0;
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;

    d.TEL_bmsCellVoltageMaxDeciMv = 40000;   // 4000.0 mV
    d.TEL_bmsCellVoltageMinDeciMv = 39500;   // 3950.0 mV
    d.TEL_bmsTempHighestC = 25;
    d.TEL_bmsTempLowestC  = 23;
    d.TEL_bmsSystemState  = 2;               // IDLE — güvenli başlangıç
    d.TEL_bmsPackVoltageDeciV = 800;         // 80.0 V — bandın ortası
    d.TEL_bmsCurrentCentiA = 0;
    d.TEL_bmsSocHundredths = 8000;           // %80.00
    d.TEL_bmsDataValid = true;
    return d;
}

}  // namespace test_helpers
