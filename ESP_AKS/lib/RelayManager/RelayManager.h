#pragma once
#include <atomic>
#include <cstdint>
#include "driver/spi_master.h"

#define MCP23S17_IODIRA 0x00
#define MCP23S17_IODIRB 0x01
#define MCP23S17_OLATA 0x14
#define MCP23S17_OLATB 0x15
#define MCP23S17_ADDR 0x40       // SPI opcode, R/W=0 (write)
#define MCP23S17_ADDR_READ 0x41  // SPI opcode, R/W=1 (read) — G3 geri-okuma yolu

class RelayManager {
   public:
    static RelayManager& instance();

    bool begin();
    void setRelay(uint8_t channel, bool state);
    void allOn();   // Close all 10 positive contactors
    void allOff(bool silent = false);  // Open all — SAFETY

    // Read back current relay state for diagnostics
    bool getRelayState(uint8_t channel) const;

    // --- G3: MCP23S17 geri-okuma / çıkış doğrulama yolu ---
    // Tek bir register'ı SPI read (0x41) ile okur. Başarıda true ve out set.
    bool readRegister(uint8_t reg, uint8_t& out);

    // OLATA/OLATB/IODIRA/IODIRB'i geri okur, beklenen gölge-durumla (shadow:
    // ~s_relayState + tüm pinler output) karşılaştırır. Uyuşmazlıkta chip'i
    // yeniden init edip çıkışları re-assert eder ve actuator fault bayrağını
    // KALICI olarak set eder (VcuLogic her tick okur). Çıkışlar eşleşiyorsa
    // true döner. begin()/allOn()/allOff()/setRelay() sonrası HEMEN çağrılır.
    bool verifyOutputs();

    // VCU task tick'inden çağrılır; RELAY_VERIFY_PERIOD_MS'den seyrek
    // olmayacak şekilde verifyOutputs()'u tetikler (her tick değil).
    void verifyIfDue(uint32_t nowMs);

    // VcuLogic'in her tick okuyabileceği kalıcı atomic actuator-fault bayrağı
    // (R1: kuyruğa/olaya güvenilmez — düşen event tuzağı yok).
    bool hasActuatorFault() const;
    void clearActuatorFault();  // VcuLogic RESET yolunda çağrılır

#ifdef NATIVE_BUILD
    // Yalnız native test build'inde aktif. Singleton'ın iç state'ini
    // sıfırlar (relayState, init flag, SPI handle). Production build'inde
    // tanımlı değildir.
    void resetForTest();
#endif

   private:
    RelayManager() = default;
    void writeRegister(uint8_t reg, uint8_t value);
    // Güvenli init sırasını (OLAT HIGH → IODIR output) tekrar uygular ve
    // s_relayState'i re-assert eder. verifyOutputs() uyuşmazlıkta çağırır.
    void reinitAndReassert();

    uint16_t s_relayState = 0;
    spi_device_handle_t s_spiDev = nullptr;
    bool s_initialized = false;

    std::atomic<bool> s_actuatorFault{false};
    uint32_t s_lastVerifyMs = 0;
    bool s_verifyStarted = false;  // ilk verifyIfDue çağrısında periyodu başlat
};
