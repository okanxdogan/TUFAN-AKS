#pragma once
#include <cstdint>
#include <stdbool.h>

// Pure function — ESP-IDF bağımsız, native test edilebilir.
// last_hb_ms == 0 ise (hiç heartbeat gelmedi) false döner.
// (now_ms - last_hb_ms) > timeout_ms ise true (link kesik).
static inline bool link_check_timeout(uint64_t now_ms,
                                      uint64_t last_hb_ms,
                                      uint32_t timeout_ms) {
    if (last_hb_ms == 0u) return false;
    return (now_ms - last_hb_ms) > (uint64_t)timeout_ms;
}

// Implementasyon src/main.cpp'de — tüm task'lardan güvenle çağrılabilir.
#ifdef __cplusplus
extern "C" {
#endif
bool LoRa_IsLinkDown(void);
#ifdef __cplusplus
}
#endif
