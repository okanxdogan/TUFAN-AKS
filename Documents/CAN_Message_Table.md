# CAN Message Table

This table documents the CAN frames currently implemented or reserved by the AKS firmware.

## Motor Driver Frames

### `0x100` Torque Command

Direction: `AKS -> Motor Driver`

| Byte | Field | Type | Scale | Description |
| --- | --- | --- | --- | --- |
| 0 | Torque MSB | `uint8_t` | raw | High byte of torque command |
| 1 | Torque LSB | `uint8_t` | raw | Low byte of torque command |

Notes:

- The CAN task transmits torque at the control loop rate.
- When VCU state is not `DRIVE`, torque is forced to `0` for safety.

### `0x200` Motor Status

Direction: `Motor Driver -> AKS`

| Byte | Field | Type | Scale | Description |
| --- | --- | --- | --- | --- |
| 0 | RPM MSB | `uint8_t` | raw | High byte of motor RPM |
| 1 | RPM LSB | `uint8_t` | raw | Low byte of motor RPM |
| 2 | Torque Feedback MSB | `uint8_t` | raw | High byte of signed torque feedback |
| 3 | Torque Feedback LSB | `uint8_t` | raw | Low byte of signed torque feedback |
| 4 | Error Flags | `uint8_t` | bitfield | Motor driver fault / warning flags |

Freshness rule:

- If no valid `0x200` frame is received for `500 ms`, AKS marks motor data invalid.
- A post-reception motor timeout is treated as a critical safety condition outside `IDLE`.

## Lithium Balance c-BMS Frames

Source: Lithium Balance c-BMS24 — CAN sniffer logları ile reverse-engineering

Protocol parameters:
- ID Format: **29-bit Extended** (`CAN 2.0b`)
- Byte Order: **Big Endian** (doğrulanmış alanlar için)
- CAN speed: **500 kbps** (ESP-IDF TWAI driver ayarı ile uyumlu)
- 120 Ω termination resistor: TBD

### `0xE000` — Pack Voltage (ÇÖZÜLDÜ)

Direction: `BMS -> AKS` | Status: **DOĞRULANDI** (CAN sniffer + reverse-engineering)

| Byte | Field | TelemetryData | Type | Scale | Durum | Description |
| --- | --- | --- | --- | --- | --- | --- |
| 0–1 | BİLİNMİYOR | — | — | — | ❌ DOĞRULANMADI | Alan anlamı bilinmiyor |
| 2–3 | Pack Voltage | `TEL_bmsPackVoltageDeciV` | `uint16_t` | × 0.1 V | ✅ DOĞRULANDI | Total pack voltage |
| 4–5 | BİLİNMİYOR | — | — | — | ❌ DOĞRULANMADI | Alan anlamı bilinmiyor |

Doğrulama: CAN sniffer ile `0x020E` okundu → 52.6 V (gerçek batarya voltajı ile eşleşiyor).

### `0xE001` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. CAN sniffer loglarında tekrar eden bir ID olarak gözlemlendi.
Ham byte'lar debug log'a basılıyor, TelemetryData'ya yazılmıyor.

### `0xE002` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. Ham byte'lar debug log'a basılıyor.

### `0xE003` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. Ham byte'lar debug log'a basılıyor.

### `0xE004` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. Ham byte'lar debug log'a basılıyor.

### `0xE005` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. Ham byte'lar debug log'a basılıyor.

### `0xE032` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. Ham byte'lar debug log'a basılıyor.

### `0xE033` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. Ham byte'lar debug log'a basılıyor.

### `0x000` — Standart Frame

Direction: `Bilinmiyor` | Status: **DOĞRULANMADI**

CAN sniffer loglarında ara sıra görülen, tüm byte'ları sıfır olan 11-bit standart frame. Firmware tarafından şu an işlenmiyor.

## Sonraki Adımlar

Bilinmeyen ID'lerin çözümü için önerilen yöntem:
1. **PeakCAN + LiBAL c-BMS CREATOR yazılımı** ile çapraz doğrulama
2. Her ID için bilinen bir parametreyi değiştirip (ör. yük altına alarak akım, şarj ederek SOC) CAN loglarında hangi ID/byte'ın değiştiğini gözlemlemek
3. Lithium Balance teknik destek / doküman talebi

## Legacy / Reserved

### `0x300` Legacy BMS Status

Direction: `BMS -> AKS`

Status: reserved for backward compatibility logging only. The current firmware does not parse payload fields from this frame.
