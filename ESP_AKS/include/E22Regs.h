#ifndef E22_REGS_H
#define E22_REGS_H

// E22Regs.h
// E22-400T30D-V2 (SX1268) register-command sözleşmesi.
//
// !!! ÇAPRAZ-REPO SENKRONIZASYON UYARISI !!!
// Bu dosyadaki adres ve değer sabitleri, TUFAN-UKS reposundaki e22_regs.h
// ile BİREBİR AYNI olmalıdır. İki repo ayrı derlendiği için ortak bir
// header paylaşılamıyor; senkronizasyon manuel olarak korunur. Bu iki
// dosyadan biri değiştirilirse diğeri de AYNI COMMIT'te güncellenmelidir.
// Contract-drift native testi (bkz. E22 geçiş PROMPT 9) bu iki dosyanın
// değer tablosunu karşılaştırır — burada bir sabit değişirse UKS
// tarafında da aynı değişikliği yapmadan testler kırmızı kalmalıdır.
//
// Komut formatı (EBYTE E22 / SX126x ailesi, E32'den FARKLI):
//   Yazma : 0xC0 <start_addr> <len> <val...>       (yanıt: 0xC1 aynı format)
//   Okuma : 0xC1 <start_addr> <len>                (yanıt: 0xC1 <start_addr> <len> <val...>)
//   Hata  : 0xFF 0xFF 0xFF (modül komutu kabul etmedi / hazır değil)
// E32'nin aksine yazma komutuna modül kendi başlığını (0xC0) DEĞİL, okuma
// başlığını (0xC1) ile yanıt verir — bu yüzden yazma sonrası doğrulama da
// okuma yanıtıyla aynı ayrıştırıcıyı (E22Config) kullanır.

#include <cstdint>

// --- Komut byte'ları ---
#define E22_CMD_WRITE      0xC0U
#define E22_CMD_READ       0xC1U
#define E22_CMD_WRITE_TEMP 0xC2U  // G7-FIX-2: kalici OLMAYAN (RAM) yazma

// --- Register adresleri (E22-400T30D-V2 register haritası) ---
// DOĞRULANDI (2026-07-15, bench dump — bkz. Documents/BENCH_E22_TEYIT.md
// "Sonuç Kaydı"): NETID (0x02) ve sonrasındaki REG0..REG3 kaydırması,
// EBYTE'ın SX126x tabanlı E22 ailesi için yaygın register haritasına
// dayanıyordu; bench testinde AKS ve UKS C1 okuma dump'ları
// (E22_DIAGNOSTIC_MODE / bkz. e22_diagnostic.cpp) bu adreslerin gerçek
// donanımla ve birbiriyle birebir eştiğini doğruladı.
#define E22_REG_ADDR_ADDH    0x00U
#define E22_REG_ADDR_ADDL    0x01U
#define E22_REG_ADDR_NETID   0x02U
#define E22_REG_ADDR_REG0    0x03U
#define E22_REG_ADDR_REG1    0x04U
#define E22_REG_ADDR_REG2    0x05U
#define E22_REG_ADDR_REG3    0x06U
#define E22_REG_ADDR_CRYPT_H 0x07U
#define E22_REG_ADDR_CRYPT_L 0x08U

#define E22_REG_BLOCK_START E22_REG_ADDR_ADDH
#define E22_REG_BLOCK_LEN   0x09U  // ADDH..CRYPT_L dahil, 9 byte

// --- Sözleşme değerleri (ORTAK BLOK — E22 KONFİGÜRASYON SÖZLEŞMESİ) ---
// Bu değerler UKS e22_regs.h ile birebir aynı taşınmalıdır.
#define E22_CFG_ADDH    0x00U  // adres 0x0000: hedefli değil, genel yayın
#define E22_CFG_ADDL    0x00U
#define E22_CFG_NETID   0x00U
// REG0 = 0110 0010: bit[7:5]=011 UART 9600 | bit[4:3]=00 8N1 | bit[2:0]=010 hava hızı 2.4 kbps
// (menzil artışı için 9.6 kbps'ten düşürüldü, ekip onaylı kalibrasyon — bkz.
// LORA_TX_PERIOD_MS SystemConfig.h'de aynı commit'te 500->1000 ms'e ayarlandı)
#define E22_CFG_REG0    0x62U
// REG1 = bit[7:6]=00 alt-paket 240B | bit5=0 RSSI ortam gürültüsü kapalı | bit[1:0]=00 TX gücü EN YÜKSEK (T30D: 30 dBm)
// DİKKAT: E22'de de 00=maks, 11=min — E32'deki OPTION karışıklığı tekrarlanmasın.
#define E22_CFG_REG1    0x00U
// REG2 = kanal 23 -> 410.125 + 23 = 433.125 MHz (433 ISM bandı: 433.05-434.79 içinde)
#define E22_CFG_REG2    0x17U
// REG3 = bit7=0 RSSI byte eklenmez | bit6=0 transparan | bit5=0 röle kapalı | bit4=0 LBT kapalı
#define E22_CFG_REG3    0x00U
// CRYPT: sıfır-dışı ortak anahtar (G7 — heartbeat-injection açığı kapatma).
// Gerekçe: broadcast (ADDH/ADDL/NETID=0) + transparan mod + tek sabit byte
// (0xB0) link canlılığı. CRYPT=0x0000 iken 433.125 MHz'deki HERHANGİ bir
// E22 modülü saniyede bir 0xB0 basarsa, gerçek UKS kapalıyken bile AKS
// linki "UP" sanar → offline örnekleme hiç başlamaz → kesinti verisi
// KALICI KAYBOLUR. Sıfır-dışı ortak gizli anahtar bunu neredeyse bedavaya
// kapatır. Heartbeat protokolü (0xB0) DEĞİŞMEZ; yalnızca RF katmanı
// şifrelenir. E22 CRYPT'i geri OKUYAMAZ (okuma her zaman 0 döner) →
// yazılır ama e22_regsEqual doğrulamasından HARİÇ tutulur (bkz.
// E22Config). CRYPT_H/L LİTERAL tutulur (drift-guard değerleri sayısal
// parse eder); 16-bit E22_CRYPT_KEY ile tutarlılık E22Config.cpp'de
// static_assert ile bağlanır.
//
// !!! ÇAPRAZ-REPO SENKRON !!! Bu değerler UKS Core/Inc/e22_regs.h
// (E22_VAL_CRYPT_H/L) ve tools/e2e/contract.py (E22_CRYPT_H/L) ile AYNI
// COMMIT'te birebir güncellenmelidir. Drift-guard:
// test_contract_drift.py::test_e22_register_targets_match_across_repos.
// Geçmiş: 90c8e1b'de bu değer AKS tarafında tek başına 0x5A3C'ye
// çekilmiş, UKS güncellenmeden bırakılınca link kırılmış ve aynı
// commit'te ikisi de 0x0000'a geri alınmıştı. Bkz. Documents/
// E22_CRYPT_SENKRON.md "Geçmiş" bölümü — bu kez üç dosya + UKS AYNI
// commit setinde senkron güncellendi.
#define E22_CRYPT_KEY   0x5A3CU  // 16-bit ortak anahtar (UKS ile aynı)
#define E22_CFG_CRYPT_H 0x5AU    // = (E22_CRYPT_KEY >> 8) & 0xFF
#define E22_CFG_CRYPT_L 0x3CU    // = E22_CRYPT_KEY & 0xFF

#endif  // E22_REGS_H
