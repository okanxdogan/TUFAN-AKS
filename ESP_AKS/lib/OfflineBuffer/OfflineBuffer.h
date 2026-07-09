#pragma once
#include <cstdint>
#include "VehicleData.h"  // TelemetryData (M3)

// Dairesel FIFO tampon — bağlantı kesikliğinde telemetri paketlerini saklar.
// Statik bellek, dinamik tahsis YOK. Thread-safe DEĞİL; çağıran senkronize eder.

// 9.2.e / 9.4.b.vi: 60 sn kesinti × OFFLINE_SAMPLE_PERIOD_MS (1 Hz) = 60
// paket + %25 marj = 75. Eskiden 300 (60 sn × 5 Hz) idi; örnekleme
// seyreltilince (S2) kapasite de aynı oranda küçüldü, replay yükü 5'e bölündü.
#define OB_CAPACITY 75

void ob_reset();
bool ob_push(const TelemetryData& data);  // dolu ise en eskiyi düşür, true döner
bool ob_pop(TelemetryData& out);           // boşsa false döner
bool ob_peek(TelemetryData& out);          // en eskiyi OKUR, düşürmez; boşsa false
bool ob_drop_front();                      // en eskiyi düşürür; boşsa false döner
int  ob_count();
bool ob_is_empty();

// Kesinti örneklemesini seyreltir (S2): has_sample false ise (bu kesintide
// henüz örnek alınmadı) hemen örnekle; aksi halde en az period_ms geçtiyse
// true döner. has_sample'ı ayrı bir bool almak bilinçli bir tercih:
// last_sample_ms'i "henüz yok" sentineli olarak 0 kullanmak (link_check_timeout
// deseni), zaman damgasının kendisi 0 olabildiği durumlarda (ör. kesinti
// tam uptime=0 anında başlarsa) yanlış pozitif üretir.
static inline bool offline_should_sample(uint64_t now_ms,
                                          uint64_t last_sample_ms,
                                          bool has_sample,
                                          uint32_t period_ms) {
    if (!has_sample) return true;
    return (now_ms - last_sample_ms) >= (uint64_t)period_ms;
}
