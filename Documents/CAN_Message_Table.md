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

Sniffer oturumları:
1. **Oturum 1**: İlk keşif oturumu (pack voltajı ~52.6 V olan paket)
2. **Oturum 2**: 24.421 frame, ~8 dk, paket idle (pack voltajı ~79.0 V olan farklı bir paket)

Protocol parameters:
- ID Format: **29-bit Extended** (`CAN 2.0b`)
- Byte Order: **Big Endian** (doğrulanmış alanlar için)
- CAN speed: **500 kbps** (ESP-IDF TWAI driver ayarı ile uyumlu)
- 120 Ω termination resistor: TBD

Güven seviyeleri: **DOĞRULANDI** (bağımsız ölçümle teyit edildi) / **HIPOTEZ-orta** (tutarlı
gözlem var, bağımsız teyit yok) / **HIPOTEZ-düşük** (UNVERIFIED — ölçek/anlam bilinmiyor).
HIPOTEZ seviyesindeki alanlar firmware'de parse edilmemeli, TelemetryData'ya yazılmamalıdır.

### `0xE000` — Pack Voltage (ÇÖZÜLDÜ)

Direction: `BMS -> AKS` | Status: **DOĞRULANDI** — yalnızca `byte[2:3]`; diğer alanlar hipotez seviyesinde (CAN sniffer + reverse-engineering, 2 oturum)

| Byte | Field | TelemetryData | Type | Scale | Durum | Description |
| --- | --- | --- | --- | --- | --- | --- |
| 0–1 | Pack Current (aday) | — | `int16_t` | UNVERIFIED — ölçek bilinmiyor | ⚠️ HIPOTEZ-orta | Oturum 2'de idle iken `0xFFFF` → −1 ve `0xFFFE` → −2 okundu; sıfıra yakın signed değer idle pack akımı ile tutarlı. Ölçek doğrulanmadı. |
| 2–3 | Pack Voltage | `TEL_bmsPackVoltageDeciV` | `uint16_t` | × 0.1 V | ✅ DOĞRULANDI | Total pack voltage |
| 4–5 | Kapasite sayacı (aday) | — | — | UNVERIFIED — ölçek bilinmiyor | ⚠️ HIPOTEZ-düşük | Oturum 2'de idle boyunca yavaşça azaldı (`0x0F5E` → `0x0F5B`). Kapasite/sayaç adayı; anlamı ve ölçeği doğrulanmadı. |
| 6–7 | Kapasite sayacı (aday) | — | — | UNVERIFIED — ölçek bilinmiyor | ⚠️ HIPOTEZ-düşük | Oturum 2'de idle boyunca yavaşça azaldı (`0x0971` → `0x096D`). Kapasite/sayaç adayı; anlamı ve ölçeği doğrulanmadı. |

Örnek frame'ler (Oturum 2, paket idle):
- Oturum başı: `FF FF 03 16 0F 5E 09 71` → packV = 790 (79.0 V), current_raw = −1
- Oturum sonu: `FF FE 03 16 0F 5B 09 6D` → packV = 790 (79.0 V), current_raw = −2

Doğrulama:
- **Oturum 1**: CAN sniffer ile `0x020E` okundu → 52.6 V (gerçek batarya voltajı ile eşleşiyor).
- **Oturum 2**: Aynı decode kuralı (`byte[2:3]` × 0.1 V) farklı bir pakette `0x0316` okundu → 79.0 V
  (o paketin gerçek voltajı ile eşleşiyor).

Decode kuralı iki farklı paket/oturumda bağımsız olarak doğrulandı — güçlü kanıt.

### `0xE001` — Analog Kanallar + Sıcaklık Adayı (HIPOTEZ)

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI** (hipotez seviyesinde gözlemler var)

Örnek frame (Oturum 2, paket idle): `80 84 80 AB 80 96 19 19`

| Byte | Field | Type | Scale | Durum | Description |
| --- | --- | --- | --- | --- | --- |
| 0–1 | Analog kanal 1 (aday) | — | UNVERIFIED — ölçek bilinmiyor | ⚠️ HIPOTEZ-düşük | `0x8084` — `0x8000` civarı offset'li analog kanal adayı |
| 2–3 | Analog kanal 2 (aday) | — | UNVERIFIED — ölçek bilinmiyor | ⚠️ HIPOTEZ-düşük | `0x80AB` — aynı desen |
| 4–5 | Analog kanal 3 (aday) | — | UNVERIFIED — ölçek bilinmiyor | ⚠️ HIPOTEZ-düşük | `0x8096` — aynı desen |
| 6 | Sıcaklık 1 (aday) | `uint8_t`? | °C? | ⚠️ HIPOTEZ-orta | `0x19` = 25; oturum sırasındaki ortam sıcaklığı ile tutarlı. Doğrulanmadı. |
| 7 | Sıcaklık 2 (aday) | `uint8_t`? | °C? | ⚠️ HIPOTEZ-orta | `0x19` = 25; byte 6 ile aynı değer. Doğrulanmadı. |

Ham byte'lar debug log'a basılıyor, TelemetryData'ya yazılmıyor. Hipotezler doğrulanana kadar
firmware'de parse edilmemelidir.

### `0xE002` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Örnek frame (Oturum 2): `03 70 03 E8 01 00 00 00`

Gözlem notları (Oturum 2):
- `byte[0:1]` = `0x0370` (880) ve `byte[2:3]` = `0x03E8` (1000), `0x1806E5F4` charger frame'indeki
  voltage/current setpoint değerleriyle birebir aynı — şarj limit parametresi adayı.
  ⚠️ HIPOTEZ-düşük, UNVERIFIED — ölçek/anlam doğrulanmadı.
- **Multiplex gözlemi**: `0xE002` ve `0xE004`, BMS'in yayın döngüsünde aynı slotu dönüşümlü
  (multiplexed) kullanıyor — ikisi aynı periyotta sırayla gönderiliyor.

Ham byte'lar debug log'a basılıyor.

### `0xE003` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Tüm byte alanları bilinmiyor. Ham byte'lar debug log'a basılıyor.

### `0xE004` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Örnek frame (Oturum 2): `01 89 03 E8 03 E8 00 00`

Gözlem notları (Oturum 2):
- Alan anlamları bilinmiyor; `0x03E8` (1000) değeri iki kez tekrarlanıyor.
  ⚠️ HIPOTEZ yok — UNVERIFIED, ölçek/anlam bilinmiyor.
- **Multiplex gözlemi**: `0xE004`, yayın döngüsünde `0xE002` ile aynı slotu dönüşümlü kullanıyor
  (bkz. `0xE002` notu).

Ham byte'lar debug log'a basılıyor.

### `0xE005` — BİLİNMİYOR

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Örnek frame (Oturum 2): `03 E8 05 DC 00 0B 71 B0`

Alan anlamları bilinmiyor (`0x03E8` = 1000, `0x05DC` = 1500 gibi yuvarlak değerler dikkat çekici
ama anlam/ölçek UNVERIFIED). Ham byte'lar debug log'a basılıyor.

### `0xE032` — BİLİNMİYOR (Reserved/Heartbeat Adayı)

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Oturum 2'de gözlemlenen tüm frame'lerde payload tamamen sıfır (`00 00 00 00 00 00 00 00`).
Reserved alan veya heartbeat adayı. Ham byte'lar debug log'a basılıyor.

### `0xE033` — BİLİNMİYOR (Reserved/Heartbeat Adayı)

Direction: `BMS -> AKS` | Status: **DOĞRULANMADI**

Oturum 2'de gözlemlenen tüm frame'lerde payload tamamen sıfır (`00 00 00 00 00 00 00 00`).
Reserved alan veya heartbeat adayı. Ham byte'lar debug log'a basılıyor.

### `0x1806E5F4` — Charger Komut Frame'i (J1939)

Direction: `BMS -> Charger` | Status: **DOĞRULANDI** (Oturum 2 sniffer; standart J1939 şarj
protokolü ile uyumlu)

J1939 çözümlemesi: PGN `0x1806`, DA (hedef adres) `0xE5`, SA (kaynak adres) `0xF4`.
Bu frame BMS tarafından şarj cihazına gönderilir; **AKS bu frame'i yalnızca DİNLER**, asla
göndermez.

| Byte | Field | Type | Scale | Durum | Description |
| --- | --- | --- | --- | --- | --- |
| 0–1 | Charge Voltage Setpoint | `uint16_t` | × 0.1 V | ✅ DOĞRULANDI | Şarj voltaj hedefi |
| 2–3 | Charge Current Setpoint | `uint16_t` | × 0.1 A | ✅ DOĞRULANDI | Şarj akım hedefi |
| 4–7 | BİLİNMİYOR | — | — | ❌ DOĞRULANMADI | Oturum 2'de hep `00 00 00 00` |

Örnek frame (Oturum 2): `03 70 03 E8 00 00 00 00` → Vset = 880 (88.0 V), Iset = 1000 (100.0 A).
Değerler 79.0 V'luk paket için makul şarj hedefleriyle tutarlı.

### `0x000` — Standart Frame (Reserved/Heartbeat Adayı)

Direction: `Bilinmiyor` | Status: **DOĞRULANMADI**

CAN sniffer loglarında ara sıra görülen, tüm byte'ları sıfır olan 11-bit standart frame.
Oturum 2'de de gözlemlenen tüm frame'lerde payload tamamen sıfırdı — reserved/heartbeat adayı.
Firmware tarafından şu an işlenmiyor.

## Sonraki Adımlar

Bilinmeyen ID'lerin çözümü için önerilen yöntem:
1. **PeakCAN + LiBAL c-BMS CREATOR yazılımı** ile çapraz doğrulama
2. Her ID için bilinen bir parametreyi değiştirip (ör. yük altına alarak akım, şarj ederek SOC) CAN loglarında hangi ID/byte'ın değiştiğini gözlemlemek
3. Lithium Balance teknik destek / doküman talebi

