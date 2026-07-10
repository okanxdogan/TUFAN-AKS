# CAN Message Table — TEK DOĞRULUK KAYNAĞI

> **Son güncelleme:** 2026-07-09
> **Zemin gerçeği:** Lithium Balance c-BMS24 gerçek CAN sniffer logu (Oturum 3,
> 2026-07-09 18:01, paket idle, ~80 V / ~25°C).
> **Kural:** Çelişki olursa bu tablo kazanır — eski yorum, TODO veya .md DEĞİL.

Bu tablo, AKS firmware'inin dinlediği veya gönderdiği TÜM CAN frame'lerini
byte-byte belgeler. Her alan için doğrulama durumu ve kanıt verilmiştir.

## Doğrulama Seviyeleri

| Seviye | Anlamı | Firmware Kuralı |
| --- | --- | --- |
| ✅ DOĞRULANDI | Gerçek log ile bağımsız olarak doğrulandı | TelemetryData'ya YAZILABİLİR, karar mantığına BAĞLANABİLİR |
| ⚠️ HİPOTEZ | Tutarlı gözlem var, bağımsız teyit yok | TelemetryData'ya YAZILMAZ, debug log'a basılır |
| ❌ BİLİNMİYOR | Alan anlamı / ölçeği bilinmiyor | TelemetryData'ya YAZILMAZ, ham byte loglanır |

## Protokol Parametreleri

| Parametre | Değer |
| --- | --- |
| ID Format | **29-bit Extended** (CAN 2.0b) |
| Byte Order | **Big Endian** (doğrulanmış alanlar için kanıtlandı) |
| CAN Bitrate | **500 kbps** (ESP-IDF TWAI driver, TJA1050 transceiver) |
| 120 Ω Termination | Mevcut değil (Solion föyü: "Mevcut değildir") |

---

## Motor Driver Frames

### `0x100` — Torque Command

Direction: `AKS → Motor Driver` | Status: **MOTOR_DRIVER_PRESENT=0 — henüz aktif değil**

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | Torque Command | uint16_t | Big | unsigned | raw | ❌ BİLİNMİYOR | Motor sürücüsü entegre değil, frame gönderilmiyor |

### `0x200` — Motor Status

Direction: `Motor Driver → AKS` | DLC: 8 | Status: **MSTest/mock_motor ile doğrulanmış**

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | RPM | int16_t | Big | signed | raw | ✅ DOĞRULANDI | MSTest/mock_motor |
| 2–3 | Motor Voltage | uint16_t | Big | unsigned | ×0.1 V | ✅ DOĞRULANDI | MSTest/mock_motor |
| 4–6 | Rezerve | — | — | — | — | ❌ BİLİNMİYOR | Kullanılmıyor |
| 7 | Error Flags / Running | uint8_t | — | unsigned | bitfield | ✅ DOĞRULANDI | bit0=çalışıyor, bit[7:1]=hata bayrakları |

Freshness: 500 ms timeout → `TEL_motorTimeoutActive`, IDLE dışında FAULT.

---

## Lithium Balance c-BMS Frames

Kaynak: Lithium Balance c-BMS24 — 29-bit Extended ID

### `0x0000E000` — Pack Telemetri (TAMAMEN ÇÖZÜLDÜ)

Direction: `BMS → AKS` | DLC: 8 | Status: **✅ DOĞRULANDI — tüm 4 alan**

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek / Çarpan | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | Pack Current | int16_t | Big | **signed** | ×0.1 A | ✅ DOĞRULANDI | `FF F5` → int16 = -11 → ×0.1 = **-1.1 A** (log: "Pack Akimi : -1.1 A" ✓); `FF F3` → -13 → **-1.3 A** ✓; `FF F1` → -15 → **-1.5 A** ✓ |
| 2–3 | Pack Voltage | uint16_t | Big | unsigned | ×0.1 V | ✅ DOĞRULANDI | `03 20` → 800 → ×0.1 = **80.0 V** (log: "paket voltajı : 80.0V" ✓). Önceki Oturum 1'de `02 0E` → 52.6 V, Oturum 2'de `03 16` → 79.0 V — her oturumda doğru. |
| 4–5 | SoC 1 (State of Charge) | uint16_t | Big | unsigned | ×0.01 % | ✅ DOĞRULANDI | `26 7D` → 9853 → ×0.01 = **98.53%** (log: "Kalan Kapasite / SoC 1 : 98.53" ✓) |
| 6–7 | SoC 2 (State of Charge) | uint16_t | Big | unsigned | ×0.01 % | ✅ DOĞRULANDI | `26 58` → 9816 → ×0.01 = **98.16%** (log: "Kalan Kapasite / SoC 2 : 98.16" ✓) |

**Firmware mapping:**
- `TEL_bmsCurrentCentiA` = raw × 10 (0.1A → centi-A dönüşümü)
- `TEL_bmsPackVoltageDeciV` = raw (zaten deciV)
- `TEL_bmsSocHundredths` = raw (SoC 1, zaten hundredths-%)
- SoC 2 şu an kullanılmıyor (gelecekte çapraz doğrulama için)

**Önceki "kapasite sayacı" hipotezi ÇÜRÜTÜLDÜ:** Oturum 2'de byte[4:5] ve byte[6:7]
"yavaşça azalan sayaç" olarak yorumlanmıştı. Oturum 3 logu, bu alanların sabit %98.53
ve %98.16 SoC değerleri olduğunu KANIT ile gösterir — BMS kendi SoC'sini raporluyor,
sayaç değil.

---

### `0x0000E001` — Sıcaklık + Bilinmeyen Analog Kanallar

Direction: `BMS → AKS` | DLC: 8 | Status: **Kısmi — byte[6:7] DOĞRULANDI, byte[0:5] BİLİNMİYOR**

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek / Çarpan | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | Bilinmiyor (Analog Kanal 1 ?) | — | — | — | — | ❌ BİLİNMİYOR | `82 1D`=0x821D, `82 25`=0x8225, `82 22`=0x8222, `82 1E`=0x821E, `82 24`=0x8224, `82 23`=0x8223 — 0x8000 civarında ofsete sahip, küçük varyasyonlu analog kanal deseni. Anlam/ölçek bilinmiyor. |
| 2–3 | Bilinmiyor (Analog Kanal 2 ?) | — | — | — | — | ❌ BİLİNMİYOR | `82 35`=0x8235, `82 36`=0x8236, `82 33`=0x8233, `82 34`=0x8234 — aynı 0x8000 ofseti deseni. |
| 4–5 | Bilinmiyor (Analog Kanal 3 ?) | — | — | — | — | ❌ BİLİNMİYOR | `82 27`=0x8227, `82 2E`=0x822E, `82 2B`=0x822B, `82 28`=0x8228 — aynı desen. |
| 6 | Temperature 1 (Kanal 1) | int8_t | — | signed | 1 °C (ofset yok) | ✅ DOĞRULANDI | `19` = 25 → **25°C** (log: "Batarya Sicakligi (Kanal 1) : 25 °C" ✓). Tüm 7 frame'de sabit `0x19`. |
| 7 | Temperature 2 (Kanal 2) | int8_t | — | signed | 1 °C (ofset yok) | ✅ DOĞRULANDI | `18` = 24 → **24°C** (log: "Batarya Sicakligi (Kanal 2) : 24 °C" ✓). Tüm 7 frame'de sabit `0x18`. |

**Firmware mapping:**
- `TEL_bmsTempHighestC` = max(byte[6], byte[7])
- `TEL_bmsTempLowestC` = min(byte[6], byte[7])
- byte[0:5] → yalnızca debug log, TelemetryData'ya YAZILMAZ

---

### `0x1806E5F4` — Charger Komut Frame'i (J1939)

Direction: `BMS → Charger` (AKS YALNIZCA DİNLER) | DLC: 8 | Status: **✅ DOĞRULANDI (kısmi)**

J1939 çözümlemesi: PGN `0x1806`, DA (hedef adres) `0xE5`, SA (kaynak adres) `0xF4`.

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek / Çarpan | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | Charge Voltage Setpoint | uint16_t | Big | unsigned | ×0.1 V | ✅ DOĞRULANDI | `03 70` → 880 → ×0.1 = **88.0 V** (Oturum 2'de de aynı). 24S LiFePO4 (3.65V/hücre) için makul şarj hedefi. |
| 2–3 | Charge Current Setpoint | uint16_t | Big | unsigned | ×0.1 A | ✅ DOĞRULANDI | `01 C5` → 453 → ×0.1 = **45.3 A**. Not: Oturum 2'de `03 E8` → 100.0 A idi — BMS'in şarj durumuna göre ayarladığı dinamik setpoint. |
| 4–7 | Bilinmiyor | — | — | — | — | ❌ BİLİNMİYOR | Tüm oturumlarda `00 00 00 00` |

**Not:** `byte[2:3]` Oturum 2'de `03 E8` (100.0 A), Oturum 3'te `01 C5` (45.3 A) — bu
fark BMS'in şarj algoritmasına göre akım setpoint'ini dinamik ayarladığını gösterir.

---

### `0x0000E002` — BİLİNMİYOR

Direction: `BMS → AKS` | DLC: 8 | Status: **❌ BİLİNMİYOR**

Bu CAN ID, Oturum 3 logunda GÖRÜLMEDİ (log'da yalnızca E000, E001, E005, 1806E5F4
mevcut). Oturum 2'de gözlemlenmişti.

Oturum 2 örneği: `03 70 03 E8 01 00 00 00`

| Byte | Alan Adı | Durum | Gözlem |
| --- | --- | --- | --- |
| 0–7 | Bilinmiyor | ❌ BİLİNMİYOR | Oturum 2: byte[0:1]=0x0370 (880), byte[2:3]=0x03E8 (1000) — şarj limit/config adayı. E004 ile multiplex paylaşımı gözlemlendi. |

---

### `0x0000E003` — BİLİNMİYOR

Direction: `BMS → AKS` | DLC: 8 | Status: **❌ BİLİNMİYOR**

Tüm byte alanları bilinmiyor. Oturum 3'te görülmedi.

---

### `0x0000E004` — BİLİNMİYOR

Direction: `BMS → AKS` | DLC: 8 | Status: **❌ BİLİNMİYOR**

Oturum 3'te görülmedi. Oturum 2 örneği: `01 89 03 E8 03 E8 00 00`

| Byte | Alan Adı | Durum | Gözlem |
| --- | --- | --- | --- |
| 0–7 | Bilinmiyor | ❌ BİLİNMİYOR | Oturum 2: `0x03E8` (1000) iki kez; E002 ile multiplex paylaşımı gözlemlendi. |

---

### `0x0000E005` — BİLİNMİYOR

Direction: `BMS → AKS` | DLC: 8 | Status: **❌ BİLİNMİYOR**

Oturum 3'te gözlemlendi, TÜM frame'lerde SABİT: `01 C5 05 DC 00 0B 71 B0`

| Byte | Alan Adı | Durum | Gözlem |
| --- | --- | --- | --- |
| 0–1 | Bilinmiyor | ❌ BİLİNMİYOR | `01 C5` = 453 — charger current setpoint ile AYNI değer (0x1806E5F4 byte[2:3]). Tesadüf mü, ayna mı, bilinmiyor. |
| 2–3 | Bilinmiyor | ❌ BİLİNMİYOR | `05 DC` = 1500 — yuvarlak değer, olası limit parametresi. |
| 4–5 | Bilinmiyor | ❌ BİLİNMİYOR | `00 0B` = 11 |
| 6–7 | Bilinmiyor | ❌ BİLİNMİYOR | `71 B0` = 29104 |

---

### `0x0000E032` — BİLİNMİYOR (Reserved/Heartbeat Adayı)

Direction: `BMS → AKS` | DLC: 8 | Status: **❌ BİLİNMİYOR**

Oturum 3'te görülmedi. Oturum 2'de tüm payload sıfır (`00 00 00 00 00 00 00 00`).

---

### `0x0000E033` — BİLİNMİYOR (Reserved/Heartbeat Adayı)

Direction: `BMS → AKS` | DLC: 8 | Status: **❌ BİLİNMİYOR**

Oturum 3'te görülmedi. Oturum 2'de tüm payload sıfır (`00 00 00 00 00 00 00 00`).

---

### `0x000` — Standart Frame (Reserved/Heartbeat Adayı)

Direction: `Bilinmiyor` | 11-bit STD | Status: **❌ BİLİNMİYOR**

Oturum 3'te görülmedi. Önceki oturumlarda tüm payload sıfır. Firmware tarafından işlenmiyor.

---

## Doğrulama Özet Tablosu

| CAN ID | Tablo Durumu | Firmware'de Parse | TelemetryData'ya Yazılıyor | Karar Mantığına Bağlı |
| --- | --- | --- | --- | --- |
| 0xE000 byte[0:1] Current | ✅ DOĞRULANDI | ✅ parseLbBmsE000 | ✅ TEL_bmsCurrentCentiA | ✅ isCurrentWarning/Critical ← hasWarning/CriticalCondition (şarj 11/13 A, deşarj 9/15 A — CONFIG, ekip onayı bekliyor) |
| 0xE000 byte[2:3] Voltage | ✅ DOĞRULANDI | ✅ parseLbBmsE000 | ✅ TEL_bmsPackVoltageDeciV | ✅ checkPackVoltageFault + VcuLogic |
| 0xE000 byte[4:5] SoC 1 | ✅ DOĞRULANDI | ✅ parseLbBmsE000 | ✅ TEL_bmsSocHundredths | ❌ (gösterim, karar dışı) |
| 0xE000 byte[6:7] SoC 2 | ✅ DOĞRULANDI | ✅ parseLbBmsE000 | ✅ TEL_bmsSoc2Hundredths | ❌ (gösterim/araştırma, karar dışı) |
| 0xE001 byte[6] Temp1 | ✅ DOĞRULANDI | ✅ parseLbBmsE001 | ✅ TEL_bmsTempHighestC (max seçimi) | ✅ isTempWarning/Critical ← hasWarning/CriticalCondition (55/70 °C) |
| 0xE001 byte[7] Temp2 | ✅ DOĞRULANDI | ✅ parseLbBmsE001 | ✅ TEL_bmsTempLowestC (min seçimi) | ❌ (yalnız gösterim; karar max kanaldan) |
| 0xE001 byte[0:5] | ❌ BİLİNMİYOR | ❌ | ❌ | ❌ |
| 0x1806E5F4 byte[0:1] | ✅ DOĞRULANDI | ✅ parseCharger | ✅ ChargerCommand | ❌ (gözlem amaçlı) |
| 0x1806E5F4 byte[2:3] | ✅ DOĞRULANDI | ✅ parseCharger | ✅ ChargerCommand | ❌ (gözlem amaçlı) |
| 0xE002–E005, E032, E033, 0x000 | ❌ BİLİNMİYOR | ❌ (stub) | ❌ | ❌ |

---

## Sonraki Adımlar

1. **E001 byte[0:5] çözümü:** 0x8000 civarındaki ofsette analog kanal deseni — olası
   bireysel hücre voltajı (offset-binary?). Tek hücreye yük uygulayarak korelasyon testi.
2. **E002–E005 çözümü:** Diagnostic sniffer modu ile çapraz gözlem; LiBAL c-BMS CREATOR ile
   PeakCAN doğrulama.
3. **Bitrate teyidi:** 500 kbps çalışıyor (frame'ler geliyor); Solion föyü "standart 125 kbps"
   diyor ama bu farklı bir yapılandırma olabilir.
4. **SoC 2 kullanımı:** SoC 1 vs SoC 2 arasındaki 0.37% farkın anlamı araştırılacak (farklı
   algoritma? farklı hücre grubu?).
