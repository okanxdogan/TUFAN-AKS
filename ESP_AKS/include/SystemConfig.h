#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// SystemConfig.h
// Centralized configuration for pin assignments, timeouts, and other constants
// Used across multiple modules for consistency

// --- Includes ---
#include "E22Regs.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"


// --- CAN Message IDs ---
// #define CAN_ID_TORQUE_CMD 0x100    // AKS → Motor Driver
#define CAN_ID_MOTOR_STATUS                                                    \
  0x123 // Motor Driver → AKS (MSTest/mock_motor ile doğrulandı, 11-bit STD)
// Veri alma konusunda hata gelirse ID ile alakalı olabilir
// Lithium Balance c-BMS — 29-bit Extended ID, Big Endian
// Gerçek CAN sniffer loglarından doğrulanmış ID'ler.
// SADECE 0xE000 byte[2:3] = packV alanı reverse-engineer ile çözüldü.
// Diğer ID'lerin alan anlamları henüz DOĞRULANMADI.
#define CAN_ID_LB_BMS_E000                                                     \
  0x0000E000 // Pack voltage (ÇÖZÜLDÜ: byte[2:3] = packV deciV)
#define CAN_ID_LB_BMS_E001 0x0000E001 // TODO: alan anlamı doğrulanmadı
#define CAN_ID_LB_BMS_E002 0x0000E002 // TODO: alan anlamı doğrulanmadı
#define CAN_ID_LB_BMS_E003 0x0000E003 // TODO: alan anlamı doğrulanmadı
#define CAN_ID_LB_BMS_E004 0x0000E004 // TODO: alan anlamı doğrulanmadı
#define CAN_ID_LB_BMS_E005 0x0000E005 // TODO: alan anlamı doğrulanmadı
#define CAN_ID_LB_BMS_E032 0x0000E032 // TODO: alan anlamı doğrulanmadı
#define CAN_ID_LB_BMS_E033 0x0000E033 // TODO: alan anlamı doğrulanmadı

// CAN sniffer loglarında ara sıra görülen 11-bit standart frame.
// Tüm byte'ları sıfır, anlamı bilinmiyor. Şu an işlenmiyor.
#define CAN_ID_LB_STD_0x000 0x000 // STD 11-bit — TODO: alan anlamı doğrulanmadı

// --- CAN (TJA1050 transceiver) ---
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// --- Nextion HMI (UART) ---
#define HMI_UART_NUM UART_NUM_1
// Not: J8 konnektöründe screen_TX'in ekranın mı yoksa ESP'nin mi TX'i olduğuna
// göre aşağıdaki 32 ve 33 yer değiştirebilir, ancak donanım pinleri 32 ve
// 33'tür.
#define HMI_TX_PIN GPIO_NUM_33 // Şemadaki screen_RX (ESP TX -> Ekran RX)
#define HMI_RX_PIN GPIO_NUM_32 // Şemadaki screen_TX (Ekran TX -> ESP RX)

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
#define LORA_TX_PIN GPIO_NUM_16 // ESP TX -> Şemadaki LR_RXD (IO16)
#define LORA_RX_PIN GPIO_NUM_17 // ESP RX <- Şemadaki LR_TXD (IO17)
#define LORA_AUX_PIN                                                           \
  GPIO_NUM_35 // IO35 sadece giriş; dahili pull-up YOK — harici 10k pull-up
              // gerekli
#define LORA_M0_PIN GPIO_NUM_25 // Şemadaki MO (IO25)
#define LORA_M1_PIN GPIO_NUM_26 // Şemadaki M1 (IO26)
#define LORA_UART_BAUD 9600   // MCU↔E22 yerel seri hız (config modunda da aynı)
#define LORA_TX_PERIOD_MS 200 // 5 Hz telemetry uplink
#define LORA_RX_TIMEOUT_MS 20
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
#define LORA_AUX_MODE_TIMEOUT_MS                                               \
  500 // M0/M1 geçişi sonrası AUX HIGH bekleme (ms)
#define LORA_AUX_CFG_TIMEOUT_MS                                                \
  2000 // Config yazımı sonrası flash tamamlanma (ms)
#define LORA_CFG_READ_TIMEOUT_MS 500 // C1 sorgu/onay yanıtı bekleme (ms)

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
#define RELAY_CH_POS_0 0 // Positive contactor output 0 (physical load TBD)
#define RELAY_CH_POS_1 1 // Positive contactor output 1 (physical load TBD)
#define RELAY_CH_POS_2 2 // Positive contactor output 2 (physical load TBD)
#define RELAY_CH_POS_3 3 // Positive contactor output 3 (physical load TBD)
#define RELAY_CH_POS_4 4 // Positive contactor output 4 (physical load TBD)
#define RELAY_CH_POS_5 5 // Positive contactor output 5 (physical load TBD)
#define RELAY_CH_POS_6 6 // Positive contactor output 6 (physical load TBD)
#define RELAY_CH_POS_7 7 // Positive contactor output 7 (physical load TBD)
#define RELAY_CH_POS_8 8 // Positive contactor output 8 (physical load TBD)
#define RELAY_CH_POS_9 9 // Positive contactor output 9 (physical load TBD)

#define RELAY_TOTAL_CHANNELS 10

// --- UKS LoRa Heartbeat Byte ---
// 9.2.a: RF hatti tek yonlu telemetri + heartbeat'tir; UKS->AKS komut
// kanali (eski 0xA1-0xA4) sistemden tamamen kaldirildi.
#define UKS_HEARTBEAT_BYTE                                                     \
  0xB0 // UKS ~1 Hz periyodik heartbeat (stabilizasyon teyidi)

// --- LoRa Link Monitörü ---
#define LINK_TIMEOUT_MS 3000U // 3 sn: 1 Hz heartbeat için 3x marj

// Boot anindan itibaren bu sure icinde HIC heartbeat gelmediyse link DOWN
// kabul edilir (9.2.e / 9.4.b.vi): arac acildiginda UKS hic yayinda
// degilse AKS'in sonsuza dek "link UP" varsayip o donemin verisini
// kaybetmesini onler (bkz. link_check_timeout_with_boot_grace).
#define BOOT_LINK_GRACE_MS 5000U

// --- Offline Buffer Örnekleme ve Replay (9.2.e / 9.2.h / 9.4.b.vi) ---
// Kesinti sirasinda buffer'a yazilan ornekleme periyodu. 9.2.h izleme
// merkezi kayitlari arasi en fazla 5 sn kuralina 5x marjla uyar ve
// OB_CAPACITY / replay suresini 5'e boler (60 sn'lik kesinti icin 300
// yerine 60-75 paket).
#define OFFLINE_SAMPLE_PERIOD_MS 1000U

// Link UP oldugunda tek LORA_TX_PERIOD_MS tikinde en fazla bu kadar
// buffered (replay) paket gonderilir; canli paket akisi hic kesilmeden
// (1 canli + en fazla bu kadar replay / tik) buffer bosaltilir (S1).
#define REPLAY_BURST_PER_TICK 3

// --- LoRa RX Tanısı ---
#define LORA_UNKNOWN_BYTE_WARN_INTERVAL_MS                                     \
  10000U // RF gurultu tanisi icin en fazla 1 WARN / 10 sn

// --- Phase 1 Planning Notes ---
// Torque command generation is intentionally held at zero until the pedal /
// brake input model is finalized. READY -> DRIVE enable is now command-driven,
// but propulsion stays inhibited until the torque mapping rules are defined.
//
// Contactor opening is still immediate in FAULT / EMERGENCY_STOP. The future
// safe shutdown sequence should coordinate:
// 1. Zero torque request
// 2. Short hold time for motor torque decay
// 3. Contactor opening
#define VCU_CONTACTOR_OPEN_DELAY_MS 20

// --- Phase 2 Safety Thresholds ---
// Warning levels should eventually trigger derating.
// Critical levels should force a transition to FAULT.
//
// AÇIK İŞ (Lithium Balance c-BMS geçişi): TEL_bmsTempHighestC/LowestC hiçbir
// CAN ID'den parse edilmiyor (bkz. CanParse.cpp parseLbBmsE001..E033
// stub'ları), value-init default'unda (0) kalıyor — bu yüzden aşağıdaki
// TEMP eşikleri şu an fiilen HİÇ TETİKLENMEZ. CAN ID reverse-engineering'i
// tamamlanıp gerçek parse eklendiğinde bu not kaldırılmalı.
#define BMS_WARN_MAX_TEMP_C 55
#define BMS_CRITICAL_MAX_TEMP_C 70
// Current thresholds in centi-mA (0.01 mA units) — BMS Pack Current resolution.
// 1 A = 100 000 centi-mA.
// NOT: Bu eşikler Solion föyünden alınmıştı, Lithium Balance c-BMS'in akım
// çözünürlüğü henüz DOĞRULANMADI. Gerçek BMS dokümanı ile eşleştirilmeli.
#define BMS_WARN_MAX_CHARGE_CURRENT_CENTI_MA 90000          // 0.9 A
#define BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_MA 100000     // 1.0 A
#define BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_MA 900000      // 9.0 A
#define BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_MA 1500000 // 15.0 A
// Pack voltage thresholds in decivolts (1 deciV = 0.1 V).
// Lithium Balance c-BMS Pack Voltage: CAN ID 0xE000, byte[2:3],
// big-endian uint16, raw * 0.1 = V. (DOĞRULANDI)
#define BMS_WARN_MIN_PACK_VOLTAGE_DECI_V 740
#define BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V 700
#define BMS_WARN_MAX_PACK_VOLTAGE_DECI_V 850
#define BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V 870

// Task watchdog timing is still using the ESP-IDF default configuration.
// The shorter LoRa RX timeout below improves scheduling margin, but the global
// watchdog timeout should still be reviewed once final task runtimes stabilize.

// --- CAN Freshness Thresholds ---
#define CAN_MOTOR_STATUS_TIMEOUT_MS 1500 // 500ms periyotla gelen veri için 3x marj (jitter'ı önlemek için)
#define CAN_BMS_STATUS_TIMEOUT_MS 500

// UKS'in aralik-disi alan sanitizasyonu (CanManager::getTelemetryData)
// tetiklendiginde ayni durum tekrar tekrar olussa bile log spam'ini
// onlemek icin alan basina en fazla 1 WARN / bu sure.
#define TEL_SANITIZE_WARN_THROTTLE_MS 10000

#endif // SYSTEM_CONFIG_H
