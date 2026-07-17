#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// SystemConfig.h
// Centralized configuration for pin assignments, timeouts, and other constants
// Used across multiple modules for consistency

// --- Includes ---
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "E22Regs.h"

// --- CAN Message IDs ---
//#define CAN_ID_TORQUE_CMD 0x100    // AKS → Motor Driver
#define CAN_ID_MOTOR_STATUS 0x200  // Motor Driver → AKS

// Lithium Balance c-BMS — 29-bit Extended ID, Big Endian
// Gerçek CAN sniffer loglarından doğrulanmış ID'ler.
// 0xE000: tüm alanlar (packV, current, SoC1, SoC2) DOĞRULANDI.
// 0xE001: byte[6:7] sıcaklık DOĞRULANDI; byte[0:5] BİLİNMİYOR.
// E002-E005, E032, E033: alan anlamları BİLİNMİYOR.
// Bkz. Documents/CAN_Message_Table.md (tek doğruluk kaynağı).
#define CAN_ID_LB_BMS_E000 0x0000E000  // DOĞRULANDI: PackV, Current, SoC1, SoC2
#define CAN_ID_LB_BMS_E001 0x0000E001  // DOĞRULANDI: Temp (byte[6:7]), min/max/avg cellV (byte[0:5])
#define CAN_ID_LB_BMS_E002 0x0000E002  // statik konfig/limit yayını (sabit, oturum boyunca değişmez, anlam kesin doğrulanmadı)
#define CAN_ID_LB_BMS_E003 0x0000E003  // statik konfig/limit yayını (sabit, oturum boyunca değişmez, anlam kesin doğrulanmadı)
#define CAN_ID_LB_BMS_E004 0x0000E004  // statik konfig/limit yayını (sabit, oturum boyunca değişmez, anlam kesin doğrulanmadı)
#define CAN_ID_LB_BMS_E005 0x0000E005  // statik konfig/limit yayını (sabit, oturum boyunca değişmez, anlam kesin doğrulanmadı)
#define CAN_ID_LB_BMS_E006 0x0000E006  // statik konfig/limit yayını (sabit, oturum boyunca değişmez, anlam kesin doğrulanmadı)
#define CAN_ID_LB_BMS_E015 0x0000E015  // DOĞRULANDI: hücre 0-3 voltajı (raw/10 = mV)
#define CAN_ID_LB_BMS_E016 0x0000E016  // DOĞRULANDI: hücre 4-7 voltajı
#define CAN_ID_LB_BMS_E017 0x0000E017  // DOĞRULANDI: hücre 8-11 voltajı
#define CAN_ID_LB_BMS_E018 0x0000E018  // DOĞRULANDI: hücre 12-15 voltajı
#define CAN_ID_LB_BMS_E019 0x0000E019  // DOĞRULANDI: hücre 16-19 voltajı
#define CAN_ID_LB_BMS_E020 0x0000E020  // DOĞRULANDI: hücre 20-23 voltajı
#define CAN_ID_LB_BMS_E032 0x0000E032  // BİLİNMİYOR — gözlemlenen oturumda hep sıfır
#define CAN_ID_LB_BMS_E033 0x0000E033  // BİLİNMİYOR — gözlemlenen oturumda hep sıfır

// Diagnostic Sniffer Modu: E002-E005, E032, E033 gibi bilinmeyen ID'lerin
// içeriğinde hücre voltajı (2.5V-3.65V) paternlerini arayıp loglamak için 1 yapın.
#define ENABLE_BMS_DIAGNOSTIC_SNIFFER 0

// Charger komut frame'i — 29-bit Extended ID (J1939: PGN 0x1806, DA 0xE5,
// SA 0xF4). BMS -> Charger yönünde; byte[0:1] = şarj voltaj hedefi ×0.1 V,
// byte[2:3] = şarj akım hedefi ×0.1 A (bkz. Documents/CAN_Message_Table.md).
// AKS bu frame'i YALNIZCA DİNLER, asla göndermez. Şu an işlenmiyor.
#define CAN_ID_LB_CHARGER_CMD 0x1806E5F4

// CAN sniffer loglarında ara sıra görülen 11-bit standart frame.
// Tüm byte'ları sıfır, anlamı bilinmiyor. Şu an işlenmiyor.
#define CAN_ID_LB_STD_0x000 0x000      // STD 11-bit — TODO: alan anlamı doğrulanmadı

// --- CAN (TJA1050 transceiver) ---
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// --- CAN RX Yolu Sertleştirme (G6) ---
// TWAI sürücüsünün donanım RX kuyruğu derinliği. Varsayılan 5 idi; 100 Hz
// işleme × 5 = 500 frame/s tavan yaratıyordu. 500 kbps bus ~3800 frame/s
// taşıyabilir ve BMS 9+ ID'yi 10 ms penceresinde art arda basınca kuyruk
// ALARMSIZ taşıyordu. 32'ye çıkarıldı + TWAI_ALERT_RX_QUEUE_FULL etkin.
// Bellek maliyeti: TWAI sürücüsü her slot için ~sizeof(twai_hal_frame_t)
// (~16 B) ayırır → 32 slot ≈ ~0.5 KB RAM (varsayılan 5'e göre ~430 B fazla).
#define CAN_RX_QUEUE_LEN 32
// processRxMessages tek çağrıda kuyruğu boşalana kadar okur; bu üst sınır
// task açlığına / sonsuz döngüye karşı emniyet (kuyruk derinliğiyle aynı).
#define CAN_RX_DRAIN_MAX CAN_RX_QUEUE_LEN
// RX_QUEUE_FULL / drop istatistiklerini oran-sınırlı loglama aralığı (özet):
// her olayda değil, en fazla bu sürede bir WARN.
#define CAN_RX_STATS_LOG_INTERVAL_MS 1000U

// --- CAN Autobaud Yeniden-Deneme (Kalıcı Sağırlık Düzeltmesi) ---
// CanManager::begin() üçü de başarısız olursa 500 kbps'e fallback yapardı ve
// BİR DAHA ASLA yeniden denemezdi — BMS boot anında sessizse (uykuda/geç
// açılıyor) veya bus gerçekte 125/250 kbps ise AKS kalıcı olarak sağır
// kalıyordu (saha olayı, bkz. Documents/BRING_UP_CHECKLIST.md bölüm 4).
// Bitrate doğrulanmamışken VE henüz hiçbir geçerli frame alınmamışken (saf
// karar mantığı: lib/CanManager/AutobaudPolicy.h::autobaud_should_retry) CAN
// task döngüsünden bu aralıkta bir yeniden algılama denenir. Tek tikte 3
// hızın TÜMÜ denenmez (task döngüsünü 3×1 sn kilitler) — her retry tikinde
// rotasyonla TEK hız denenir (bkz. CanManager::retryAutobaudIfNeeded).
#define CAN_AUTOBAUD_RETRY_INTERVAL_MS 5000U
// Fallback'te doğrulanamamış kaldığı sürece görünürlük: en fazla 1 WARN / bu
// süre (spam önleme, RX_QUEUE_FULL loglamasıyla aynı desen).
#define CAN_AUTOBAUD_WARN_LOG_INTERVAL_MS 60000U

// --- Nextion HMI (UART) ---
#define HMI_UART_NUM UART_NUM_1
// Not: J8 konnektöründe screen_TX'in ekranın mı yoksa ESP'nin mi TX'i olduğuna
// göre aşağıdaki 32 ve 33 yer değiştirebilir, ancak donanım pinleri 32 ve
// 33'tür.
#define HMI_TX_PIN GPIO_NUM_33  // Şemadaki screen_RX (ESP TX -> Ekran RX)
#define HMI_RX_PIN GPIO_NUM_32  // Şemadaki screen_TX (Ekran TX -> ESP RX)
// Nextion seri hızı (8N1 → ham ~960 B/s) — aşağıdaki resync bütçe
// static_assert'ı bunu kullanır. DİKKAT: DisplayHMI::begin() UART config'i
// baud'u ayrıca literal (9600) yazar; orası değişirse bu sabit de AYNI
// commit'te güncellenmeli, yoksa bütçe kanıtı eski hıza göre doğrular.
#define HMI_UART_BAUD 9600

// --- Nextion Reset (brown-out) Algılama ---
// readTouchCommand RX yoluna paralel bağlı dedektör (lib/DisplayHMI/
// NextionResetDetect.h) Nextion Startup event'ini (00 00 00 FF FF FF)
// yakalayınca bkcmd=0 yeniden gönderilir ve ekran cache'leri geçersiz kılınır
// (bkz. DisplayHMI::HMI_handleNextionReset). WARN logu oran-sınırlıdır
// (CAN_RX_STATS_LOG_INTERVAL_MS deseni): en fazla 1 WARN / bu süre — ekran
// güç hattı sürekli brown-out yapıyorsa log spam'i önlenir, toplam sayaç
// logda görünür kalır.
#define HMI_RESET_WARN_LOG_INTERVAL_MS 5000U

// --- HMI Round-Robin Resync (reset dedektörünün emniyet katmanı) ---
// Startup event'i brown-out sırasında RX hattında bozulup KAYBOLABİLİR —
// o durumda yukarıdaki dedektör hiç tetiklenmez ve ekran kalıcı yarı-dolu
// kalırdı. Bu katman olaydan bağımsızdır: her bu aralıkta bir, 11 skalar
// alandan yalnızca SIRADAKİ TEKİ cache'e bakılmaksızın zorla gönderilir
// (saf karar mantığı: lib/DisplayHMI/ResyncPolicy.h::hmi_resync_due_field).
//
// TOPARLANMA SÜRESİ: tam tur = HMI_RESYNC_FIELD_COUNT × bu aralık
// = 11 × 500 ms ≈ 5.5 sn — ekran, tespit edilemeyen bir reset sonrası en
// geç bu süre içinde kendini onarır (insan/gösterge zaman ölçeğinde kabul
// edilebilir; daha agresif değerler UART bütçesinden yer).
#define HMI_RESYNC_INTERVAL_MS 500U

// Tek resync komutunun KÖTÜ-DURUM bayt boyutu (0xFF×3 end-byte DAHİL).
// En uzun komut: 'contactor.txt="CLOSED"' = 22 + 3 = 25 B → marjla 26.
#define HMI_RESYNC_CMD_MAX_BYTES 26U

// BÜTÇE KANITI: 9600 baud 8N1 → ham 960 B/s; HMI_Task 10 Hz → döngü başına
// ~96 B (buildBmsNextionCommands maxBytes=90 bu bütçeden). Resync tetik
// başına TEK alan gönderir → tepe ek yük = 26 B / 500 ms = 52 B/s, ham
// kapasitenin ≤ %10'u (96 B/s) olmalı ki BMS panelinin 90 B/döngü tavanıyla
// birlikte TX ring (256 B) baskı altında kalmasın. Aralık görev periyodunun
// (100 ms) altına indirilse bile hmi_resync_due_field çağrı başına tek alan
// döndürdüğünden fiili tavan 26 B × 10 Hz = 260 B/s'de kendiliğinden doyar;
// aşağıdaki assert normal yapılandırmayı %10 payın içinde tutar.
#ifdef __cplusplus
static_assert((unsigned)HMI_RESYNC_CMD_MAX_BYTES * 1000u /
                      (unsigned)HMI_RESYNC_INTERVAL_MS
                  <= ((unsigned)HMI_UART_BAUD / 10u) / 10u,
              "HMI resync yuku UART butcesinin %10 payini asiyor — "
              "HMI_RESYNC_INTERVAL_MS'i artirin (bkz. ustteki butce kaniti).");
#endif

// --- BMS Panel Round-Robin Resync (24 hücre + özet alanlar) ---
// Skalar resync katmanının 24 hücrelik panele uzantısı: her bu aralıkta bir
// SIRADAKİ TEK slot'un (27 slot: 24 hücre üçlüsü cell/j/bal + cellmax +
// cellmin + warn) BmsNextionCache girdileri geçersiz kılınır (saf yardımcı:
// lib/BmsAlgo/BmsNextionPacket.h::bmsNextionCacheInvalidateSlot); mevcut
// change-compare + maxBytes=90 yolu slot'u yeniden yayar. Invalidasyon
// yapışkan olduğundan bütçe tükenmesinde resync kaybolmaz.
//
// TOPARLANMA SÜRESİ: tam tur = 27 slot × 1000 ms = 27 sn; hücre slotları
// updateCells tikini (1 Hz) beklediğinden en kötü ~+1-2 sn kuyruk → ekranın
// hücre paneli tespit edilemeyen reset sonrası ~29 sn içinde kendini onarır.
// Kritik bilgiler (state/contactor/pack) zaten skalar katmanda ~5.5 sn'de
// toparlanır; detay paneli için daha yavaş tur bilinçli tercihtir (bütçe).
#define BMS_RESYNC_INTERVAL_MS 1000U

// Tek slot'un KÖTÜ-DURUM bayt boyutu (end-byte'lar DAHİL): hücre üçlüsü
// "cell23.val=65535"(19) + "j23.val=100"(14) + "bal23.val=1"(14) = 47 → 48.
#define BMS_RESYNC_SLOT_MAX_BYTES 48U

// BİRLEŞİK BÜTÇE KANITI: iki resync katmanının toplam tepe yükü
//   skalar: 26 B / 500 ms = 52 B/s   +   BMS: 48 B / 1000 ms = 48 B/s
//   = 100 B/s ≤ ham kapasitenin %15'i (960 × 0.15 = 144 B/s).
// BMS tarafı ayrıca buildBmsNextionCommands'ın 90 B/döngü sert tavanından
// geçtiğinden tek döngüde bütçe aşımı zaten mümkün değildir; bu assert
// ortalama yükün de sınırlı kaldığını derleme zamanında kanıtlar.
#ifdef __cplusplus
static_assert((unsigned)HMI_RESYNC_CMD_MAX_BYTES * 1000u /
                      (unsigned)HMI_RESYNC_INTERVAL_MS
                      + (unsigned)BMS_RESYNC_SLOT_MAX_BYTES * 1000u /
                            (unsigned)BMS_RESYNC_INTERVAL_MS
                  <= ((unsigned)HMI_UART_BAUD / 10u) * 15u / 100u,
              "Toplam resync yuku (skalar + BMS panel) UART butcesinin %15 "
              "payini asiyor — resync araliklarindan birini artirin.");
#endif

// --- HMI Command IDs ---
#define HMI_CMD_START 1
#define HMI_CMD_RESET 2
#define HMI_CMD_EMERGENCY_STOP 3
#define HMI_CMD_DRIVE_ENABLE 4

// --- LoRa E22-400T30D-V2 (SX1268, UART & Kontrol) ---
// Pin-uyumlu E32-433T30D yerine geçti; pin atamaları DEĞİŞMEDİ. Config
// protokolü register-tabanlıdır (bkz. E22Regs.h) ve config-modu pin
// seviyeleri E32'den FARKLIDIR (aşağıya bkz.).
#define LORA_UART_NUM UART_NUM_2
#define LORA_TX_PIN GPIO_NUM_16   // ESP TX -> Şemadaki LR_RXD (IO16)
#define LORA_RX_PIN GPIO_NUM_17   // ESP RX <- Şemadaki LR_TXD (IO17)
#define LORA_AUX_PIN GPIO_NUM_35  // IO35 sadece giriş; dahili pull-up YOK — harici 10k pull-up gerekli
#define LORA_M0_PIN GPIO_NUM_25   // Şemadaki MO (IO25)
#define LORA_M1_PIN GPIO_NUM_26   // Şemadaki M1 (IO26)
#define LORA_UART_BAUD 9600       // MCU↔E22 yerel seri hız (config modunda da aynı)
// 2 Hz telemetry uplink (1 Hz'den geri döndürüldü — parkur keşfinde
// maksimum mesafe 500 m ölçüldü, 2.4 kbps hava hızı bu mesafe için aşırı
// tedbirdi; hava hızı 4.8 kbps'e çıkarıldı, ekip onaylı kalibrasyon).
// Gerekçe: ~90 byte'lık bir TEL paketi 4.8 kbps'te ~190 ms havada kalır;
// 500 ms'lik periyotta canlı doluluk ~%38 — UKS'in 1 Hz 0xB0 heartbeat'inin
// kanala girebileceği pencere yeterli kalır.
#define LORA_TX_PERIOD_MS 500
#define LORA_RX_TIMEOUT_MS 20

// G10: Serileşmiş telemetri frame'inin (CSV "TEL,...\r\n", bkz. Telemetry.cpp
// sendStatus) KÖTÜ-DURUM bayt boyutu. Buffer 192 B; alanların maksimum basamak
// genişliğiyle (10 haneli seq/ts, 11 haneli current, vb.) teorik tavan ~112 B —
// güvenli tarafta 120 alınır. Link bütçesi static_assert'ı bunu kullanır.
#define LORA_TEL_FRAME_MAX_BYTES 120
#define LORA_MODE_NORMAL_M0_LEVEL 0
#define LORA_MODE_NORMAL_M1_LEVEL 0
// E22 config modu: M0=0, M1=1 (E32'nin M0=1,M1=1 config modundan FARKLI —
// E22'de M0=1,M1=1 "derin uyku / OTA" moduna karşılık gelir, config değil).
#define LORA_MODE_CONFIG_M0_LEVEL 0
#define LORA_MODE_CONFIG_M1_LEVEL 1
#define LORA_AUX_READY_LEVEL 1
#define LORA_PROTOCOL_VERSION 2

// E22 register sözleşmesi (adresler + değerler) E22Regs.h içindedir —
// UKS e22_regs.h ile birebir senkron tutulmalıdır (bkz. o dosyanın başı).

// --- E22 Config Modu Zaman Aşımları ---
#define LORA_AUX_MODE_TIMEOUT_MS  500   // M0/M1 geçişi sonrası AUX HIGH bekleme (ms)
#define LORA_AUX_CFG_TIMEOUT_MS   2000  // Config yazımı sonrası flash tamamlanma (ms)
#define LORA_CFG_READ_TIMEOUT_MS  500   // C1 sorgu/onay yanıtı bekleme (ms)

// --- MCP23S17 I/O Expander (SPI) → Relays ---
#define RELAY_SPI_HOST SPI2_HOST
#define RELAY_SPI_MOSI GPIO_NUM_23
#define RELAY_SPI_MISO GPIO_NUM_19
#define RELAY_SPI_CLK GPIO_NUM_18
#define RELAY_SPI_CS GPIO_NUM_14

// --- MCP23S17 Relay Channel Assignments ---
// All relay outputs are active-low and currently reserved for the positive
// contactor bank. The software mapping is stable, but the final harness /
// physical load assignment for each channel still needs hardware validation.
// Keep this table synchronized with the wiring document before replacing the
// placeholder descriptions below.
//
// Kanal rol sözlüğü (her kanalın FİZİKSEL işlevi; donanım ekibi harness'i
// netleştirince "role:" etiketleri kesinleştirilecek):
//   MAIN_POSITIVE — ana pozitif kontaktör (HV+ bara)
//   MAIN_NEGATIVE — ana negatif kontaktör (HV- bara)
//   AUX           — yardımcı yük/röle (fan, pompa, ikaz vb.)
// NOT: Bu projede PRECHARGE devresi YOKTUR — precharge rolü TANIMLANMAZ.
// Aşağıdaki "role:" değerleri mevcut "positive contactor bank" niyetine göre
// PROVİZYONEL MAIN_POSITIVE'dir; fiziksel yük "TBD" olduğundan donanım ekibiyle
// netleşince güncellenecektir.
#define RELAY_CH_POS_0 0  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_1 1  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_2 2  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_3 3  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_4 4  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_5 5  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_6 6  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_7 7  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_8 8  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)
#define RELAY_CH_POS_9 9  // role: MAIN_POSITIVE (TBD — donanım ekibiyle netleşince güncellenecek)

#define RELAY_TOTAL_CHANNELS 10

// --- Şartname Bölüm 3, 8.2.a — S1/S2 kontaktör rolleri + uyarı flaşörü ---
// S1 = şarj hattı kontaktörü, S2 = sürüş hattı kontaktörü (şartname 8.2.a):
//   (iii) şarjda  S1 KAPALI + S2 AÇIK
//   (vii) sürüşte S1 AÇIK   + S2 KAPALI
//   (vi)  güvenlik probleminde İKİSİ DE AÇIK
// Flaşör (şartname 6.e.ii): sıcaklık uyarısına bağlı sesli+ışıklı ikaz —
// kontaktör DEĞİLDİR, bank maskesinin DIŞINDA tutulur ki allOff (güvenlik
// açması) uyarı flaşörünü SÖNDÜRMESİN.
//
// Kanal atamaları (yazılım tarafı sabit; fiziksel yük eşlemesi donanım
// ekibi teyidi BEKLİYOR — bkz. RELAY_ROLES_ASSIGNED):
#define RELAY_CH_S2_DRIVE 0   // S2 — sürüş hattı kontaktörü (kanal 1-7 sürüş bankının parçası)
#define RELAY_CH_S1_CHARGE 8  // S1 — şarj hattı kontaktörü
#define RELAY_CH_FLASHER 9    // Uyarı flaşörü (sesli+ışıklı, şartname 6.e.ii)

// Kanal-yük eşlemesi (yukarıdaki S1/S2/flaşör rolleri) donanım ekibiyle
// HENÜZ teyit edilmedi. 0 (varsayılan) iken rol mantığının TAMAMI (flaşör,
// S1/S2 mod anahtarlaması) derleme dışıdır ve araç davranışı eski "tek bank"
// haliyle BAYT-BAYT aynı kalır. Teyit gelince build flag'i ile 1 yapılır
// (ör. platformio.ini -D RELAY_ROLES_ASSIGNED=1).
#ifndef RELAY_ROLES_ASSIGNED
#define RELAY_ROLES_ASSIGNED 0
#endif

#if !RELAY_ROLES_ASSIGNED
#warning "kanal-yük eşlemesi donanım ekibiyle teyit edilmedi — bank davranışı eski haliyle sürüyor"
// Roller atanmamışken maske 10 kanalın TAMAMI: allOn/allOff bugünkü davranışla
// birebir aynı kalır (flaşör kanalı diye bir ayrım henüz YOK).
#define RELAY_CONTACTOR_BANK_MASK ((1u << RELAY_TOTAL_CHANNELS) - 1u)  // 0x3FF
#else
// Roller atandı: kontaktör bankı = flaşör HARİÇ tüm kanallar (S1 + S2 +
// sürüş bankı 1-7). allOff bu maskeyi açar → şartname 8.2.a.vi (güvenlik
// probleminde S1 ve S2 dahil hepsi açılır) sağlanır, flaşör kanalının son
// yazılan durumu shadow'da KORUNUR (uyarı sönmez).
#define RELAY_CONTACTOR_BANK_MASK \
    (((1u << RELAY_TOTAL_CHANNELS) - 1u) & ~(1u << RELAY_CH_FLASHER))  // 0x1FF
// Sürüş hattı bankı = S2 (kanal 0) + kanal 1-7. S1 bu maskenin BİLİNÇLİ
// olarak DIŞINDADIR: READY girişi yalnız bu maskeyi kapatır, S1 açık kalır
// (şartname 8.2.a.vii). S1, RELAY_CONTACTOR_BANK_MASK'ın İÇİNDEDİR ki
// allOff güvenlik açması S1'i de açsın (8.2.a.vi).
#define RELAY_DRIVE_BANK_MASK \
    (RELAY_CONTACTOR_BANK_MASK & ~(1u << RELAY_CH_S1_CHARGE))  // 0x0FF
#ifdef __cplusplus
static_assert((RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_FLASHER)) == 0,
              "Flasor kanali kontaktor bank maskesinin DISINDA olmali — "
              "allOff uyari flasorunu sondurmemeli (sartname 6.e.ii).");
static_assert((RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_S1_CHARGE)) != 0 &&
                  (RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_S2_DRIVE)) != 0,
              "S1 ve S2 kontaktor bank maskesinin ICINDE olmali — guvenlik "
              "acmasi (allOff) ikisini de acmali (sartname 8.2.a.vi).");
#endif
#endif  // RELAY_ROLES_ASSIGNED

// Flaşör histerezisi (şartname 6.e.ii/6.e.iii): flaşör 55 °C'de (BMS_WARN_
// MAX_TEMP_C, >= semantiği) yanar; sıcaklık (55 − bu değer) = 53 °C'nin
// ALTINA inince söner. Eşik sınırında titremeyi (ON/OFF çırpınması) önler.
// Yalnız RELAY_ROLES_ASSIGNED=1 iken derlenen flaşör mantığı kullanır.
#define FLASHER_HYSTERESIS_C 2

// --- MCP23S17 Çıkış Doğrulama (Actuator Verify) Periyodu ---
// VCU task'i 20 ms'de bir (50 Hz) döner; OLAT/IODIR geri-okuma doğrulamasını
// HER tick yapmak SPI bara yükünü gereksiz artırır (tick başına 4 register
// read). 100 ms (10 Hz) periyot, MCP23S17 brown-out/reset ile default'a
// (tüm pinler input → röle sürücüleri floating) dönmesini kontaktör/insan
// zaman ölçeğine göre yeterince hızlı yakalar; yazma noktaları (allOn/allOff/
// setRelay/begin) zaten HEMEN doğrulandığından bu periyodik tarama yalnızca
// yazma OLMADAN oluşan sessiz reset'leri yakalamak içindir.
#define RELAY_VERIFY_PERIOD_MS 100U

// --- UKS LoRa Heartbeat Byte ---
// 9.2.a: RF hatti tek yonlu telemetri + heartbeat'tir; UKS->AKS komut
// kanali (eski 0xA1-0xA4) sistemden tamamen kaldirildi.
#define UKS_HEARTBEAT_BYTE     0xB0   // UKS ~1 Hz periyodik heartbeat (stabilizasyon teyidi)

// --- LoRa Link Monitörü ---
// 9 sn: tek frekansli yarim-dubleks E22 kanalinda UKS'in 1 Hz 0xB0
// heartbeat'i, AKS'in kendi telemetri TX'i ile kanali paylastigindan HER
// ZAMAN 1 Hz ulasamiyor — saha loglarinda gozlenen fiili heartbeat araligi
// ~5-6 sn idi (eski LINK_TIMEOUT_MS=3000 bu araliktan kisa oldugu icin link
// surekli DOWN->UP flapping yapiyordu, bkz. LoRa_Link_Analysis.md). 9 sn,
// gozlenen ~5-6 sn'lik araliga rahat marj birakir; LORA_TX_PERIOD_MS'in
// 500'e dusurulmesiyle birlikte heartbeat'in kanala girme sansi da artar.
#define LINK_TIMEOUT_MS        9000U

// Boot anindan itibaren bu sure icinde HIC heartbeat gelmediyse link DOWN
// kabul edilir (9.2.e / 9.4.b.vi): arac acildiginda UKS hic yayinda
// degilse AKS'in sonsuza dek "link UP" varsayip o donemin verisini
// kaybetmesini onler (bkz. link_check_timeout_with_boot_grace).
#define BOOT_LINK_GRACE_MS     5000U

// --- Offline Buffer Örnekleme ve Replay (9.2.e / 9.2.h / 9.4.b.vi) ---
// Kesinti sirasinda buffer'a yazilan ornekleme periyodu. 9.2.h izleme
// merkezi kayitlari arasi en fazla 5 sn kuralina 5x marjla uyar ve
// OB_CAPACITY / replay suresini 5'e boler (60 sn'lik kesinti icin 300
// yerine 60-75 paket).
#define OFFLINE_SAMPLE_PERIOD_MS 1000U

// Link UP oldugunda tek LORA_TX_PERIOD_MS tikinde en fazla bu kadar
// buffered (replay) paket gonderilir; canli paket akisi hic kesilmeden
// (1 canli + en fazla bu kadar replay / tik) buffer bosaltilir (S1).
#define REPLAY_BURST_PER_TICK  1

// --- G10: LoRa Link Bütçesi (frame boyutu × oran ≤ UART kapasitesi) ---
// Tek TX tikinde (her LORA_TX_PERIOD_MS) en fazla (1 canlı + REPLAY_BURST_PER_TICK
// replay) frame gönderilir. TEPE bayt/sn:
//     tepe = (1 + REPLAY_BURST_PER_TICK) × LORA_TEL_FRAME_MAX_BYTES × 1000
//            / LORA_TX_PERIOD_MS
// UART hattı 8N1 → 10 bit/byte → ham kapasite = LORA_UART_BAUD / 10 [B/s].
// Heartbeat'in kanala girebilmesi + jitter için %20 EMNİYET PAYI → × 0.8.
//     KURAL:  tepe ≤ (LORA_UART_BAUD / 10) × 0.8
// Frame boyutu ve baud UKS sözleşmesidir — DEĞİŞTİRİLEMEZ; bütçe yalnız
// LORA_TX_PERIOD_MS / REPLAY_BURST_PER_TICK ile ayarlanır. Mevcut değerler
// (2 Hz, 1 replay + 1 canlı, 120 B): tepe = 2×120×1000/500 = 480 B/s ≤ 768 B/s.
// (Not: LORA_TX_PERIOD_MS 200'e — 5 Hz — düşürülürse tepe 1200 B/s olur ve
//  aşağıdaki static_assert derlemeyi KIRAR; bu kasıtlı bir emniyettir. Bu
//  bütçe, MCU<->E22 yerel UART hattının (LORA_UART_BAUD, 9600, DEĞİŞMEDİ)
//  kapasitesini denetler — 4.8 kbps hava hızı ayrı bir darboğazdır ve bu
//  static_assert'in kapsamında DEĞİLDİR.)
#ifdef __cplusplus
static_assert(
    (1u + (unsigned)REPLAY_BURST_PER_TICK) * (unsigned)LORA_TEL_FRAME_MAX_BYTES *
            1000u / (unsigned)LORA_TX_PERIOD_MS
        <= (unsigned)LORA_UART_BAUD * 8u / 100u,  // = baud/10 × 0.8
    "G10: LoRa link butcesi asildi — LORA_TX_PERIOD_MS / REPLAY_BURST_PER_TICK / "
    "LORA_TEL_FRAME_MAX_BYTES uclusu UART kapasitesini (baud/10 x 0.8) asiyor. "
    "Frame boyutu/baud DEGISTIRME (UKS sozlesmesi); periyodu artir veya replay "
    "oranini dusur.");
#endif

// --- G11: LoRa UART init retry emniyeti ---
// uart_driver_install bu kadar denemede kurulamazsa retry döngüsü SONSUZ
// beklemez; telemetri "devre dışı" moduna geçer (araç durmaz — bkz. main.cpp
// EspLoraHal::begin / vTask_LoRa_UKS + lib/LoraLink/UartInitRetry.h).
#define LORA_UART_MAX_INIT_ATTEMPTS 5

// --- G11-b: LoRa görev-başı kurulumu KALICI devre dışı kalmasın ---
// EspLoraHal::begin() (yukarıdaki LORA_UART_MAX_INIT_ATTEMPTS denemesi)
// başarısız olup "devre dışı" moduna geçtikten SONRA vTask_LoRa_UKS artık
// SONSUZA DEK boş döngüde kalmaz — bu sabit aralıkla begin()'i (+ E22
// config'i) yeniden dener; geçici bir UART/donanım aksaklığı reboot
// beklemeden kendi kendine düzelebilir (araç bu süre boyunca zaten
// etkilenmiyordu — bkz. Documents/LoRa_Link_Analysis.md "Current
// Reliability Policy"). Watchdog retry beklemesi sırasında da beslenir.
#define LORA_INIT_RETRY_INTERVAL_MS 30000U

// --- LoRa RX Tanısı ---
#define LORA_UNKNOWN_BYTE_WARN_INTERVAL_MS 10000U  // RF gurultu tanisi icin en fazla 1 WARN / 10 sn

// --- FreeRTOS Task Öncelikleri (M6) ---
// Yüksek sayı = yüksek öncelik. GÜVENLİK SIRALAMASI: VCU (durum makinesi/
// güvenlik) > CAN (güvenlik-kritik alım: BMS/motor freshness, kontaktör) >
// LoRa (yalnızca telemetri) > HMI (ekran). Telemetri (LoRa) güvenlik-kritik
// CAN alımını ASLA preempt etmemeli — bu yüzden CAN > LoRa.
// GERİ ALMA: LoRa öncelik düşüşü heartbeat zamanlamasını bozarsa (kaçırma),
// TASK_PRIORITY_LORA'yı CAN'in ÜSTÜNE çıkarmak yerine önce LoRa periyodunu/
// stack'ini gözden geçir; sabitler burada olduğundan tek satırda geri alınır.
#define TASK_PRIORITY_VCU  10  // en yüksek — güvenlik durum makinesi
#define TASK_PRIORITY_CAN   8  // güvenlik-kritik CAN alımı (telemetriden yüksek)
#define TASK_PRIORITY_LORA  5  // telemetri uplink (CAN'in altında)
#define TASK_PRIORITY_HMI   2  // ekran güncelleme (en düşük)

// --- Motor Sürücüsü Entegrasyon Bayrağı ---
#ifndef MOTOR_DRIVER_PRESENT
#define MOTOR_DRIVER_PRESENT 0  // Motor sürücüsü entegre edildiğinde 1 yap — READY interlock'u ve zero-torque yolu bu bayrağa bağlı.
#endif

// --- HİPOTEZ: Akımdan türetilmiş sysState (bkz. Documents/CAN_Message_Table.md
// "0x0000E003" ve lib/Telemetry/SysStateDerive.h) ---
// UKS `sysState` alanı (TEL alan 12) hiçbir CAN ID'den DOĞRULANMIŞ parse
// almıyor (bkz. Documents/UKS_LoRa_Protocol.md "DOĞRULANACAK") —
// TelemetrySanitize::sanitizeSystemState(0) bunu FAULT(4) yapar, UKS
// ekranında BMS her zaman FAULT görünür. Bu bayrak (varsayılan KAPALI),
// DOĞRULANMIŞ akım sinyalinden (0xE000 byte[0:1]) basit bir Discharge/IDLE/
// Charge tahmini üretmeyi dener — E003 byte[0:1]'in gerçek sysState olup
// olmadığı henüz TEYİT EDİLMEDİ (⚠️ HİPOTEZ, bkz. CAN_Message_Table.md).
// EK B GÜVEN KURALI gereği bu türetilmiş değer YALNIZCA UKS telemetri
// gösterimi içindir — VCU karar mantığına (FAULT/kontaktör) ASLA BAĞLANMAZ
// (bkz. SysStateDerive.h "applyIfEnabled" çağrı noktası: yalnız LoRa TX
// paketleme yolunda, VcuLogic'in okuduğu TelemetryData'ya DOKUNMAZ).
#ifndef SYSSTATE_DERIVE_FROM_CURRENT
#define SYSSTATE_DERIVE_FROM_CURRENT 0
#endif

// CONFIG — akımın "IDLE" sayılacağı simetrik bant (centi-Amper, TEL_bmsCurrentCentiA
// ile aynı birim). Öneri: 50 (=0.5 A) — 0xE000 byte[0:1] çözünürlüğü 0.1 A
// (bkz. CAN_Message_Table.md) olduğundan birkaç LSB'lik ölçüm gürültüsüne
// karşı marj bırakır. Saha kalibrasyonu/ekip onayı BEKLİYOR.
#define SYSSTATE_CURRENT_IDLE_BAND_CENTI_A 50

// --- Motor Error-Flag Debounce (G9) ---
// Motor errorFlags → FAULT yolu, geçici/tek-seferlik hata bitine (ör. CAN CRC
// bit hatası) süratle kontaktör açtırmasın diye N ARDIŞIK frame onayı ister.
// Sayaç, temiz (errorFlags==0) frame gelince sıfırlanır. 2-3 ardışık frame
// önerilir (parazit filtreleme ile gerçek fault gecikmesi arası denge).
// Bkz. lib/CanManager/MotorFaultDebounce.h (saf, bayraktan bağımsız).
#define MOTOR_ERROR_DEBOUNCE_FRAMES 3

// --- Phase 1 Planning Notes ---
// Torque command generation is intentionally held at zero until the pedal /
// brake input model is finalized. READY -> DRIVE enable is now command-driven,
// but propulsion stays inhibited until the torque mapping rules are defined.
//
// E-STOP / FAULT güvenli kapanış sırası (VcuLogic handleEmergencyStop/
// handleFault) ARTIK kurulu: 1) sendTorqueCommand(0) 2) VCU_CONTACTOR_OPEN_
// DELAY_MS bekle 3) kontaktörleri aç. MOTOR_DRIVER_PRESENT=0 iken (1) gerçek
// frame göndermez (bkz. CanManager::sendTorqueCommand) — bkz. G2 riski,
// Documents/MOTOR_ENTEGRASYON_NOTU.md.
// 20 ms sembolik; motor sürücüsü entegrasyonunda gerçek tork sönüm süresine
// göre kalibre edilecek (motor RPM/akım düşüşü doğrulanmadan sahaya çıkma).
#define VCU_CONTACTOR_OPEN_DELAY_MS 20

// --- Phase 2 Safety Thresholds ---
// Warning levels should eventually trigger derating (AÇIK İŞ B12 — İSKELET
// KURULDU 2026-07-15: lib/VcuLogic/DeratingPolicy.h WARN sinyallerinden
// 0..100 bir tork-izin yüzdesi hesaplıyor ve VcuLogic.cpp run() bunu yalnız
// LOGLUYOR ("derating önerisi %N" — bkz. aşağıdaki DERATING_* sabitleri).
// KALAN AÇIK İŞ: (1) bu yüzdeleri gerçek bir tork komutuna bağlamak —
// motor sürücüsü tork komut yolu (setTorqueSink/G2) gerçek frame üretmeye
// başlayınca tasarlanacak; (2) DERATING_* yüzdelerinin/eşik-yaklaşma
// oranının saha kalibrasyonu/ekip onayı. Bugün araç davranışı DEĞİŞMEZ.
// Critical levels should force a transition to FAULT.
//
// EK B GÜVEN KURALI: Güvenlik kararı (FAULT/kontaktör) yalnızca DOĞRULANMIŞ
// sinyallerden türetilir. Şu an DOĞRULANMIŞ olanlar: pack voltajı (0xE000
// byte[2:3]), akım (0xE000 byte[0:1] + saha gözlemi), SoC (0xE000 byte[4:5]),
// en yüksek hücre sıcaklığı (0xE001 byte[6:7]), 24 hücre voltajı (E015-E020)
// + BMS freshness (G12: E000/E001 ID bazında, hücre voltajı ise
// CAN_cellVoltageSeenMask ile ayrı izlenir). Hücre voltajı eşikleri artık
// karar mantığına BAĞLI: VcuLogic::hasWarningCondition/hasCriticalCondition
// (bkz. VcuLogic.h) BmsAlgo.h'deki BMS_CELL_UNDERVOLT/OVERVOLT_WARN/CRIT_
// DECI_MV eşiklerini (deci-mV — TEL_bmsCellVoltageMin/MaxDeciMv alanıyla AYNI
// ölçek; GÜVENLİK-EŞİĞİ DÜZELTMESİ 2026-07-13, önceden mV-ölçekli makrolarla
// karşılaştırılıyordu, bkz. Documents/Threshold_Ownership.md) TEL_
// cellVoltageDataValid iken kullanır. AÇIK İŞ: yalnızca TEL_bmsSystemState
// kaynağı henüz doğrulanmadığı için ilgili kontrol (==4 FAULT) YER
// TUTUCUDUR ve karar mantığına fiilen BAĞLI DEĞİLDİR.

// Pack voltage thresholds in decivolts (1 deciV = 0.1 V).
// Kaynak alan: Lithium Balance c-BMS 0xE000 byte[2:3], big-endian uint16,
// raw * 0.1 = V — DOĞRULANDI (2 sniffer oturumu). KARAR MANTIĞINA BAĞLI.
//
// Paket: 24S LiFePO4. Referans aralık (paket etiketi/spec):
//   min 60.0 V (2.50 V/hücre), nominal 76.8 V (3.20 V/hücre),
//   maks 87.6 V (3.65 V/hücre)
// CRITICAL eşikleri doğrudan spec uçlarından; WARN eşikleri hücre başına
// 3.00 V / 3.55 V'den türetildi. CONFIG — gerçek saha kalibrasyonu bekleniyor;
// nihai değerler ekip/danışman onayı ile güncellenmeli.
#define BMS_WARN_MIN_PACK_VOLTAGE_DECI_V 720      // 72.0 V (3.00 V/hücre)
#define BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V 600  // 60.0 V (2.50 V/hücre — spec min)
#define BMS_WARN_MAX_PACK_VOLTAGE_DECI_V 852      // 85.2 V (3.55 V/hücre)
#define BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V 876  // 87.6 V (3.65 V/hücre — spec maks)

// Sıcaklık eşikleri — kaynak sinyal DOĞRULANDI (0xE001 byte[6:7],
// max(temp1,temp2)) ve TEL_bmsTempHighestC'ye parse ediliyor. KARAR
// MANTIĞINA BAĞLI: VcuLogic::isTempWarning/isTempCritical (>= semantiği)
// hasWarningCondition/hasCriticalCondition içinden çağrılır — 55 °C ve
// üzeri UYARI, 70 °C ve üzeri FAULT (sistem kendini kapatır). Kritik
// sıcaklık isReadyEntryPermitted üzerinden READY girişini de bloklar.
// HMI katmanı (BmsAlgo.h BMS_TEMP_OVERTEMP_WARN_C/CRIT_C) aynı 55/70
// değerlerine hizalıdır.
//
// Şartname Bölüm 3, 6.e.iii: 55 uyarı / 70 kapanma, 15°C sabit aralık.
// Bu iki değer şartname idealinin BİREBİR kendisidir — DEĞİŞTİRİLMEZ;
// 15 °C'lik uyarı-kapanma aralığı aşağıdaki static_assert ile derleme
// zamanında kilitlidir (BmsAlgo.h HMI eşikleriyle eşitlik kilidi de
// VcuLogic.h'dedir — iki başlığı birden gören ilk karar katmanı orasıdır).
#define BMS_WARN_MAX_TEMP_C 55
#define BMS_CRITICAL_MAX_TEMP_C 70
#ifdef __cplusplus
static_assert(BMS_CRITICAL_MAX_TEMP_C - BMS_WARN_MAX_TEMP_C == 15,
              "Sartname Bolum 3, 6.e.iii: uyari (55) ile kapanma (70) arasinda "
              "15 C sabit aralik korunmali — esiklerden biri tek tarafli "
              "degistirilemez.");
#endif
// Current thresholds in centi-Ampere (0.01 A units) — parser çıktısı
// TEL_bmsCurrentCentiA ile AYNI birim (raw 0.1A × 10 = centi-A). Böylece
// eşikler parser ölçeğiyle uçtan uca hizalı; aşırı akım koruması gerçek
// değerlerde tetiklenebilir (G5 düzeltmesi — eski centi-mA yorumu 1000× kör
// bırakıyordu).
//
// Kaynak sinyal DOĞRULANDI (0xE000 byte[0:1] + saha gözlemi, Temmuz 2026):
// şarjda +9.9 A, deşarjda gaza bağlı −0.1…−1.5 A gözlendi; işaret
// konvansiyonu + şarj / − deşarj (BmsModel.h ile uyumlu). KARAR MANTIĞINA
// BAĞLI: VcuLogic::isCurrentWarning/isCurrentCritical (>= semantiği)
// hasWarningCondition/hasCriticalCondition içinden çağrılır.
//
// CONFIG — şarj eşikleri saha verisine göre kalibre edildi: 9.9 A nominal
// şarj akımının üstünde marjla WARN 11 A / CRITICAL 13 A önerildi. Nihai
// değerler BMS/şarj cihazı spec'iyle EKİP ONAYI BEKLİYOR. Deşarj eşikleri
// (9/15 A) saha gözlemindeki −1.5 A tepe değerin çok üstünde; onlar da aynı
// onay turunda gözden geçirilmeli.
#define BMS_WARN_MAX_CHARGE_CURRENT_CENTI_A       1100  // 11.0 A
#define BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A   1300  // 13.0 A
#define BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_A    900   // 9.0 A
#define BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A 1500 // 15.0 A
// Hücre voltajı eşikleri (mV) — 24S LiFePO4 spec'inden türetildi
// (2.50 V / 3.65 V per hücre). TEL_bmsCellVoltageMin/MaxDeciMv DOĞRULANDI ve
// parse ediliyor (0xE001 byte[0:1]/byte[2:3], bkz. CanParse::parseLbBmsE001)
// ve karar mantığına BAĞLI — fiilen kullanılan eşik seti burada DEĞİL,
// BmsAlgo.h'de: BMS_CELL_UNDERVOLT_CRIT_MV/BMS_CELL_OVERVOLT_CRIT_MV
// (VcuLogic::hasCriticalCondition tarafından çağrılır). Bu dosyada AYNI
// değerlerle kullanılmayan bir kopya makro seti (BMS_CRITICAL_MIN/MAX_CELL_
// VOLTAGE_MV) vardı — 2026-07-13'te grep ile hiçbir referans bulunmadığı
// doğrulanıp SİLİNDİ. Hücre voltajı CRITICAL eşiğini değiştirecekseniz
// BmsAlgo.h'yi güncelleyin (tek doğruluk kaynağı).

// --- B12: Derating Policy (İSKELET — bkz. lib/VcuLogic/DeratingPolicy.h) ---
// WARN bandında (hasWarningCondition==true) 0..100 bir tork-izin yüzdesi
// hesaplanır; ŞU AN yalnızca run() içinde LOGLANIR, gerçek bir tork komutuna
// BAĞLANMAZ (motor sürücüsü tork yolu hazır olunca, bkz. G2/MOTOR_ENTEGRASYON_
// NOTU.md). Basit 3 kademeli harita — ekip kalibrasyonu BEKLİYOR (CONFIG):
//   WARN yok            -> DERATING_TORQUE_PERCENT_NOMINAL
//   WARN aktif          -> DERATING_TORQUE_PERCENT_WARNING
//   CRITICAL'e yaklaşma  -> DERATING_TORQUE_PERCENT_APPROACHING_CRITICAL
// "Yaklaşma" WARN->CRITICAL aralığının DERATING_APPROACHING_CRITICAL_
// FRACTION_PERCENT kadarının tüketilmesi olarak tanımlanır (bkz.
// DeratingPolicy.h yorumu — ham eşik değerinin doğrudan yüzdesi DEĞİL: bu,
// sıfırdan uzak mutlak eşiklerde (ör. pack aşırı gerilim 852/876 deciV)
// ters sonuç verirdi, WARN->CRITICAL ARALIĞININ yüzdesi fiziksel olarak
// anlamlıdır).
#define DERATING_TORQUE_PERCENT_NOMINAL 100
#define DERATING_TORQUE_PERCENT_WARNING 50
#define DERATING_TORQUE_PERCENT_APPROACHING_CRITICAL 20
#define DERATING_APPROACHING_CRITICAL_FRACTION_PERCENT 90

// Task watchdog timing is still using the ESP-IDF default configuration.
// The shorter LoRa RX timeout below improves scheduling margin, but the global
// watchdog timeout should still be reviewed once final task runtimes stabilize.

// --- CAN Freshness Thresholds ---
#define CAN_MOTOR_STATUS_TIMEOUT_MS 1500
#define CAN_BMS_STATUS_TIMEOUT_MS   500
#define CAN_CELL_VOLTAGE_TIMEOUT_MS 500  // E015-E020 grubu
// Charger komut frame'i (0x1806E5F4) OPSİYONEL bir akıştır: araç sürüşteyken
// charger bağlı olmayabilir. Timeout yalnızca saklanan setpoint'leri "bayat"
// işaretler; CAN_Event/FAULT ÜRETMEZ (krş. motor timeout -> FAULT).
#define CAN_CHARGER_TIMEOUT_MS      2000

// UKS'in aralik-disi alan sanitizasyonu (yalnizca vTask_LoRa_UKS icindeki uplink asamasinda yapilir)
// tetiklendiginde ayni durum tekrar tekrar olussa bile log spam'ini
// onlemek icin alan basina en fazla 1 WARN / bu sure.
#define TEL_SANITIZE_WARN_THROTTLE_MS 10000

#endif  // SYSTEM_CONFIG_H
