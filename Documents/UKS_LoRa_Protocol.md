# UKS <-> AKS LoRa Protocol

This document defines the current command and telemetry contract between the UKS ground unit and the AKS firmware.

## Link Assumptions

- Radio module: E22-400T30D-V2 (SX1268) — pin-compatible successor to the retired E32-433T30D
- UART mode: transparent mode
- Selected startup mode: `M0 = 0`, `M1 = 0`
- UART baud: `9600`
- Air data rate: `9.6 kbps` (`REG0` bit[2:0] = `100`)
- Frequency: `433.125 MHz` (channel `23`, `REG2 = 0x17`, base offset `410.125 MHz`)
- Telemetry transmit period: `200 ms` (`5 Hz`)
- No application-layer ACK or retransmission is implemented in AKS at this stage.

Loss handling policy:

- AKS transmits the latest telemetry snapshot only.
- If a packet is missed, the next packet replaces it.
- UKS should detect packet loss by checking the telemetry sequence counter.

## UKS -> AKS Command Bytes

Single-byte command values currently recognized by AKS:

| Symbol | Value | Meaning |
| --- | --- | --- |
| `UKS_CMD_EMERGENCY_STOP` | `0xA1` | Immediate emergency stop request |
| `UKS_CMD_START` | `0xA2` | Request `IDLE -> READY` |
| `UKS_CMD_STOP` | `0xA3` | Request reset / stop |
| `UKS_CMD_DRIVE_ENABLE` | `0xA4` | Request `READY -> DRIVE` |

Receiver behavior:

- Unknown command bytes are ignored.
- Commands are interpreted from `LO_rxBuffer[0]`.
- AKS does not currently implement command CRC, framing bytes, or retransmission.

## AKS -> UKS Telemetry Format

AKS transmits one ASCII CSV line per sample with `CRLF` line termination.

Example:

```text
TEL,1,42,3150,12,0,1,0,77,-15,28,612,3400,0,1
```

Field order is fixed and must be parsed positionally:

| Index | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `TEL` | literal | Packet type tag |
| 1 | protocol version | `uint8` | Current value: `1` |
| 2 | sequence | `uint32` | Increments on each successful TX |
| 3 | motor RPM | `uint16` | Latest motor RPM |
| 4 | motor torque feedback | `int16` | Latest signed torque feedback |
| 5 | motor error flags | `uint8` | Motor fault flags |
| 6 | motor data valid | `0/1` | `1` if latest motor frame is considered fresh |
| 7 | motor timeout active | `0/1` | `1` if motor status timed out after first reception |
| 8 | BMS SOC | `uint8` | State of charge in percent |
| 9 | BMS current | `int16` | Pack current in deci-amps |
| 10 | BMS temperature | `int16` | Degrees Celsius |
| 11 | BMS pack voltage | `uint16` | Deci-volts |
| 12 | BMS average cell voltage | `uint16` | Millivolts |
| 13 | BMS error flags | `uint8` | BMS fault flags |
| 14 | BMS data valid | `0/1` | `1` if BMS telemetry fields are populated |

UKS parser requirements:

- Reject packets whose field count is not exactly `15`.
- Reject packets whose tag is not `TEL`.
- Track `sequence` to detect dropped packets.
- Treat repeated `sequence` values as duplicate or stale samples.

## Alan Aralıkları ve AKS Tarafı Sanitizasyon (v2, 19 alan)

> **NOT — bu bölüm günceldir, yukarıdaki "AKS -> UKS Telemetry Format"
> tablosu ise eski v1 (15 alan) protokolünü anlatır ve güncel değildir.**
> Güncel format `ESP_AKS/lib/Telemetry/Telemetry.cpp::sendStatus()` ve
> `TUFAN-UKS-TELEMETRY/Core/Src/telemetry.c::Decode_Line()` ile birebir
> doğrulanmıştır: toplam **19 token** (ilk token literal `"TEL"`, ardından
> 18 sayısal alan), sıra: `ver,seq,rpm,torque,motorErr,motorValid,
> motorTimeout,cellVMax,cellVMin,tempH,tempL,sysState,packV,current,soc,
> bmsValid,ts_ms,spd_x10`.

UKS `Decode_Line`, her alanı `Parse_Int` ile ayrı ayrı sert aralık
kontrolünden geçirir. **Alanlardan TEK BİRİ aralık dışıysa UKS tüm
frame'i reddeder** (`parse_fail`) — yani o anki RPM, gerilim, akım,
sıcaklık gibi diğer tüm geçerli alanlar da birlikte kaybolur. Şartname
9.2.b/9.2.d telemetri akışının sürekliliğini zorunlu kıldığından, AKS
UKS'in kabul aralığı dışına kesinlikle çıkmayan değerler üretmelidir.

| Alan | UKS kabul aralığı | AKS garantisi | Nasıl |
| --- | --- | --- | --- |
| `sysState` | `1..4` | Garantili | `CanManager::getTelemetryData()` içinde `TelemetrySanitize::sanitizeSystemState()` — aralık dışı değer `4` (FAULT) yapılır |
| `soc` | `0..10000` | Garantili | `TelemetrySanitize::sanitizeSoc()` — `10000`'e clamp edilir |
| `current` | `-2147483647..2147483647` (tam `int32_t` değil, `INT32_MIN` hariç) | Garantili | `TelemetrySanitize::sanitizeCurrent()` — tam `INT32_MIN` görülürse `INT32_MIN+1`'e kaydırılır |
| `spd_x10` | `0..3000` | Garantili | `rpmToSpeedKmhX10()` içindeki clamp `TEL_SPD_X10_MAX` (=3000) sabitine göre yapılır (`Telemetry.h`) |
| `cellVMax`/`cellVMin`, `packV` | `0..65535` (`uint16_t` tip sınırı) | Garantili (tip sınırı = kabul aralığı) | Ek işlem gerekmez |
| `tempH`/`tempL` | `-128..127` (`int8_t` tip sınırı) | Garantili (tip sınırı = kabul aralığı) | Ek işlem gerekmez |
| `ts_ms` | `0..2147483647` | **GARANTİ EDİLMİYOR** | Bilinen açık sorun: `esp_timer_get_time()/1000` tabanlı 32-bit boot-ms sayaç ~24.8 günden sonra bu sınırı aşar (sarma noktası ~49.7 gün). **Bu ayrı bir iş kalemi olarak UKS tarafında ele alınacaktır — bkz. "DOĞRULANACAK" listesi altında değil, ayrı takip.** |
| `seq` | `0..2147483647` | Pratikte garantili (uzun vadeli, düşük öncelik) | `TEL_sequenceCounter` `uint32_t`, teorik olarak ~2.1 milyar paketten sonra bu sınırı aşabilir |

### DOĞRULANACAK

- **`sysState` alanının kaynağı ve eşlemesi henüz DOĞRULANMADI.**
  Lithium Balance c-BMS'in hangi CAN ID'sinde, hangi byte'ta ve hangi
  kodlama ile (1=Discharge, 2=IDLE, 3=Charge, 4=FAULT) system state
  gönderdiği bilinmiyor. Şu an `TEL_bmsSystemState` alanı hiçbir CAN
  ID'den parse EDİLMİYOR (sıfır olarak kalıyor). İlgili ID çözüldüğünde
  `CanParse::parseLbBmsExxx()` stub'larından birine gerçek parse
  eklenmeli ve eşleme tablosu doğrulanmalı.
  `TelemetrySanitize::sanitizeSystemState()` aralık-dışı durumu zaten
  FAULT'a çevirdiği için sistem güvenli tarafta kalır, ama telemetri
  UKS ekranında yanlış durum gösterebilir.

## Future Extensions

Planned but not implemented:

- Framed command packets with checksum
- ACK / retransmission policy
- Explicit fault and VCU state fields in uplink telemetry

Note: radio configuration management through the E22 register protocol is
now implemented (`vTask_LoRa_UKS` boot sequence + `lib/E22Config`) — this
was previously listed here as a future item under the E32 module.
