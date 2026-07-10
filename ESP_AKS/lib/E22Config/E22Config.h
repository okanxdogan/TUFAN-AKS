#pragma once
// E22Config.h
// E22-400T30D-V2 register komutlarını inşa eden ve yanıtlarını doğrulayan
// SAF fonksiyonlar (ESP-IDF / UART bağımsız). Gerçek UART transferi
// src/main.cpp (vTask_LoRa_UKS) ve src/e22_diagnostic.cpp içinde yapılır;
// bu dosya yalnızca byte dizisi üretir/ayrıştırır, böylece native test
// edilebilir (bkz. test/test_native_e22_config).

#include <cstddef>
#include <cstdint>

#include "E22Regs.h"

// Register bloğunun (ADDH..CRYPT_L) bellek-içi karşılığı. CRYPT bilerek
// DAHİL DEĞİL: E22 CRYPT'i geri okuyamaz (okuma her zaman 0 döner), bu
// yüzden yazma-sonrası doğrulama karşılaştırmasına giremez.
struct E22RegValues {
  uint8_t addh;
  uint8_t addl;
  uint8_t netid;
  uint8_t reg0;
  uint8_t reg1;
  uint8_t reg2;
  uint8_t reg3;
};

// UKS ile birebir sözleşme değerleri (E22Regs.h E22_CFG_* sabitlerinden).
constexpr E22RegValues E22_CONTRACT_REGS = {
    E22_CFG_ADDH, E22_CFG_ADDL, E22_CFG_NETID,
    E22_CFG_REG0, E22_CFG_REG1, E22_CFG_REG2, E22_CFG_REG3,
};

// "0xC1 <start_addr> <len>" okuma komutu üretir (ADDH..CRYPT_L bloğunun
// tamamı, 3 byte). outBufSize yetersizse 0 döner.
size_t e22_buildReadAllCommand(uint8_t* outBuf, size_t outBufSize);

// "0xC0 <start_addr> <len> <val...>" yazma komutu üretir (CRYPT dahil,
// 3 + E22_REG_BLOCK_LEN byte). outBufSize yetersizse 0 döner.
size_t e22_buildWriteCommand(const E22RegValues& regs, uint8_t* outBuf,
                              size_t outBufSize);

// G7-FIX-2: "0xC2 <CRYPT_H_addr> 0x02 <CRYPT_H> <CRYPT_L>" KALICI OLMAYAN
// (RAM) yazma komutu üretir — yalnızca CRYPT_H/L (2 byte), ADDH..REG3'e
// dokunmaz. Read-before-write skip yolu (LoraLink::configureE22
// needsWrite=false dalı) CRYPT'i hiç karşılaştırmadığı ve REG0..REG3
// değişmediği sürece her boot'ta tetiklendiği için CRYPT hedefi
// güncellense bile flash'a hiç yazılmıyor olabilir (bkz. Documents/
// E22_CRYPT_SENKRON.md "G7-FIX-2"). Bu komut o dalda her boot'ta
// gönderilerek CRYPT'in güç kesilene kadar güncel kalmasını garanti eder
// — flash ömrünü etkilemez. outBufSize < 5 ise 0 döner.
size_t e22_buildWriteCryptTempCommand(uint8_t* outBuf, size_t outBufSize);

// Modülün "0xFF 0xFF 0xFF" hata çerçevesini gönderip göndermediğini
// kontrol eder (komut reddedildi / modül hazır değil).
bool e22_isErrorResponse(const uint8_t* resp, size_t respLen);

// "0xC1 <start_addr> <len> <val...>" biçimindeki bir yanıtı ayrıştırır —
// hem C1 okuma sorgusunun yanıtı hem de C0 yazma sonrası modülün gönderdiği
// onay için kullanılır (E22, E32'nin aksine yazmayı 0xC0 ile değil 0xC1
// formatıyla onaylar). Başlık/adres/uzunluk uyuşmazsa false döner ve out
// dokunulmadan kalır.
bool e22_parseRegResponse(const uint8_t* resp, size_t respLen,
                           E22RegValues& out);

// İki register kümesini karşılaştırır (CRYPT struct'ta olmadığı için
// karşılaştırma dışıdır).
bool e22_regsEqual(const E22RegValues& a, const E22RegValues& b);
