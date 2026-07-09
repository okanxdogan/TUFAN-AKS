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
#define CAN_ID_LB_BMS_E001 0x0000E001  // DOĞRULANDI: Temp1 & Temp2 (byte[6:7]); byte[0:5] BİLİNMİYOR
#define CAN_ID_LB_BMS_E002 0x0000E002  // BİLİNMİYOR — alan anlamı çözülmedi
#define CAN_ID_LB_BMS_E003 0x0000E003  // BİLİNMİYOR — alan anlamı çözülmedi
#define CAN_ID_LB_BMS_E004 0x0000E004  // BİLİNMİYOR — alan anlamı çözülmedi
#define CAN_ID_LB_BMS_E005 0x0000E005  // BİLİNMİYOR — alan anlamı çözülmedi
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

// --- Nextion HMI (UART) ---
#define HMI_UART_NUM UART_NUM_1
// Not: J8 konnektöründe screen_TX'in ekranın mı yoksa ESP'nin mi TX'i olduğuna
// göre aşağıdaki 32 ve 33 yer değiştirebilir, ancak donanım pinleri 32 ve
// 33'tür.
#define HMI_TX_PIN GPIO_NUM_33  // Şemadaki screen_RX (ESP TX -> Ekran RX)
#define HMI_RX_PIN GPIO_NUM_32  // Şemadaki screen_TX (Ekran TX -> ESP RX)

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
// 2 Hz telemetry uplink (5 Hz'den düşürüldü — saha loglarında tespit edilen
// link flapping düzeltmesinin parçası). Tek frekanslı yarım-dubleks E22
// kanalında 5 Hz sürekli AKS TX'i, UKS'in 1 Hz 0xB0 heartbeat'inin kanala
// girebileceği boşluğu neredeyse hiç bırakmıyordu; heartbeat AKS'e ancak
// ~5-6 sn'de bir ulaşıyor, LINK_TIMEOUT_MS bu araliktan kisa oldugundan link
// surekli DOWN->UP flapping yapiyordu. 2 Hz, kanalda heartbeat'in gecebilecegi
// duzenli bosluklar yaratir.
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
//  aşağıdaki static_assert derlemeyi KIRAR; bu kasıtlı bir emniyettir.)
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
// Warning levels should eventually trigger derating.
// Critical levels should force a transition to FAULT.
//
// EK B GÜVEN KURALI: Güvenlik kararı (FAULT/kontaktör) yalnızca DOĞRULANMIŞ
// sinyallerden türetilir. Şu an DOĞRULANMIŞ olanlar: pack voltajı (0xE000
// byte[2:3]) + BMS freshness (E000 timeout). Akım/sıcaklık/hücre voltajı/SOC
// kaynak sinyalleri henüz doğrulanmadığı için aşağıdaki ilgili eşikler YER
// TUTUCUDUR ve karar mantığına BAĞLI DEĞİLDİR.

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

// Sıcaklık eşikleri — kaynak sinyal DOĞRULANDI (0xE001 byte[6:7]) ve
// TEL_bmsTempHighestC'ye parse ediliyor. ANCAK bu eşikler henüz
// VcuLogic karar mantığına BAĞLANMAMIŞTIR (hasWarningCondition /
// hasCriticalCondition içinde temp kontrolü yok). Bağlanana kadar
// YER TUTUCUDUR.
#define BMS_WARN_MAX_TEMP_C 55
#define BMS_CRITICAL_MAX_TEMP_C 70
// Current thresholds in centi-Ampere (0.01 A units) — parser çıktısı
// TEL_bmsCurrentCentiA ile AYNI birim (raw 0.1A × 10 = centi-A). Böylece
// eşikler parser ölçeğiyle uçtan uca hizalı; aşırı akım koruması gerçek
// değerlerde tetiklenebilir (G5 düzeltmesi — eski centi-mA yorumu 1000× kör
// bırakıyordu).
// Not: CAN_BMS_CURRENT akımı doğrulanmıştır ve TEL_bmsCurrentCentiA
// üzerinden erişilebilir.
#define BMS_WARN_MAX_CHARGE_CURRENT_CENTI_A       90    // 0.9 A
#define BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A   100   // 1.0 A
#define BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_A    900   // 9.0 A
#define BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A 1500 // 15.0 A
// Hücre voltajı eşikleri (mV) — 24S LiFePO4 spec'inden türetildi
// (2.50 V / 3.65 V per hücre).
// Kaynak sinyal BİLİNMİYOR — TEL_bmsCellVoltageMin/MaxDeciMv hiçbir
// CAN ID'den parse edilmiyor. BAĞLANMAMALI.
#define BMS_CRITICAL_MIN_CELL_VOLTAGE_MV 2500
#define BMS_CRITICAL_MAX_CELL_VOLTAGE_MV 3650

// Task watchdog timing is still using the ESP-IDF default configuration.
// The shorter LoRa RX timeout below improves scheduling margin, but the global
// watchdog timeout should still be reviewed once final task runtimes stabilize.

// --- CAN Freshness Thresholds ---
#define CAN_MOTOR_STATUS_TIMEOUT_MS 1500
#define CAN_BMS_STATUS_TIMEOUT_MS   500
// Charger komut frame'i (0x1806E5F4) OPSİYONEL bir akıştır: araç sürüşteyken
// charger bağlı olmayabilir. Timeout yalnızca saklanan setpoint'leri "bayat"
// işaretler; CAN_Event/FAULT ÜRETMEZ (krş. motor timeout -> FAULT).
#define CAN_CHARGER_TIMEOUT_MS      2000

// UKS'in aralik-disi alan sanitizasyonu (yalnizca vTask_LoRa_UKS icindeki uplink asamasinda yapilir)
// tetiklendiginde ayni durum tekrar tekrar olussa bile log spam'ini
// onlemek icin alan basina en fazla 1 WARN / bu sure.
#define TEL_SANITIZE_WARN_THROTTLE_MS 10000

#endif  // SYSTEM_CONFIG_H
