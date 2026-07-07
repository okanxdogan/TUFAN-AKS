# UKS <-> AKS LoRa Protocol

This document defines the current command and telemetry contract between the UKS ground unit and the AKS firmware.

## Link Assumptions

> **Not (link flapping düzeltmesi):** `LORA_TX_PERIOD_MS` 200'den 500'e
> (5 Hz → 2 Hz), `LINK_TIMEOUT_MS` ise 3000'den 9000'e çıkarıldı — bkz.
> `Documents/LoRa_Link_Analysis.md` ve `include/SystemConfig.h`.

- Radio module: E22-400T30D-V2 (SX1268) — pin-compatible successor to the retired E32-433T30D
- UART mode: transparent mode
- Selected startup mode: `M0 = 0`, `M1 = 0`
- UART baud: `9600`
- Air data rate: `9.6 kbps` (`REG0` bit[2:0] = `100`)
- Frequency: `433.125 MHz` (channel `23`, `REG2 = 0x17`, base offset `410.125 MHz`)
- Telemetry transmit period: `500 ms` (`2 Hz`) — lowered from `200 ms` (`5 Hz`) to fix link flapping: on a single-frequency half-duplex E22 channel, continuous 5 Hz AKS TX left little room for UKS's 1 Hz `0xB0` heartbeat to get through (field logs showed ~5-6 s effective heartbeat interval, shorter than the previous `LINK_TIMEOUT_MS`, causing constant DOWN→UP flapping)
- No application-layer ACK or retransmission is implemented in AKS at this stage.

Loss handling policy:

- AKS transmits the latest telemetry snapshot only.
- If a packet is missed, the next packet replaces it.
- UKS should detect packet loss by checking the telemetry sequence counter.

## RF hattı tek yönlü (9.2.a)

TEKNOFEST Elektromobil yönetmeliği madde 9.2.a gereği araç ile izleme
merkezi arasındaki haberleşme yalnızca araçtan izleme merkezine (AKS ->
UKS) tek yönlü veri aktarımı olacak şekildedir. Bu maddenin tek istisnası,
haberleşme stabilizasyonunu teyit amaçlı UKS -> AKS **heartbeat**
sinyalidir (`0xB0`, bkz. aşağıdaki tablo) — içerik taşımaz, yalnızca
UKS'in canlı olduğunu doğrular.

Bu nedenle UKS -> AKS yönünde eskiden var olan tek-byte komut kanalı
(`UKS_CMD_EMERGENCY_STOP` `0xA1`, `UKS_CMD_START` `0xA2`, `UKS_CMD_STOP`
`0xA3`, `UKS_CMD_DRIVE_ENABLE` `0xA4`) sistemden tamamen kaldırılmıştır.
Elektromobil sınıfında uzaktan durdurma zorunluluğu yoktur; acil durdurma
araç üstündeki fiziksel kontaktörle sağlanır, RF hattından bağımsızdır.

AKS RX davranışı:

| Byte | Anlam | Davranış |
| --- | --- | --- |
| `UKS_HEARTBEAT_BYTE` (`0xB0`) | Stabilizasyon teyidi (~1 Hz) | `s_lastHeartbeatMs` güncellenir → `LinkMonitor` / `OfflineBuffer` zinciri |
| Diğer her byte | Tanımsız / RF gürültüsü | Yok sayılır; sayaç artar; en fazla 1 WARN / 10 sn (bkz. `LoraRxHandler.h`) |

AKS komut CRC'si, çerçeveleme byte'ı veya yeniden gönderim mekanizması
implemente etmiyor — bunlara artık ihtiyaç yok, çünkü işlenen tek RX
byte'ı heartbeat'tir.

## AKS -> UKS Telemetry Format (v2, 19 fields)

AKS transmits one ASCII CSV line per sample with `CRLF` line termination.

Example (real AKS golden-test vector, see `ESP_AKS` native tests):

```text
TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,12345,1413
```

Field order is fixed and must be parsed positionally:

| Index | Field | Type | Scale | Description |
| --- | --- | --- | --- | --- |
| 0 | `TEL` | literal | — | Packet type tag |
| 1 | `ver` | `uint8` | — | Protocol version (current: `2`) |
| 2 | `seq` | `uint32` | — | Increments on each successful TX |
| 3 | `rpm` | `uint16` | raw | Latest motor RPM |
| 4 | `torque` | `int16` | raw | Latest signed torque feedback |
| 5 | `motorErr` | `uint8` | bit flags | Motor fault flags |
| 6 | `motorValid` | `0/1` | — | `1` if latest motor frame is considered fresh |
| 7 | `motorTimeout` | `0/1` | — | `1` if motor status timed out after first reception |
| 8 | `cellVMax` | `uint16` | ×0.1 mV | Highest cell voltage |
| 9 | `cellVMin` | `uint16` | ×0.1 mV | Lowest cell voltage |
| 10 | `tempH` | `int16` (source `int8`) | °C | Highest BMS temperature |
| 11 | `tempL` | `int16` (source `int8`) | °C | Lowest BMS temperature |
| 12 | `sysState` | `uint8` | enum | `1`=Discharge `2`=IDLE `3`=Charge `4`=FAULT |
| 13 | `packV` | `uint16` | ×0.1 V | Pack voltage |
| 14 | `current` | `int32` | ×0.01 mA | Pack current (`+`charge / `-`discharge) |
| 15 | `soc` | `uint16` | ×0.01 % | State of charge (`10000`=100.00%) |
| 16 | `bmsValid` | `0/1` | — | `1` if BMS telemetry fields are populated |
| 17 | `ts_ms` | `uint32` | ms | Milliseconds since AKS boot |
| 18 | `spd_x10` | `uint16` | ×0.1 km/h | Vehicle speed |

UKS parser requirements:

- Reject packets whose field count is not exactly `19`.
- Reject packets whose tag is not `TEL`.
- Reject packets whose `ver` is not `2`.
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

Cancelled: framed command packets with checksum — the UKS -> AKS command
channel was removed entirely for 9.2.a compliance (see "RF hattı tek
yönlü (9.2.a)" above); there is no command payload left to frame.

Still planned but not implemented:

- ACK / retransmission policy
- Explicit fault and VCU state fields in uplink telemetry

Note: radio configuration management through the E22 register protocol is
now implemented (`vTask_LoRa_UKS` boot sequence + `lib/E22Config`) — this
was previously listed here as a future item under the E32 module.
