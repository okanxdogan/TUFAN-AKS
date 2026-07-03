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

// Boot-grace sarmalayıcısı (9.2.e / 9.4.b.vi): link_check_timeout, hiç
// heartbeat gelmemişken (last_hb_ms==0) hep false döner — araç açıldığında
// UKS hiç yayında değilse AKS sonsuza dek "link UP" varsayar ve o dönemin
// verisi hiç buffer'a girmeden kaybolur. Bu sarmalayıcı, boot'tan
// boot_grace_ms geçtiği halde hâlâ hiç heartbeat gelmediyse link'i DOWN
// sayar; ilk heartbeat gelir gelmez mevcut link_check_timeout davranışına
// döner. Mevcut link_check_timeout DEĞİŞMEDİ — yalnız bu yeni fonksiyon
// eklendi.
static inline bool link_check_timeout_with_boot_grace(uint64_t now_ms,
                                                       uint64_t last_hb_ms,
                                                       uint32_t timeout_ms,
                                                       uint64_t boot_ms,
                                                       uint32_t boot_grace_ms) {
    if (last_hb_ms == 0u) {
        return (now_ms - boot_ms) > (uint64_t)boot_grace_ms;
    }
    return link_check_timeout(now_ms, last_hb_ms, timeout_ms);
}

// Implementasyon src/main.cpp'de — tüm task'lardan güvenle çağrılabilir.
#ifdef __cplusplus
extern "C" {
#endif
bool LoRa_IsLinkDown(void);
#ifdef __cplusplus
}
#endif
