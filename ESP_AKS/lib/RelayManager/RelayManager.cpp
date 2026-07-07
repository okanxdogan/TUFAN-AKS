#include "RelayManager.h"
#include "SystemConfig.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static constexpr const char* TAG = "RelayManager";

RelayManager& RelayManager::instance() {
    static RelayManager inst;
    return inst;
}

bool RelayManager::begin() {
    spi_bus_config_t buscfg = {
        .mosi_io_num = RELAY_SPI_MOSI,
        .miso_io_num = RELAY_SPI_MISO,
        .sclk_io_num = RELAY_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 1000000,
        .spics_io_num = RELAY_SPI_CS,
        .queue_size = 1,
    };

    esp_err_t ret =
        spi_bus_initialize(RELAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = spi_bus_add_device(RELAY_SPI_HOST, &devcfg, &s_spiDev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return false;
    }

    // CRITICAL: Write OLAT registers BEFORE setting pins as output.
    // MCP23S17 defaults OLAT to 0x00 (LOW).  In our active-low hardware
    // LOW = relay ON, so we must set 0xFF (HIGH = relay OFF) first to
    // prevent all relays firing the instant IODIR switches to output.
    writeRegister(MCP23S17_OLATA, 0xFF);
    writeRegister(MCP23S17_OLATB, 0xFF);

    // Now safe to set all pins as output
    writeRegister(MCP23S17_IODIRA, 0x00);
    writeRegister(MCP23S17_IODIRB, 0x00);

    s_relayState = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "RelayManager initialized — all relays OFF");

    // Init yazımlarının chip'e gerçekten oturduğunu geri-okuma ile doğrula.
    verifyOutputs();
    return true;
}

void RelayManager::setRelay(uint8_t channel, bool state) {
    if (!s_initialized || channel >= RELAY_TOTAL_CHANNELS) {
        ESP_LOGW(TAG, "setRelay: invalid call (init=%d, ch=%d)", s_initialized,
                 channel);
        return;
    }

    if (state)
        s_relayState |= (1u << channel);
    else
        s_relayState &= ~(1u << channel);

    // Hardware is active-low: invert logical state before writing
    uint16_t hw = ~s_relayState;
    writeRegister(MCP23S17_OLATA, hw & 0xFF);
    writeRegister(MCP23S17_OLATB, (hw >> 8) & 0xFF);

    ESP_LOGD(TAG, "Relay %d → %s (state=0x%03X)", channel, state ? "ON" : "OFF",
             s_relayState);

    verifyOutputs();  // G3: yazma sonrası HEMEN geri-okuma doğrulaması
}

void RelayManager::allOn() {
    if (!s_initialized) {
        ESP_LOGW(TAG, "allOn called before begin()");
        return;
    }
    s_relayState = (1u << RELAY_TOTAL_CHANNELS) - 1;  // bits 0-9 set
    // Active-low: all relays ON = all used pins LOW
    uint16_t hw = ~s_relayState;
    writeRegister(MCP23S17_OLATA, hw & 0xFF);
    writeRegister(MCP23S17_OLATB, (hw >> 8) & 0xFF);
    ESP_LOGI(TAG, "All %d contactors closed", RELAY_TOTAL_CHANNELS);

    verifyOutputs();  // G3: kontaktörler gerçekten kapandı mı geri-oku
}

void RelayManager::allOff(bool silent) {
    s_relayState = 0;
    // Active-low: all relays OFF = all pins HIGH
    writeRegister(MCP23S17_OLATA, 0xFF);
    writeRegister(MCP23S17_OLATB, 0xFF);
    if (!silent) {
        ESP_LOGW(TAG, "All relays de-energized");
    }

    // G3: SAFETY — açma komutunun chip'e oturduğunu geri-okuma ile doğrula.
    // "sessiz re-assert doğrulama değildir" (VcuLogic); asıl doğrulama burada.
    verifyOutputs();
}

bool RelayManager::getRelayState(uint8_t channel) const {
    if (channel >= RELAY_TOTAL_CHANNELS)
        return false;
    return (s_relayState >> channel) & 0x01;
}

#ifdef NATIVE_BUILD
void RelayManager::resetForTest() {
    s_relayState = 0;
    s_initialized = false;
    s_spiDev = nullptr;
    s_actuatorFault.store(false, std::memory_order_relaxed);
    s_lastVerifyMs = 0;
    s_verifyStarted = false;
}
#endif

void RelayManager::writeRegister(uint8_t reg, uint8_t value) {
    if (s_spiDev == nullptr)
        return;

    uint8_t tx[3] = {MCP23S17_ADDR, reg, value};
    spi_transaction_t t = {};
    t.length = 24;
    t.tx_buffer = tx;

    esp_err_t ret = spi_device_transmit(s_spiDev, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write failed: reg=0x%02X val=0x%02X err=%s", reg,
                 value, esp_err_to_name(ret));
    }
}

// --- G3: geri-okuma / doğrulama yolu ---------------------------------------
bool RelayManager::readRegister(uint8_t reg, uint8_t& out) {
    if (s_spiDev == nullptr)
        return false;

    // MCP23S17 read: [0x41, reg, dummy] gönderilir, MISO 3. byte'ta register
    // değerini döndürür. Transaction deseni writeRegister ile aynı (24 bit,
    // aynı device => aynı hz/mode).
    uint8_t tx[3] = {MCP23S17_ADDR_READ, reg, 0x00};
    uint8_t rx[3] = {0, 0, 0};
    spi_transaction_t t = {};
    t.length = 24;
    t.rxlength = 24;
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t ret = spi_device_transmit(s_spiDev, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read failed: reg=0x%02X err=%s", reg,
                 esp_err_to_name(ret));
        return false;
    }
    out = rx[2];
    return true;
}

void RelayManager::reinitAndReassert() {
    // Güvenli sıra (bkz. begin()): önce OLAT'ı emniyetli HIGH'a al, sonra
    // pinleri output yap, en son gerçek gölge-durumu re-assert et.
    writeRegister(MCP23S17_OLATA, 0xFF);
    writeRegister(MCP23S17_OLATB, 0xFF);
    writeRegister(MCP23S17_IODIRA, 0x00);
    writeRegister(MCP23S17_IODIRB, 0x00);

    uint16_t hw = ~s_relayState;
    writeRegister(MCP23S17_OLATA, hw & 0xFF);
    writeRegister(MCP23S17_OLATB, (hw >> 8) & 0xFF);
}

bool RelayManager::verifyOutputs() {
    if (!s_initialized || s_spiDev == nullptr)
        return true;  // doğrulanacak bir şey yok / okunamaz

    const uint16_t hw = ~s_relayState;
    const uint8_t expOlatA = hw & 0xFF;
    const uint8_t expOlatB = (hw >> 8) & 0xFF;

    uint8_t olatA = 0, olatB = 0, iodirA = 0, iodirB = 0;
    bool readOk = readRegister(MCP23S17_OLATA, olatA) &&
                  readRegister(MCP23S17_OLATB, olatB) &&
                  readRegister(MCP23S17_IODIRA, iodirA) &&
                  readRegister(MCP23S17_IODIRB, iodirB);

    // IODIR default'u 0xFF (tüm pinler input) — brown-out/reset işareti.
    bool match = readOk && olatA == expOlatA && olatB == expOlatB &&
                 iodirA == 0x00 && iodirB == 0x00;
    if (match)
        return true;

    ESP_LOGE(TAG,
             "Actuator verify MISMATCH: OLAT=%02X/%02X (exp %02X/%02X) "
             "IODIR=%02X/%02X readOk=%d — re-init + re-assert",
             olatA, olatB, expOlatA, expOlatB, iodirA, iodirB, readOk);

    // (a) chip'i yeniden init et ve çıkışları re-assert et
    reinitAndReassert();
    // (b) VcuLogic'e kalıcı actuator fault bildir (atomic, her tick okunur)
    s_actuatorFault.store(true, std::memory_order_release);

    // Re-assert sonrası tekrar doğrula; hâlâ uyuşmuyorsa fault zaten latch'li,
    // yalnızca logla (VcuLogic fault'u sürdürür).
    uint8_t vA = 0, vB = 0, dA = 0, dB = 0;
    bool reOk = readRegister(MCP23S17_OLATA, vA) &&
                readRegister(MCP23S17_OLATB, vB) &&
                readRegister(MCP23S17_IODIRA, dA) &&
                readRegister(MCP23S17_IODIRB, dB);
    if (!(reOk && vA == expOlatA && vB == expOlatB && dA == 0x00 &&
          dB == 0x00)) {
        ESP_LOGE(TAG, "Actuator re-assert STILL mismatched — fault latched");
    }
    return false;
}

void RelayManager::verifyIfDue(uint32_t nowMs) {
    if (!s_initialized)
        return;
    if (!s_verifyStarted) {
        s_verifyStarted = true;
        s_lastVerifyMs = nowMs;
        return;  // ilk çağrıda periyodu başlat, hemen okuma yapma
    }
    if (nowMs - s_lastVerifyMs < RELAY_VERIFY_PERIOD_MS)
        return;
    s_lastVerifyMs = nowMs;
    verifyOutputs();
}

bool RelayManager::hasActuatorFault() const {
    return s_actuatorFault.load(std::memory_order_acquire);
}

void RelayManager::clearActuatorFault() {
    s_actuatorFault.store(false, std::memory_order_release);
}