#pragma once
#include "Telemetry.h"

// Dairesel FIFO tampon — bağlantı kesikliğinde telemetri paketlerini saklar.
// Statik bellek, dinamik tahsis YOK. Thread-safe DEĞİL; çağıran senkronize eder.

#define OB_CAPACITY 300  // 60 sn × 5 Hz

void ob_reset();
bool ob_push(const TelemetryData& data);  // dolu ise en eskiyi düşür, true döner
bool ob_pop(TelemetryData& out);           // boşsa false döner
int  ob_count();
bool ob_is_empty();
