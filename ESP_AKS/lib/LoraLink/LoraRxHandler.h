#pragma once
#include <cstdint>
#include "SystemConfig.h"

// Pure function — ESP-IDF bağımsız, native test edilebilir.
// 9.2.a: RF hattı tek yönlü telemetri + heartbeat'tir, komut kanalı yok.
// UKS_HEARTBEAT_BYTE dışındaki her byte "bilinmeyen" sayılır.
enum class LoraRxByteKind { HEARTBEAT, UNKNOWN };

static inline LoraRxByteKind lora_classify_rx_byte(uint8_t rx_byte) {
    return (rx_byte == UKS_HEARTBEAT_BYTE) ? LoraRxByteKind::HEARTBEAT
                                            : LoraRxByteKind::UNKNOWN;
}

// Bilinmeyen byte kaydı — sayaci her zaman artirir; son uyaridan bu yana en
// az warn_interval_ms geçtiyse true döner ve *last_warn_ms günceller (RF
// gürültü teşhisi, throttled WARN — log spam önleme).
static inline bool lora_note_unknown_byte(uint64_t now_ms, uint32_t *count,
                                           uint64_t *last_warn_ms,
                                           uint32_t warn_interval_ms) {
    (*count)++;
    if (now_ms - *last_warn_ms >= (uint64_t)warn_interval_ms) {
        *last_warn_ms = now_ms;
        return true;
    }
    return false;
}
