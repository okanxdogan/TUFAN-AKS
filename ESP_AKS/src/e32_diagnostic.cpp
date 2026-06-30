// e32_diagnostic.cpp
// Geçici tanı aracı: E32-433T30D modülünün mevcut SPED/CHAN/OPTION register
// değerlerini SADECE OKUR, hiçbir yazma komutu göndermez.
//
// Etkinleştirme: -D E32_DIAGNOSTIC_MODE derleme flag'i (bkz. platformio.ini
// [env:esp32dev_diag] ortamı).  Bu flag olmadan dosya tamamen boş derlenir;
// normal firmware'e hiçbir etkisi yoktur.

#ifdef E32_DIAGNOSTIC_MODE

#include "SystemConfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr const char* TAG = "E32_DIAG";

// AUX HIGH bekleme ve RX okuma için maksimum süreler
static constexpr int DIAG_AUX_WAIT_MS = 500;
static constexpr int DIAG_RX_WAIT_MS  = 500;

// ---------------------------------------------------------------------------
// SPED byte ayrıştırma yardımcıları (E32-433T30D datasheet — Tablo 4)
// ---------------------------------------------------------------------------

static const char* DIAG_parityStr(uint8_t p) {
    // bits[7:6]
    switch (p) {
        case 0: return "8N1";
        case 1: return "8O1";
        case 2: return "8E1";
        case 3: return "8N1";
        default: return "?";
    }
}

static const char* DIAG_uartBaudStr(uint8_t b) {
    // bits[5:3]
    switch (b) {
        case 0: return "1200";
        case 1: return "2400";
        case 2: return "4800";
        case 3: return "9600";
        case 4: return "19200";
        case 5: return "38400";
        case 6: return "57600";
        case 7: return "115200";
        default: return "?";
    }
}

static const char* DIAG_airRateStr(uint8_t r) {
    // bits[2:0]
    switch (r) {
        case 0: return "0.3 kbps";
        case 1: return "1.2 kbps";
        case 2: return "2.4 kbps";
        case 3: return "4.8 kbps";
        case 4: return "9.6 kbps";
        default: return "19.2 kbps";
    }
}

static void DIAG_decodeSped(uint8_t sped) {
    const uint8_t parity = (sped >> 6) & 0x03u;
    const uint8_t baud   = (sped >> 3) & 0x07u;
    const uint8_t air    = sped & 0x07u;
    ESP_LOGI(TAG, "  SPED  = 0x%02X  ->  parity: %s | UART: %s bps | air rate: %s",
             sped, DIAG_parityStr(parity), DIAG_uartBaudStr(baud), DIAG_airRateStr(air));
}

// ---------------------------------------------------------------------------
// CHAN byte: kanal numarası + frekans (E32-433 serisi: 410 + CHAN MHz)
// ---------------------------------------------------------------------------
static void DIAG_decodeChan(uint8_t chan) {
    const unsigned freq = 410u + chan;
    ESP_LOGI(TAG, "  CHAN  = 0x%02X  ->  channel %u  (%u MHz)", chan, chan, freq);
}

// ---------------------------------------------------------------------------
// OPTION byte ayrıştırma (E32-433T30D datasheet — Tablo 5)
// bits[7]   : TX mode   (0=transparent, 1=fixed)
// bits[6]   : IO mode   (0=open-collector, 1=push-pull)
// bits[5:3] : wireless wake-up time
// bits[2]   : FEC       (0=off, 1=on)
// bits[1:0] : TX power  (00=30dBm, 01=27dBm, 10=24dBm, 11=21dBm)
// ---------------------------------------------------------------------------
static const char* DIAG_wakeupStr(uint8_t t) {
    static const char* tbl[] = {
        "250 ms","500 ms","750 ms","1000 ms","1250 ms","1500 ms","1750 ms","2000 ms"
    };
    return tbl[t & 0x07u];
}

static const char* DIAG_txPowerStr(uint8_t p) {
    switch (p) {
        case 0: return "30 dBm (1 W)";
        case 1: return "27 dBm";
        case 2: return "24 dBm";
        case 3: return "21 dBm";
        default: return "?";
    }
}

static void DIAG_decodeOption(uint8_t opt) {
    ESP_LOGI(TAG,
             "  OPTION= 0x%02X  ->  TX_mode: %s | IO: %s | wake-up: %s | FEC: %s | power: %s",
             opt,
             (opt >> 7) & 1u ? "fixed" : "transparent",
             (opt >> 6) & 1u ? "push-pull" : "open-collector",
             DIAG_wakeupStr((opt >> 3) & 0x07u),
             (opt >> 2) & 1u ? "on" : "off",
             DIAG_txPowerStr(opt & 0x03u));
}

// ---------------------------------------------------------------------------
// app_main — tek giriş noktası (sadece bu env derleniyor)
// ---------------------------------------------------------------------------
extern "C" void app_main() {
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "  E32-433T30D DIAGNOSTIC — READ ONLY");
    ESP_LOGI(TAG, "  Hicbir yazma komutu gonderilmeyecek.");
    ESP_LOGI(TAG, "================================================");

    // --- 1. M0, M1 GPIO çıkış olarak ayarla ---
    gpio_config_t DIAG_modeCfg = {};
    DIAG_modeCfg.pin_bit_mask   = (1ULL << LORA_M0_PIN) | (1ULL << LORA_M1_PIN);
    DIAG_modeCfg.mode           = GPIO_MODE_OUTPUT;
    DIAG_modeCfg.pull_up_en     = GPIO_PULLUP_DISABLE;
    DIAG_modeCfg.pull_down_en   = GPIO_PULLDOWN_DISABLE;
    DIAG_modeCfg.intr_type      = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&DIAG_modeCfg));

    // --- 2. AUX GPIO giriş olarak ayarla ---
    gpio_config_t DIAG_auxCfg = {};
    DIAG_auxCfg.pin_bit_mask  = (1ULL << LORA_AUX_PIN);
    DIAG_auxCfg.mode          = GPIO_MODE_INPUT;
    DIAG_auxCfg.pull_up_en    = GPIO_PULLUP_DISABLE;
    DIAG_auxCfg.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    DIAG_auxCfg.intr_type     = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&DIAG_auxCfg));

    // --- 3. Konfigürasyon moduna gir: M0=HIGH, M1=HIGH ---
    gpio_set_level(LORA_M0_PIN, 1);
    gpio_set_level(LORA_M1_PIN, 1);
    ESP_LOGI(TAG, "M0=HIGH M1=HIGH  ->  config mode");

    // --- 4. UART başlat ---
    // NOT: E32, konfigürasyon modunda SPED ayarından bağımsız olarak
    // daima 9600 baud 8N1 kullanır (datasheet Section 4.4).
    uart_config_t DIAG_uartCfg = {
        .baud_rate  = 9600,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(LORA_UART_NUM, &DIAG_uartCfg);
    uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(LORA_UART_NUM, 256, 0, 0, nullptr, 0);

    // --- 5. AUX HIGH bekle (modül config moduna hazır) ---
    ESP_LOGI(TAG, "AUX HIGH bekleniyor (max %d ms)...", DIAG_AUX_WAIT_MS);
    const TickType_t DIAG_t0 = xTaskGetTickCount();
    bool DIAG_auxReady = false;
    while ((xTaskGetTickCount() - DIAG_t0) < pdMS_TO_TICKS(DIAG_AUX_WAIT_MS)) {
        if (gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL) {
            DIAG_auxReady = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (DIAG_auxReady) {
        ESP_LOGI(TAG, "AUX HIGH  ->  modul hazir");
    } else {
        ESP_LOGW(TAG, "AUX %d ms icinde HIGH olmadi, devam ediliyor", DIAG_AUX_WAIT_MS);
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // mod geçişi stabilizasyonu

    // --- 6. RX buffer temizle, okuma sorgusu gönder: 0xC1 0xC1 0xC1 ---
    uart_flush(LORA_UART_NUM);
    const uint8_t DIAG_readCmd[3] = {0xC1, 0xC1, 0xC1};
    uart_write_bytes(LORA_UART_NUM,
                     reinterpret_cast<const char*>(DIAG_readCmd),
                     sizeof(DIAG_readCmd));
    ESP_LOGI(TAG, "Gonderildi: 0xC1 0xC1 0xC1  (mevcut parametreleri oku)");

    // --- 7. Yanıt oku: HEAD ADDH ADDL SPED CHAN OPTION (6 byte) ---
    uint8_t DIAG_resp[6] = {};
    const int DIAG_rxLen = uart_read_bytes(LORA_UART_NUM,
                                           DIAG_resp,
                                           sizeof(DIAG_resp),
                                           pdMS_TO_TICKS(DIAG_RX_WAIT_MS));

    if (DIAG_rxLen < 6) {
        ESP_LOGE(TAG, "Eksik yanit: %d/6 byte alindi", DIAG_rxLen);
        ESP_LOGE(TAG, "Olasiliklar: modul beslenmemis | yanlis kablo | AUX bagli degil");
    } else {
        ESP_LOGI(TAG, "-------- E32 Mevcut Parametreler --------");
        ESP_LOGI(TAG, "  HEAD  = 0x%02X  %s",
                 DIAG_resp[0],
                 DIAG_resp[0] == 0xC0 ? "(beklenen deger)" : "(!!! 0xC0 bekleniyor)");
        ESP_LOGI(TAG, "  ADDH  = 0x%02X", DIAG_resp[1]);
        ESP_LOGI(TAG, "  ADDL  = 0x%02X  ->  modul adresi: 0x%04X",
                 DIAG_resp[2],
                 static_cast<unsigned>((DIAG_resp[1] << 8) | DIAG_resp[2]));
        DIAG_decodeSped(DIAG_resp[3]);
        DIAG_decodeChan(DIAG_resp[4]);
        DIAG_decodeOption(DIAG_resp[5]);
        ESP_LOGI(TAG, "-----------------------------------------");
    }

    // --- 8. Normal moda geri dön: M0=LOW, M1=LOW ---
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(LORA_M0_PIN, LORA_MODE_NORMAL_M0_LEVEL);
    gpio_set_level(LORA_M1_PIN, LORA_MODE_NORMAL_M1_LEVEL);
    ESP_LOGI(TAG, "M0=LOW M1=LOW  ->  normal transparent mod geri yuklendi");

    uart_driver_delete(LORA_UART_NUM);

    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "  Tani tamamlandi. Normal firmware flash'leyin.");
    ESP_LOGI(TAG, "================================================");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "[E32_DIAG] Normal firmware flash'leyin: pio run -e esp32dev --target upload");
    }
}

#endif  // E32_DIAGNOSTIC_MODE
