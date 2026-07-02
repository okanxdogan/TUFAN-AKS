#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// SystemConfig.h
// Centralized configuration for pin assignments, timeouts, and other constants
// Used across multiple modules for consistency

// --- Includes ---
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"

// --- CAN Message IDs ---
//#define CAN_ID_TORQUE_CMD 0x100    // AKS → Motor Driver
#define CAN_ID_MOTOR_STATUS 0x200  // Motor Driver → AKS
#define CAN_ID_BMS_STATUS 0x300    // Legacy (unused)
// Solion SK Serisi BMS — 29-bit Extended ID, Big Endian, 125kbps
#define CAN_ID_SOLION_BMS_A 0x111  // Cell voltages, temperatures, system state
#define CAN_ID_SOLION_BMS_B 0x112  // Pack voltage, pack current, SOC

// --- CAN (TJA1050 transceiver) ---
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

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

// --- LoRa E32-433T30D (UART & Kontrol) ---
#define LORA_UART_NUM UART_NUM_2
#define LORA_TX_PIN GPIO_NUM_16   // ESP TX -> Şemadaki LR_RXD (IO16)
#define LORA_RX_PIN GPIO_NUM_17   // ESP RX <- Şemadaki LR_TXD (IO17)
#define LORA_AUX_PIN GPIO_NUM_35  // IO35 sadece giriş; dahili pull-up YOK — harici 10k pull-up gerekli
#define LORA_M0_PIN GPIO_NUM_25   // Şemadaki MO (IO25)
#define LORA_M1_PIN GPIO_NUM_26   // Şemadaki M1 (IO26)
#define LORA_UART_BAUD 9600       // MCU↔E32 yerel seri hız (config modunda da aynı)
#define LORA_TX_PERIOD_MS 200     // 5 Hz telemetry uplink
#define LORA_RX_TIMEOUT_MS 20
#define LORA_MODE_NORMAL_M0_LEVEL 0
#define LORA_MODE_NORMAL_M1_LEVEL 0
#define LORA_AUX_READY_LEVEL 1
#define LORA_PROTOCOL_VERSION 2

// --- E32 Register Values (UKS lora.h ile birebir eşleştirilmeli) ---
// E32-433T30D SPED byte bit alanları (datasheet Tablo 4):
//   bit[7:6] = UART baud : 00=1200 01=2400 10=4800 11=9600
//   bit[5:3] = parity    : 000=8N1 001=8O1 010=8E1
//   bit[2:0] = air rate  : 000=0.3k 001=1.2k 010=2.4k 011=4.8k 100=9.6k 101=19.2k
//
// SPED=0xC4 = 1100 0100 → bit[7:6]=11(9600 baud) | bit[5:3]=000(8N1) | bit[2:0]=100(9.6 kbps air)
// 9.6 kbps air: ~78 byte v2 telemetri paketi ~65 ms havada kalır → 200 ms / 5 Hz periyoduna sığar.
// ÖNEMLİ: UKS lora.h'de de SPED=0xC4 olmalı (eski 0xC2=2.4kbps air / 0x1A=1200 baud YANLIŞTIR).
#define LORA_CFG_ADDH   0x00U
#define LORA_CFG_ADDL   0x00U
#define LORA_CFG_SPED   0xC4U
#define LORA_CFG_CHAN   0x17U  // kanal 23 -> 433 MHz
#define LORA_CFG_OPTION 0x44U  // transparent | push-pull | 250ms wake | FEC on | max-güç kodu (bit[1:0]=00)
// OPTION bit[1:0] güç: 00=en yüksek, 01=orta-yüksek, 10=orta-düşük, 11=en düşük (~10 dBm).
// T30D harici PA bu register değerini farklı yorumlar; sahada menzil testiyle doğrula.

// --- E32 Config Modu Zaman Aşımları ---
#define LORA_AUX_MODE_TIMEOUT_MS  500   // M0/M1=1 sonrası AUX HIGH bekleme (ms)
#define LORA_AUX_CFG_TIMEOUT_MS   2000  // Config yazımı sonrası flash tamamlanma (ms)

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
#define RELAY_CH_POS_0 0  // Positive contactor output 0 (physical load TBD)
#define RELAY_CH_POS_1 1  // Positive contactor output 1 (physical load TBD)
#define RELAY_CH_POS_2 2  // Positive contactor output 2 (physical load TBD)
#define RELAY_CH_POS_3 3  // Positive contactor output 3 (physical load TBD)
#define RELAY_CH_POS_4 4  // Positive contactor output 4 (physical load TBD)
#define RELAY_CH_POS_5 5  // Positive contactor output 5 (physical load TBD)
#define RELAY_CH_POS_6 6  // Positive contactor output 6 (physical load TBD)
#define RELAY_CH_POS_7 7  // Positive contactor output 7 (physical load TBD)
#define RELAY_CH_POS_8 8  // Positive contactor output 8 (physical load TBD)
#define RELAY_CH_POS_9 9  // Positive contactor output 9 (physical load TBD)

#define RELAY_TOTAL_CHANNELS 10

// --- UKS LoRa Komut ve Heartbeat Byte Tanımları ---
#define UKS_CMD_EMERGENCY_STOP 0xA1
#define UKS_CMD_START          0xA2
#define UKS_CMD_STOP           0xA3
#define UKS_CMD_DRIVE_ENABLE   0xA4
#define UKS_HEARTBEAT_BYTE     0xB0   // UKS ~1 Hz periyodik heartbeat (komut değil)

// --- LoRa Link Monitörü ---
#define LINK_TIMEOUT_MS        3000U  // 3 sn: 1 Hz heartbeat için 3x marj

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
#define BMS_WARN_MAX_TEMP_C 55
#define BMS_CRITICAL_MAX_TEMP_C 70
// Current thresholds in centi-mA (0.01 mA units) — Solion SK Pack Current resolution.
// 1 A = 100 000 centi-mA.
#define BMS_WARN_MAX_CHARGE_CURRENT_CENTI_MA    90000    // 0.9 A
#define BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_MA 100000  // 1.0 A
#define BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_MA  900000  // 9.0 A
#define BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_MA 1500000 // 15.0 A
// Pack voltage thresholds in decivolts (1 deciV = 0.1 V).
// The Solion SK BMS reports Pack Voltage with 0.1 V resolution;
// raw = V * 10, so a 78 V nominal pack sits around raw 780.
#define BMS_WARN_MIN_PACK_VOLTAGE_DECI_V 740
#define BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V 700
#define BMS_WARN_MAX_PACK_VOLTAGE_DECI_V 850
#define BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V 870

// Task watchdog timing is still using the ESP-IDF default configuration.
// The shorter LoRa RX timeout below improves scheduling margin, but the global
// watchdog timeout should still be reviewed once final task runtimes stabilize.

// --- CAN Freshness Thresholds ---
#define CAN_MOTOR_STATUS_TIMEOUT_MS 500
#define CAN_BMS_STATUS_TIMEOUT_MS   500

// UKS'in aralik-disi alan sanitizasyonu (CanManager::getTelemetryData)
// tetiklendiginde ayni durum tekrar tekrar olussa bile log spam'ini
// onlemek icin alan basina en fazla 1 WARN / bu sure.
#define TEL_SANITIZE_WARN_THROTTLE_MS 10000

#endif  // SYSTEM_CONFIG_H
