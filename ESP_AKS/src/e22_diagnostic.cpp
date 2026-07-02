// e22_diagnostic.cpp
// Geçici tanı aracı: E22-400T30D-V2 modülünün mevcut register bloğunu
// (ADDH..CRYPT_L) okuyup hex/insan-okunur biçimde dump eder, ardından
// normal transparan moda dönüp temel bir TX smoke testi yapar. Hiçbir
// kalıcı yazma komutu göndermez.
//
// Etkinleştirme: -D E22_DIAGNOSTIC_MODE derleme flag'i (bkz. platformio.ini
// [env:esp32dev_diag] ortamı).  Bu flag olmadan dosya tamamen boş derlenir;
// normal firmware'e hiçbir etkisi yoktur.
//
// Ayrıntılı saha/bench prosedürü (menzil testi, hata senaryoları) ayrı
// belgelenecektir; bu dosyanın amacı yalnızca derlenip register dump
// atabilmesini ve UART'ın temel TX/RX yolunun çalıştığını göstermektir.

#ifdef E22_DIAGNOSTIC_MODE

#include <cstdio>

#include "E22Config.h"
#include "SystemConfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr const char* TAG = "E22_DIAG";

// AUX HIGH bekleme ve RX okuma için maksimum süreler
static constexpr int DIAG_AUX_WAIT_MS = 500;
static constexpr int DIAG_RX_WAIT_MS  = 500;

// ---------------------------------------------------------------------------
// REG0 ayrıştırma (bkz. E22Regs.h ORTAK BLOK):
//   bit[7:5] = UART baud (3-bit)
//   bit[4:3] = parity / veri formatı
//   bit[2:0] = air data rate
// ---------------------------------------------------------------------------
static const char* DIAG_uartBaudStr(uint8_t b) {
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

static const char* DIAG_parityStr(uint8_t p) {
    switch (p & 0x03u) {
        case 0: return "8N1";
        case 1: return "8O1";
        case 2: return "8E1";
        case 3: return "8N1";
        default: return "8N1";
    }
}

static const char* DIAG_airRateStr(uint8_t r) {
    switch (r) {
        case 0: return "0.3 kbps";
        case 1: return "1.2 kbps";
        case 2: return "2.4 kbps";
        case 3: return "4.8 kbps";
        case 4: return "9.6 kbps";
        case 5: return "19.2 kbps";
        case 6: return "38.4 kbps";
        default: return "62.5 kbps";
    }
}

static void DIAG_decodeReg0(uint8_t reg0) {
    const uint8_t baud   = (reg0 >> 5) & 0x07u;  // bit[7:5]
    const uint8_t parity = (reg0 >> 3) & 0x03u;  // bit[4:3]
    const uint8_t air    = reg0 & 0x07u;         // bit[2:0]
    ESP_LOGI(TAG, "  REG0  = 0x%02X  ->  UART: %s bps | parity: %s | air rate: %s",
             reg0, DIAG_uartBaudStr(baud), DIAG_parityStr(parity), DIAG_airRateStr(air));
}

// ---------------------------------------------------------------------------
// REG1 ayrıştırma:
//   bit[7:6] : alt-paket boyutu (00=240B, 01=128B, 10=64B, 11=32B)
//   bit[5]   : RSSI ortam gürültüsü (0=kapali, 1=acik)
//   bit[1:0] : TX gücü — DİKKAT: E22'de 00=maks (30 dBm), 11=min
// ---------------------------------------------------------------------------
static const char* DIAG_subPacketStr(uint8_t s) {
    switch (s & 0x03u) {
        case 0: return "240 B";
        case 1: return "128 B";
        case 2: return "64 B";
        default: return "32 B";
    }
}

static const char* DIAG_txPowerStr(uint8_t p) {
    switch (p & 0x03u) {
        case 0: return "en yuksek (T30D: 30 dBm)";
        case 1: return "orta-yuksek";
        case 2: return "orta-dusuk";
        default: return "en dusuk (~10 dBm)";
    }
}

static void DIAG_decodeReg1(uint8_t reg1) {
    ESP_LOGI(TAG,
             "  REG1  = 0x%02X  ->  alt-paket: %s | RSSI ortam gurultusu: %s | TX gucu: %s",
             reg1,
             DIAG_subPacketStr((reg1 >> 6) & 0x03u),
             (reg1 >> 5) & 1u ? "acik" : "kapali",
             DIAG_txPowerStr(reg1 & 0x03u));
}

// ---------------------------------------------------------------------------
// REG2: kanal numarası + frekans (E22-400 serisi: 410.125 + REG2 MHz)
// ---------------------------------------------------------------------------
static void DIAG_decodeReg2(uint8_t reg2) {
    const float freq = 410.125f + (float)reg2;
    ESP_LOGI(TAG, "  REG2  = 0x%02X  ->  kanal %u  (%.3f MHz)", reg2, reg2, freq);
}

// ---------------------------------------------------------------------------
// REG3 ayrıştırma:
//   bit[7] : RSSI byte ekle (0=hayir, 1=evet)
//   bit[6] : iletim modu (0=transparan, 1=fixed)
//   bit[5] : röle (0=kapali, 1=acik)
//   bit[4] : LBT  (0=kapali, 1=acik)
//   bit[3:0] : WOR ayarlari (kullanilmiyor)
// ---------------------------------------------------------------------------
static void DIAG_decodeReg3(uint8_t reg3) {
    ESP_LOGI(TAG,
             "  REG3  = 0x%02X  ->  RSSI byte: %s | mod: %s | role: %s | LBT: %s",
             reg3,
             (reg3 >> 7) & 1u ? "ekleniyor" : "eklenmiyor",
             (reg3 >> 6) & 1u ? "fixed" : "transparan",
             (reg3 >> 5) & 1u ? "acik" : "kapali",
             (reg3 >> 4) & 1u ? "acik" : "kapali");
}

// ---------------------------------------------------------------------------
// app_main — tek giriş noktası (sadece bu env derleniyor)
// ---------------------------------------------------------------------------
extern "C" void app_main() {
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "  E22-400T30D-V2 DIAGNOSTIC — READ ONLY + TX SMOKE");
    ESP_LOGI(TAG, "  Hicbir kalici yazma komutu gonderilmeyecek.");
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

    // --- 3. Konfigürasyon moduna gir: M0=0, M1=1 (E22 — E32'den farklı!) ---
    gpio_set_level(LORA_M0_PIN, LORA_MODE_CONFIG_M0_LEVEL);
    gpio_set_level(LORA_M1_PIN, LORA_MODE_CONFIG_M1_LEVEL);
    ESP_LOGI(TAG, "M0=%d M1=%d  ->  config mode",
             LORA_MODE_CONFIG_M0_LEVEL, LORA_MODE_CONFIG_M1_LEVEL);

    // --- 4. UART başlat ---
    // NOT: E22, konfigürasyon modunda REG0 ayarından bağımsız olarak
    // daima 9600 baud 8N1 kullanır (E32 ile aynı davranış).
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
    uart_driver_install(LORA_UART_NUM, 256, 256, 0, nullptr, 0);

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

    // --- 6. RX buffer temizle, okuma sorgusu gönder: 0xC1 <start> <len> ---
    uart_flush(LORA_UART_NUM);
    uint8_t DIAG_readCmd[3];
    const size_t DIAG_readCmdLen =
        e22_buildReadAllCommand(DIAG_readCmd, sizeof(DIAG_readCmd));
    uart_write_bytes(LORA_UART_NUM,
                     reinterpret_cast<const char*>(DIAG_readCmd),
                     DIAG_readCmdLen);
    ESP_LOGI(TAG, "Gonderildi: 0xC1 0x%02X 0x%02X  (mevcut parametreleri oku)",
             E22_REG_BLOCK_START, E22_REG_BLOCK_LEN);

    // --- 7. Yanıt oku: 0xC1 <start> <len> <val...> (3 + 9 = 12 byte) ---
    uint8_t DIAG_resp[3 + E22_REG_BLOCK_LEN] = {};
    const int DIAG_rxLen = uart_read_bytes(LORA_UART_NUM,
                                           DIAG_resp,
                                           sizeof(DIAG_resp),
                                           pdMS_TO_TICKS(DIAG_RX_WAIT_MS));

    // Bench teyidi: register basina bir satir, UKS ile birebir eslesmesi
    // zorunlu format: "E22REG,0x%02X,0x%02X\r\n" (adres, deger).
    // NOT: E22REG satirlari kasitli olarak ciplak printf ile basilir
    // (ESP_LOGI DEGIL) — UKS ayni satiri ciplak printf ile basiyor,
    // bench'te iki cikti satir satir diff'leniyor; ESP_LOGI'nin
    // "I (ts) TAG:" oneki ve cift satir sonu diff'i bozar. Baslik ayirici
    // satiri diff kapsami disinda, ESP_LOGI kalir.
    ESP_LOGI(TAG, "--- Ham yanit (%d byte) ---", DIAG_rxLen);
    if (DIAG_rxLen >= (int)(3 + E22_REG_BLOCK_LEN)) {
        for (uint8_t i = 0; i < E22_REG_BLOCK_LEN; i++) {
            printf("E22REG,0x%02X,0x%02X\r\n", E22_REG_BLOCK_START + i,
                   DIAG_resp[3 + i]);
        }
    }

    if (DIAG_rxLen > 0 && e22_isErrorResponse(DIAG_resp, (size_t)DIAG_rxLen)) {
        ESP_LOGE(TAG, "E22 FF FF FF hata yaniti — komut reddedildi / modul hazir degil");
    } else {
        E22RegValues DIAG_regs = {};
        if (!e22_parseRegResponse(DIAG_resp, (size_t)DIAG_rxLen, DIAG_regs)) {
            ESP_LOGE(TAG, "Eksik/hatali yanit: %d/%d byte alindi",
                     DIAG_rxLen, (int)(3 + E22_REG_BLOCK_LEN));
            ESP_LOGE(TAG, "Olasiliklar: modul beslenmemis | yanlis kablo | AUX bagli degil");
        } else {
            ESP_LOGI(TAG, "-------- E22 Mevcut Parametreler --------");
            ESP_LOGI(TAG, "  ADDH  = 0x%02X", DIAG_regs.addh);
            ESP_LOGI(TAG, "  ADDL  = 0x%02X  ->  modul adresi: 0x%04X",
                     DIAG_regs.addl,
                     static_cast<unsigned>((DIAG_regs.addh << 8) | DIAG_regs.addl));
            ESP_LOGI(TAG, "  NETID = 0x%02X", DIAG_regs.netid);
            DIAG_decodeReg0(DIAG_regs.reg0);
            DIAG_decodeReg1(DIAG_regs.reg1);
            DIAG_decodeReg2(DIAG_regs.reg2);
            DIAG_decodeReg3(DIAG_regs.reg3);
            ESP_LOGI(TAG, "  (CRYPT geri okunamaz — bu dump'ta yok)");
            ESP_LOGI(TAG, "-----------------------------------------");

            if (e22_regsEqual(DIAG_regs, E22_CONTRACT_REGS)) {
                ESP_LOGI(TAG, "Sozlesme karsilastirmasi: UYUMLU (E22_CONTRACT_REGS ile birebir)");
            } else {
                ESP_LOGW(TAG, "Sozlesme karsilastirmasi: FARKLI — normal firmware boot'ta yazacak");
            }
        }
    }

    // --- 8. Normal moda geri dön: M0=0, M1=0 ---
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(LORA_M0_PIN, LORA_MODE_NORMAL_M0_LEVEL);
    gpio_set_level(LORA_M1_PIN, LORA_MODE_NORMAL_M1_LEVEL);
    ESP_LOGI(TAG, "M0=%d M1=%d  ->  normal transparent mod geri yuklendi",
             LORA_MODE_NORMAL_M0_LEVEL, LORA_MODE_NORMAL_M1_LEVEL);

    // --- 9. Temel TX/RX smoke testi ---
    // Gerçek bir echo/loopback GARANTİ EDİLMEZ (karşı uçta dinleyen bir UKS
    // birimi yoksa RX boş kalır) — bu adım sadece UART TX yolunun donmadan
    // tamamlandığını ve AUX'un normal moda döndükten sonra HIGH kaldığını
    // doğrular. Ayrıntılı saha eşleştirme testi ayrı prosedürde yapılır.
    {
        const TickType_t DIAG_normT0 = xTaskGetTickCount();
        bool DIAG_normAuxReady = false;
        while ((xTaskGetTickCount() - DIAG_normT0) < pdMS_TO_TICKS(DIAG_AUX_WAIT_MS)) {
            if (gpio_get_level(LORA_AUX_PIN) == LORA_AUX_READY_LEVEL) {
                DIAG_normAuxReady = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Normal mod AUX durumu: %s",
                 DIAG_normAuxReady ? "HIGH (hazir)" : "HIGH olmadi (timeout)");

        static const char DIAG_pingMsg[] = "E22_DIAG_PING\r\n";
        uart_flush(LORA_UART_NUM);
        const int DIAG_txBytes = uart_write_bytes(LORA_UART_NUM, DIAG_pingMsg,
                                                   sizeof(DIAG_pingMsg) - 1);
        ESP_LOGI(TAG, "TX smoke: %d byte gonderildi", DIAG_txBytes);

        uint8_t DIAG_echoBuf[32] = {};
        const int DIAG_echoLen = uart_read_bytes(LORA_UART_NUM, DIAG_echoBuf,
                                                  sizeof(DIAG_echoBuf) - 1,
                                                  pdMS_TO_TICKS(DIAG_RX_WAIT_MS));
        if (DIAG_echoLen > 0) {
            DIAG_echoBuf[DIAG_echoLen] = '\0';
            ESP_LOGI(TAG, "RX smoke: %d byte alindi: %s", DIAG_echoLen,
                     reinterpret_cast<const char*>(DIAG_echoBuf));
        } else {
            ESP_LOGI(TAG, "RX smoke: yanit yok (karsi uc dinlemiyor olabilir — beklenen)");
        }
    }

    uart_driver_delete(LORA_UART_NUM);

    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "  Tani tamamlandi. Normal firmware flash'leyin.");
    ESP_LOGI(TAG, "================================================");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "[E22_DIAG] Normal firmware flash'leyin: pio run -e esp32dev --target upload");
    }
}

#endif  // E22_DIAGNOSTIC_MODE
