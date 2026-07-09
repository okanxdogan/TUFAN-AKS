#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// G11 — LoRa UART init retry durum mantığı (SAF, native test edilebilir).
// ---------------------------------------------------------------------------
// SORUN: uart_driver_install bir kez başarısız olursa, sonraki turlarda
// ESP_ERR_INVALID_STATE dönüp döngü SONSUZA dek başarısız kalıyordu; task
// begin()'de takılıp telemetri sessizce ölü doğuyordu (reboot da yok).
//
// ÇÖZÜM: (a) retry'de driver zaten kuruluysa önce sil-yeniden kur (yandaki
// donanım adımı, çağırana ait — saf değil), (b) N denemeden sonra vazgeçip
// "telemetri devre dışı" moduna geç (araç durmaz). Bu fonksiyon (b)'nin SAF
// karar durumudur: kaç deneme başarısız olduğuna bakıp devam mı vazgeç mi der.
enum class UartInitDecision : uint8_t {
    RETRY,             // tekrar dene (çağıran: kuruluysa sil + yeniden kur)
    GIVE_UP_DISABLED,  // N denemede olmadı → telemetrisiz moda geç, reboot YOK
};

// failedAttempts: şimdiye kadarki BAŞARISIZ deneme sayısı (ilk başarısızlıkta 1).
// maxAttempts: bu sayıya ULAŞILINCA (>=) vazgeçilir. maxAttempts <= 0 ise
// hemen vazgeçilir (savunma; üretimde LORA_UART_MAX_INIT_ATTEMPTS >= 1).
inline UartInitDecision uart_init_retry_decision(int failedAttempts,
                                                 int maxAttempts) {
    if (failedAttempts >= maxAttempts)
        return UartInitDecision::GIVE_UP_DISABLED;
    return UartInitDecision::RETRY;
}
