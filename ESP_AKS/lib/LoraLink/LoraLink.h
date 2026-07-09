#pragma once
#include <cstddef>
#include <cstdint>

// LoraLink — E22 boot-config + AUX el sıkışmasının ince donanım katmanı
// (M1 refactor). Tüm UART/GPIO/zaman/watchdog erişimi enjekte edilen soyut
// ILoraHal arayüzünün arkasındadır; böylece bu sınıf ESP-IDF'e derleme-zamanı
// bağımlı DEĞİLDİR ve native derlenebilir (gerçek donanım main.cpp'deki
// EspLoraHal ile sağlanır). E22 komut byte'larını E22Config saf yardımcıları
// üretir/ayrıştırır.
//
// DAVRANIŞ KORUYAN: sıralama, timeout'lar ve loglar eski vTask_LoRa_UKS boot
// bloğuyla (main.cpp) birebir aynıdır.

// Donanım soyutlaması — main.cpp'deki EspLoraHal gerçek ESP-IDF çağrılarına,
// testlerde ise sahte/no-op implementasyona bağlanır. Pin numaraları ve AUX
// hazır-seviyesi HAL'in içinde bilinir (bu sınıfa sızmaz).
class ILoraHal {
   public:
    virtual ~ILoraHal() = default;

    // M0/M1 mode pinlerini verilen seviyelere sürer (config/normal geçişi).
    virtual void setModePins(int m0Level, int m1Level) = 0;

    // AUX pini "hazır" (LORA_AUX_READY_LEVEL) mı?
    virtual bool isAuxReady() = 0;
    // AUX pininin ham seviyesi (yalnızca tanı logu için).
    virtual int auxRawLevel() = 0;

    virtual void uartFlush() = 0;
    virtual int uartWrite(const uint8_t* data, size_t len) = 0;
    virtual int uartRead(uint8_t* buf, size_t len, uint32_t timeoutMs) = 0;

    virtual void delayMs(uint32_t ms) = 0;
    virtual uint64_t nowMs() = 0;  // monotonik ms (timeout döngüleri)
    virtual void feedWatchdog() = 0;

    // E22REG hex dump — format string (contract-drift bekçisi) main.cpp'de
    // KALIR; LoraLink yalnızca bu callback'i çağırır.
    virtual void hexDumpE22(const char* label, const uint8_t* buf, int len) = 0;
};

class LoraLink {
   public:
    explicit LoraLink(ILoraHal& hal) : m_hal(hal) {}

    // E22 boot konfigürasyonu: config moduna gir → oku → gerekirse yaz →
    // doğrula → normal moda dön. Donanım el sıkışması + loglar dahil.
    // (GPIO/UART sürücü kurulumu orchestration'da yapılır; bu fonksiyon
    // kurulu bir HAL varsayar.)
    void configureE22();

    // AUX hazır mı (TX kapısı — orchestration replay/canlı göndermeden önce
    // sorar; HAL'e ince geçiş).
    bool isAuxReady() { return m_hal.isAuxReady(); }

   private:
    // AUX HIGH gelene kadar (10 ms poll) bekler; timeoutMs içinde gelmezse
    // false. Watchdog beslenmez (kısa bekleme; çağıran uzun döngüde besler).
    bool waitAuxReady(uint32_t timeoutMs);

    ILoraHal& m_hal;
};
