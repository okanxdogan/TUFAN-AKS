#include "LoraLink.h"

#include "E22Config.h"
#include "E22Regs.h"
#include "SystemConfig.h"
#include "esp_log.h"

static constexpr const char* TAG = "LoRa";

bool LoraLink::waitAuxReady(uint32_t timeoutMs) {
    const uint64_t t0 = m_hal.nowMs();
    while ((m_hal.nowMs() - t0) < (uint64_t)timeoutMs) {
        if (m_hal.isAuxReady())
            return true;
        m_hal.delayMs(10);
    }
    return false;
}

void LoraLink::configureE22() {
    // --- E22 boot konfigürasyonu: config moduna gir + oku + gerekirse yaz + doğrula ---
    m_hal.setModePins(LORA_MODE_CONFIG_M0_LEVEL, LORA_MODE_CONFIG_M1_LEVEL);
    ESP_LOGI(TAG, "E22: config moduna giriliyor (M0=%d M1=%d)",
             LORA_MODE_CONFIG_M0_LEVEL, LORA_MODE_CONFIG_M1_LEVEL);

    if (!waitAuxReady(LORA_AUX_MODE_TIMEOUT_MS)) {
        ESP_LOGE(TAG, "E22 AUX timeout (%d ms): config moduna girilemedi, "
                      "normal moda devam ediliyor",
                 LORA_AUX_MODE_TIMEOUT_MS);
    } else {
        // 1. Mevcut bloğu oku (0xC1 <start> <len>) — bench teyidi için hex dump
        m_hal.uartFlush();
        uint8_t readCmd[3];
        const size_t readCmdLen = e22_buildReadAllCommand(readCmd, sizeof(readCmd));
        m_hal.uartWrite(readCmd, readCmdLen);

        uint8_t readResp[3 + E22_REG_BLOCK_LEN] = {};
        const int readLen =
            m_hal.uartRead(readResp, sizeof(readResp), LORA_CFG_READ_TIMEOUT_MS);
        m_hal.hexDumpE22("E22 mevcut config yaniti", readResp,
                         readLen > 0 ? readLen : 0);

        bool needsWrite = true;
        if (readLen > 0 && e22_isErrorResponse(readResp, (size_t)readLen)) {
            ESP_LOGE(TAG, "E22 okuma reddedildi (FF FF FF) — yazma denenecek");
        } else {
            E22RegValues current = {};
            if (e22_parseRegResponse(readResp, (size_t)readLen, current)) {
                if (e22_regsEqual(current, E22_CONTRACT_REGS)) {
                    ESP_LOGI(TAG, "E22 config zaten sozlesmeyle uyumlu, yazma "
                                  "atlaniyor");
                    needsWrite = false;
                } else {
                    ESP_LOGW(TAG, "E22 mevcut config sozlesmeden farkli — yaziliyor");
                }
            } else {
                ESP_LOGE(TAG, "E22 okuma yaniti eksik/hatali (%d byte) — yazma "
                              "denenecek",
                         readLen);
            }
        }

        // 2. Fark varsa (ya da okunamadıysa) kalıcı yazma komutu gönder
        if (needsWrite) {
            m_hal.uartFlush();
            uint8_t writeCmd[3 + E22_REG_BLOCK_LEN];
            const size_t writeCmdLen =
                e22_buildWriteCommand(E22_CONTRACT_REGS, writeCmd, sizeof(writeCmd));
            m_hal.uartWrite(writeCmd, writeCmdLen);

            // Flash yazımının bitmesini bekle: AUX LOW→HIGH geçişi (~200ms tipik)
            m_hal.delayMs(50);  // modülün LOW'a geçmesine zaman ver
            const bool flashDone = waitAuxReady(LORA_AUX_CFG_TIMEOUT_MS);

            if (!flashDone) {
                ESP_LOGE(TAG, "E22 flash AUX timeout (%d ms): config yazildi ancak "
                              "dogrulanamadi, devam ediliyor",
                         LORA_AUX_CFG_TIMEOUT_MS);
            } else {
                // 3. Yazma sonrası onay: E22, C0 değil C1 formatıyla yanıt verir
                uint8_t confirm[3 + E22_REG_BLOCK_LEN] = {};
                const int confirmLen =
                    m_hal.uartRead(confirm, sizeof(confirm), LORA_CFG_READ_TIMEOUT_MS);
                m_hal.hexDumpE22("E22 yazma onay yaniti", confirm,
                                 confirmLen > 0 ? confirmLen : 0);

                if (confirmLen > 0 &&
                    e22_isErrorResponse(confirm, (size_t)confirmLen)) {
                    ESP_LOGE(TAG, "E22 config yazma reddedildi (FF FF FF) — modul "
                                  "eski config ile devam ediyor olabilir, gorevde "
                                  "kaliniyor");
                } else {
                    E22RegValues written = {};
                    if (e22_parseRegResponse(confirm, (size_t)confirmLen, written) &&
                        e22_regsEqual(written, E22_CONTRACT_REGS)) {
                        ESP_LOGI(TAG,
                                 "E22 config dogrulandi: NETID=0x%02X REG0=0x%02X "
                                 "REG1=0x%02X REG2=0x%02X REG3=0x%02X",
                                 written.netid, written.reg0, written.reg1,
                                 written.reg2, written.reg3);
                    } else {
                        ESP_LOGE(TAG, "E22 config yazma onayi uyusmuyor (%d byte) — "
                                      "gorevde kaliniyor",
                                 confirmLen);
                    }
                }
            }
        }
    }

    // Normal transparan moda dön; AUX HIGH gelene kadar TX/RX başlatma
    m_hal.setModePins(LORA_MODE_NORMAL_M0_LEVEL, LORA_MODE_NORMAL_M1_LEVEL);
    waitAuxReady(LORA_AUX_MODE_TIMEOUT_MS);
    ESP_LOGI(TAG, "E22: normal mod (M0=%d M1=%d AUX=%d)",
             LORA_MODE_NORMAL_M0_LEVEL, LORA_MODE_NORMAL_M1_LEVEL,
             m_hal.auxRawLevel());
}
