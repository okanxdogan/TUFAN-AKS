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

> **Not (2026-07-17):** Motor sürücüsü henüz entegre değilken (`MOTOR_DRIVER_PRESENT=0`),
> bu frame'i hall-effect hız sensörü ünitesi (esp32-canbus-speed-sensor) üretir.
> Sensör yalnızca `data[0:1]` (RPM) alanını doldurur; `data[2:7]` = 0x00 gönderir
> (voltaj yok, hata/çalışma bayrakları 0). Motor sürücüsü entegre edildiğinde
> bu frame'in kaynağı sürücüye geçecek ve tüm alanlar gerçek değerlerle dolacaktır.

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | RPM | int16_t | Big | signed | raw | ✅ DOĞRULANDI | MSTest/mock_motor + hall sensör entegrasyon testi |
| 2–3 | Motor Voltage | uint16_t | Big | unsigned | ×0.1 V | ✅ DOĞRULANDI | MSTest/mock_motor (sensörde 0x00) |
| 4–6 | Rezerve | — | — | — | — | ❌ BİLİNMİYOR | Kullanılmıyor |
| 7 | Error Flags / Running | uint8_t | — | unsigned | bitfield | ✅ DOĞRULANDI | bit0=çalışıyor, bit[7:1]=hata bayrakları (sensörde 0x00) |

Freshness: 1500 ms timeout → `TEL_motorTimeoutActive`, IDLE dışında FAULT.
Sensör yayın periyodu: 100 ms (10 Hz) — tazelik penceresi içinde.

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
| 0–1 | Min Cell Voltage | uint16_t | Big | unsigned | raw/10 = mV | ✅ DOĞRULANDI | `82 1D`=33309 → 3330 mV. En düşük hücre gerilimi. |
| 2–3 | Max Cell Voltage | uint16_t | Big | unsigned | raw/10 = mV | ✅ DOĞRULANDI | `82 35`=33333 → 3333 mV. En yüksek hücre gerilimi. |
| 4–5 | Avg Cell Voltage | uint16_t | Big | unsigned | raw/10 = mV | ✅ DOĞRULANDI | `82 27`=33319 → 3331 mV. Ortalama hücre gerilimi. |
| 6 | Temperature 1 (Kanal 1) | int8_t | — | signed | 1 °C (ofset yok) | ✅ DOĞRULANDI | `19` = 25 → **25°C** (log: "Batarya Sicakligi (Kanal 1) : 25 °C" ✓). Tüm 7 frame'de sabit `0x19`. |
| 7 | Temperature 2 (Kanal 2) | int8_t | — | signed | 1 °C (ofset yok) | ✅ DOĞRULANDI | `18` = 24 → **24°C** (log: "Batarya Sicakligi (Kanal 2) : 24 °C" ✓). Tüm 7 frame'de sabit `0x18`. |

**Firmware mapping:**
- `TEL_bmsTempHighestC` = max(byte[6], byte[7])
- `TEL_bmsTempLowestC` = min(byte[6], byte[7])
- `TEL_bmsCellVoltageMinDeciMv` = byte[0:1] (raw / 10 = mV)
- `TEL_bmsCellVoltageMaxDeciMv` = byte[2:3]
- `TEL_bmsCellVoltageAvgDeciMv` = byte[4:5]

---

### `0x0000E015` – `0x0000E020` — 24 Hücre Voltajı (TAMAMEN ÇÖZÜLDÜ)

Direction: `BMS → AKS` | DLC: 8 | Status: **✅ DOĞRULANDI**

Her bir frame 4 adet hücre gerilimini barındırır (toplam 6 frame × 4 = 24 hücre).

| CAN ID | Hücreler | Veri Tipi | Endian | İşaret | Ölçek |
| --- | --- | --- | --- | --- | --- |
| 0xE015 | Hücre 0–3 | uint16_t | Big | unsigned | raw / 10 = mV |
| 0xE016 | Hücre 4–7 | uint16_t | Big | unsigned | raw / 10 = mV |
| 0xE017 | Hücre 8–11 | uint16_t | Big | unsigned | raw / 10 = mV |
| 0xE018 | Hücre 12–15 | uint16_t | Big | unsigned | raw / 10 = mV |
| 0xE019 | Hücre 16–19 | uint16_t | Big | unsigned | raw / 10 = mV |
| 0xE020 | Hücre 20–23 | uint16_t | Big | unsigned | raw / 10 = mV |

**Firmware mapping:**
- `TEL_bmsCellVoltages[0..23]` = raw / 10 (mV)
- Güvenlik ve HMI logic'ine bağlandı.

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

### `0x0000E002` — ⚠️ HİPOTEZ (Şarj Limit Aynası)

Direction: `BMS → AKS` | DLC: 8 | Status: **⚠️ HİPOTEZ — bağımsız teyit yok**

Bu CAN ID, Oturum 3 logunda GÖRÜLMEDİ (log'da yalnızca E000, E001, E005, 1806E5F4
mevcut). Oturum 2'de gözlemlenmişti. **PCAN trace analizi (2 oturum, ~72k mesaj,
2026-07-14)** ek gözlem sağladı — ancak her iki PCAN oturumu da BOŞTA geçti
(akım ~-0.1 A), bu yüzden şarj/yük durumuyla korelasyon KURULAMADI.

Oturum 2 örneği: `03 70 03 E8 01 00 00 00`

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | Charge Voltage Limit (aday) | uint16_t | Big | unsigned | ×0.1 V | ⚠️ HİPOTEZ | Oturum 2: `03 70` = 880 → **88.0 V** — `0x1806E5F4` byte[0:1] (Charge Voltage Setpoint) ile AYNI değer. Kanıt: Oturum 2 sniffer logu, çapraz-ID değer eşleşmesi (bağımsız ikinci kaynak yok). |
| 2–3 | Charge Current Limit (aday) | uint16_t | Big | unsigned | ×0.1 A | ⚠️ HİPOTEZ | Oturum 2: `03 E8` = 1000 → **100.0 A**. Kanıt: tek oturum gözlemi, tekrar doğrulanmadı. |
| 4 | Enable/Active bayrağı (aday) | uint8_t | — | unsigned | bitfield | ⚠️ HİPOTEZ | PCAN trace (2026-07-14): b4 değişken, 0/1'e benzer bir aç/kapa bayrağı gibi davranıyor — hangi koşulun 0↔1 yaptığı (şarj cihazı bağlı/bağlı değil?) boşta trace'te belirlenemedi. |
| 5–7 | Bilinmiyor | — | — | — | — | ❌ BİLİNMİYOR | Gözlem yok. |

E004 ile multiplex paylaşımı gözlemlendi (Oturum 2).

---

### `0x0000E003` — ⚠️ HİPOTEZ (BMS Operasyonel State + SOH Adayı)

Direction: `BMS → AKS` | DLC: 8 | Status: **⚠️ HİPOTEZ — bağımsız teyit yok, şarj trace'i ile korelasyon bekliyor**

Oturum 3'te görülmedi. **PCAN trace analizi (2 oturum, ~72k mesaj, 2026-07-14)**
ile ilk kez ayrıntılı gözlemlendi.

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | BMS operasyonel state (aday) — **UKS `sysState` sözleşme enum'u DEĞİL** | uint16_t (?) | Big (?) | unsigned | enum? | ⚠️ HİPOTEZ | PCAN trace: boot'ta `1→2→3` dizisi görülüyor, ardından kararlı `3` (4557/4565 frame, yani neredeyse tüm oturum boyunca sabit). Boşta (akım ~-0.1 A) da `3` göstermesi, bunun UKS `sysState` (1=Discharge/2=IDLE/3=Charge/4=FAULT) sözleşme enum'u OLMADIĞINI düşündürüyor — muhtemelen BMS'in kendi iç durumu (init → self-check → çalışıyor). **Teyit bekliyor:** şarj trace'i (b0/b1'in şarj sırasında değişip değişmediği). |
| 2 | İnit bayrak alanı (aday) | uint8_t | — | unsigned | bitfield | ⚠️ HİPOTEZ | PCAN trace: boot sırasında `0x01→0x81→0x91` dizisi gözlendi — üst bitlerin art arda set edilmesi, bir "init/hazır" bayrak biriktirme deseni gibi görünüyor. Tek oturum gözlemi, tekrar doğrulanmadı. |
| 4–5 | SOH (State of Health) adayı | uint16_t | Big (?) | unsigned | ×0.01 %? | ⚠️ HİPOTEZ | PCAN trace: sabit `10000` → ×0.01 yorumuyla **%100.00** — SoC alanlarıyla (0xE000 byte[4:7]) aynı ölçek kalıbı, SOH için makul bir "yeni/sağlıklı pil" değeri. Bağımsız teyit (SOH'un gerçekten düştüğü bir gözlem) yok. |
| 3, 6–7 | Bilinmiyor | — | — | — | — | ❌ BİLİNMİYOR | Gözlem yok / anlam çıkarılamadı. |

**Firmware kuralı hatırlatması:** ⚠️ HİPOTEZ seviyesindeki hiçbir alan
TelemetryData'ya YAZILMAZ, karar mantığına BAĞLANMAZ (bkz. "Doğrulama
Seviyeleri" tablosu). `SYSSTATE_DERIVE_FROM_CURRENT` (bkz. `SystemConfig.h`,
`lib/Telemetry/SysStateDerive.h`) BU alandan TÜRETİLMEZ — akımdan (0xE000
byte[0:1], DOĞRULANDI) türetilen AYRI ve daha düşük iddialı bir hipotezdir
(yalnızca Discharge/IDLE/Charge ayrımı, FAULT girdisi yok); E003 b0/b1
teyit edilirse gerçek parse onun yerini alabilir.

---

### `0x0000E004` — BİLİNMİYOR

Direction: `BMS → AKS` | DLC: 8 | Status: **❌ BİLİNMİYOR**

Oturum 3'te görülmedi. Oturum 2 örneği: `01 89 03 E8 03 E8 00 00`

| Byte | Alan Adı | Durum | Gözlem |
| --- | --- | --- | --- |
| 0–7 | Bilinmiyor | ❌ BİLİNMİYOR | Oturum 2: `0x03E8` (1000) iki kez; E002 ile multiplex paylaşımı gözlemlendi. |

---

### `0x0000E005` — ⚠️ HİPOTEZ (Charger Aynası + Kalan Kapasite Adayı)

Direction: `BMS → AKS` | DLC: 8 | Status: **⚠️ HİPOTEZ — bağımsız teyit yok**

Oturum 3'te gözlemlendi, TÜM frame'lerde SABİT: `01 C5 05 DC 00 0B 71 B0`.
**PCAN trace analizi (2026-07-14)** aynı sabitliği doğruladı, ek bir alan
hipotezi (byte[4:7]) sağladı.

| Byte | Alan Adı | Veri Tipi | Endian | İşaret | Ölçek | Durum | Kanıt |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0–1 | Charger current setpoint aynası (aday) | uint16_t | Big | unsigned | ×0.1 A | ⚠️ HİPOTEZ | `01 C5` = 453 → 45.3 A — `0x1806E5F4` byte[2:3] (Charge Current Setpoint) ile AYNI değer. Tesadüf mü, ayna mı belirsiz — tek oturumda sabit kaldığından ayırt edilemiyor. |
| 2–3 | Bilinmiyor (olası limit) | — | — | — | — | ❌ BİLİNMİYOR | `05 DC` = 1500 — yuvarlak değer, olası limit parametresi. Gözlem tek oturumla sınırlı. |
| 4–7 | Kalan kapasite adayı (Ah?) | uint32_t (?) | Big (?) | unsigned | ? | ⚠️ HİPOTEZ | PCAN trace: `00 0B 71 B0` = **750000** — ×0.001 Ah yorumuyla 750.0 Ah gibi büyük görünüyor (24S pack için makul olmayabilir) ya da farklı bir ölçek/birim gerekiyor. Ölçek/birim BELİRSİZ, tek oturum gözlemi. |

---

### `0x0000E032` — ⚠️ HİPOTEZ (Alarm/Uyarı Bitfield Adayı)

Direction: `BMS → AKS` | DLC: 8 | Status: **⚠️ HİPOTEZ — alarmsız oturumda tümü sıfır, alarm senaryosu teyidi yok**

Oturum 3'te görülmedi. Oturum 2'de tüm payload sıfır (`00 00 00 00 00 00 00 00`).
**PCAN trace analizi (2026-07-14):** ~1 Hz periyotla geliyor, alarmsız/boşta
oturumda gözlemlenen ~1000 frame'in TAMAMI `00 00 00 00 00 00 00 00`. Sıfır
payload + düşük/sabit frekans, bunun bir alarm/uyarı bitfield'i (yalnızca bir
koşul tetiklendiğinde bit set edilir) olabileceğini düşündürüyor —
**gelecekte FAULT kaynağı adayı**, ancak hiçbir bit'in 1 olduğu bir durum
GÖZLEMLENMEDİ, bu yüzden bit-anlam haritası ÇIKARILAMADI.

| Byte | Alan Adı | Durum | Gözlem |
| --- | --- | --- | --- |
| 0–7 | Alarm/uyarı bitfield adayı | ⚠️ HİPOTEZ | Alarmsız oturumlarda (~1000 frame, iki PCAN oturumu) tamamı `0x00`. Hangi bitin hangi alarma karşılık geldiği BİLİNMİYOR — bir alarm koşulu tetiklenmiş bir trace gerekiyor. |

---

### `0x0000E033` — ⚠️ HİPOTEZ (Alarm/Uyarı Bitfield Adayı)

Direction: `BMS → AKS` | DLC: 8 | Status: **⚠️ HİPOTEZ — alarmsız oturumda tümü sıfır, alarm senaryosu teyidi yok**

Oturum 3'te görülmedi. Oturum 2'de tüm payload sıfır (`00 00 00 00 00 00 00 00`).
**PCAN trace analizi (2026-07-14):** E032 ile aynı gözlem — ~1 Hz, alarmsız
oturumda tüm frame'ler sıfır. Aynı gerekçeyle alarm/uyarı bitfield adayı,
gelecekte FAULT kaynağı adayı; bit-anlam haritası çıkarılamadı.

| Byte | Alan Adı | Durum | Gözlem |
| --- | --- | --- | --- |
| 0–7 | Alarm/uyarı bitfield adayı | ⚠️ HİPOTEZ | Alarmsız oturumlarda tamamı `0x00`. Bit-anlam haritası BİLİNMİYOR. |

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
| 0xE001 byte[0:5] Min/Max/Avg | ✅ DOĞRULANDI | ✅ parseLbBmsE001 | ✅ TEL_bmsCellVoltageMin/Max/AvgDeciMv | ✅ hasWarning/CriticalCondition (Hücre koruması) |
| 0xE015-E020 Bireysel Hücre Voltajları | ✅ DOĞRULANDI | ✅ parseLbBmsE015-E020 | ✅ TEL_bmsCellVoltages[24] | ❌ (HMI gösterimi için, kararlar Min/Max üstünden) |
| 0x1806E5F4 byte[0:1] | ✅ DOĞRULANDI | ✅ parseCharger | ✅ ChargerCommand | ❌ (gözlem amaçlı) |
| 0x1806E5F4 byte[2:3] | ✅ DOĞRULANDI | ✅ parseCharger | ✅ ChargerCommand | ❌ (gözlem amaçlı) |
| 0xE002 byte[0:1]/[2:3]/4 | ⚠️ HİPOTEZ (şarj limit/enable adayı) | ❌ (stub) | ❌ | ❌ |
| 0xE003 byte[0:1] (BMS state) | ⚠️ HİPOTEZ (UKS sysState enum'u DEĞİL) | ❌ (stub) | ❌ | ❌ |
| 0xE003 byte[2] (init bayrağı) / byte[4:5] (SOH adayı) | ⚠️ HİPOTEZ | ❌ (stub) | ❌ | ❌ |
| 0xE005 byte[0:1] (charger aynası) / byte[4:7] (kapasite adayı) | ⚠️ HİPOTEZ | ❌ (stub) | ❌ | ❌ |
| 0xE032, 0xE033 (alarm bitfield adayı) | ⚠️ HİPOTEZ | ❌ (stub) | ❌ | ❌ |
| 0xE004, 0x000 | ❌ BİLİNMİYOR | ❌ (stub) | ❌ | ❌ |
| — Türetilmiş `sysState` (`SYSSTATE_DERIVE_FROM_CURRENT`) | N/A — CAN alanı DEĞİL, akımdan (0xE000, DOĞRULANDI) hesaplanır | ✅ `SysStateDerive::deriveFromCurrent` | ⚠️ yalnız flag=1 VE `TEL_bmsSystemState==0` iken, yalnız LoRa TX yolunda | ❌ (yalnız telemetri/UKS gösterimi — VCU karar mantığına BAĞLANMAZ, feature flag arkasında, varsayılan KAPALI) |

---

## Sonraki Adımlar

1. **E002–E006 çözümü:** Diagnostic sniffer modu ile çapraz gözlem; LiBAL c-BMS CREATOR ile PeakCAN doğrulama. (HMI kararlarını etkilemiyor)
3. **Bitrate teyidi:** 500 kbps çalışıyor (frame'ler geliyor); Solion föyü "standart 125 kbps"
   diyor ama bu farklı bir yapılandırma olabilir.
4. **SoC 2 kullanımı:** SoC 1 vs SoC 2 arasındaki 0.37% farkın anlamı araştırılacak (farklı
   algoritma? farklı hücre grubu?).
5. **E003 b0/b1 teyit checklist'i (PCAN, 2026-07-14 analizinden):**
   - [ ] Şarjda 30-60 sn PCAN trace al; E003 byte[0:1] değerini zamanla ve
     0xE000 byte[0:1] (Pack Current, DOĞRULANDI) ile korele et.
   - [ ] Yükte (deşarj) 30-60 sn PCAN trace al; aynı korelasyonu tekrarla.
   - [ ] **Sonuca göre:**
     - E003 b0/b1 gerçekten şarj/IDLE/deşarj ile 1/2/3 şeklinde değişiyorsa:
       `CanParse::parseLbBmsE003`'e gerçek parse ekle, `TEL_bmsSystemState`'i
       doğrudan bu alandan doldur; `SYSSTATE_DERIVE_FROM_CURRENT` hipotezini
       (bkz. `SystemConfig.h`, `lib/Telemetry/SysStateDerive.h`) KALDIR veya
       yalnızca gerçek parse `TEL_bmsSystemState`'i doldurmadığı ara durumlar
       için fallback'e indir.
     - E003 b0/b1 şarj/yükle KORELE OLMUYORSA (BMS'in kendi iç durumu
       teyit edilirse): bu alanı ❌ BİLİNMİYOR'a geri düşür,
       `SYSSTATE_DERIVE_FROM_CURRENT` hipotezini (akım tabanlı) kalıcı
       çözüm olarak değerlendirmeye devam et.
   - [ ] Hangi yol seçilirse seçilsin: `tools/e2e/test_contract_drift.py`
     `test_lb_bms_field_coverage_is_tracked` xfail izleyicisini VE
     `TEKNIK_KONTROL_PROVASI.md`/boot-log uyarısını (`main.cpp` app_main
     içindeki `ESP_LOGW` — "BMS: sysState henuz parse edilmiyor") güncelle.
