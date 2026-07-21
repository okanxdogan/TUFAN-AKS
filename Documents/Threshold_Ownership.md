# Batarya Eşik Sahipliği (Threshold Ownership)

Repoda birbirinden habersiz **iki ayrı batarya eşik seti** var:

1. `include/SystemConfig.h` — **pack-bazlı** (deciV / centi-A / °C), `src/VcuLogic.h`
   (`hasWarningCondition` / `hasCriticalCondition`) ve `lib/CanManager/CanManager.cpp`
   (`checkPackVoltageFault` üzerinden) tarafından tüketiliyor. Bu set VCU durum
   makinesini (FAULT / kontaktör) besler.
2. `lib/BmsAlgo/BmsAlgo.h` — **hücre-bazlı** (mV / °C), `computePack()` tarafından
   tüketiliyor. Çıktısı (`BmsComputed`) şu an yalnızca Nextion HMI'ye
   (`BmsNextionPacket.cpp`) gidiyor; VCU kararına bağlı DEĞİL.

Bu doküman her iki setin güncel değerlerini, tüketicilerini, bağlı oldukları
sinyalleri ve hangilerinin fiilen ölü/canlı olduğunu kaydeder. Değerler
koddan (aşağıdaki satır referanslarıyla) okunmuştur, ezberden yazılmamıştır.

---

## 1. Otorite Kuralı

**Araç güvenliği kararları (FAULT / kontaktör açma) için `SystemConfig.h`
OTORİTERDİR.** `BmsAlgo.h` eşikleri yalnızca gösterim/uyarı katmanıdır
(Nextion ekranındaki renk/uyarı seviyesi). İki set çelişirse **`SystemConfig.h`
kazanır** — `BmsAlgo.h` tarafındaki bir eşik aşılsa bile VCU FAULT'a geçmez;
tersine `SystemConfig.h` eşiği aşıldığında VCU FAULT'a geçer, ekran o anda
farklı bir renk gösteriyor olsa bile.

---

## 2. `SystemConfig.h` — Pack-Bazlı Eşikler (VCU Güvenlik Kararı OTORİTESİ)

Kaynak: `include/SystemConfig.h`, "Phase 2 Safety Thresholds" bölümü (satır 156 vd.).

| Sabit | Satır | Değer | Birim | Tüketici (fonksiyon) | Bağlı Sinyal | Sinyal Durumu | Karar Yolunda CANLI mı? |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `BMS_WARN_MIN_PACK_VOLTAGE_DECI_V` | 176 | 720 (72.0 V) | deciV | `VcuLogic::hasWarningCondition` | `TEL_bmsPackVoltageDeciV` | ✅ DOĞRULANDI | ✅ CANLI |
| `BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V` | 177 | 600 (60.0 V) | deciV | `VcuLogic::hasCriticalCondition` + `CanManager::handleLbBmsE000` → `CanParse::checkPackVoltageFault` | `TEL_bmsPackVoltageDeciV` | ✅ DOĞRULANDI | ✅ CANLI (iki bağımsız yol) |
| `BMS_WARN_MAX_PACK_VOLTAGE_DECI_V` | 178 | 852 (85.2 V) | deciV | `VcuLogic::hasWarningCondition` | `TEL_bmsPackVoltageDeciV` | ✅ DOĞRULANDI | ✅ CANLI |
| `BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V` | 179 | 876 (87.6 V) | deciV | `VcuLogic::hasCriticalCondition` + `CanManager::handleLbBmsE000` → `CanParse::checkPackVoltageFault` | `TEL_bmsPackVoltageDeciV` | ✅ DOĞRULANDI | ✅ CANLI (iki bağımsız yol) |
| `BMS_WARN_MAX_TEMP_C` | 309 | 55 | °C | `VcuLogic::isTempWarning` ← `hasWarningCondition` (>= semantiği; READY girişini de bloklar) | `TEL_bmsTempHighestC` | ✅ DOĞRULANDI (0xE001 byte[6:7], max(temp1,temp2)) | ✅ CANLI |
| `BMS_CRITICAL_MAX_TEMP_C` | 310 | 70 | °C | `VcuLogic::isTempCritical` ← `hasCriticalCondition` (>= semantiği; READY/DRIVE'da otomatik FAULT, reset interlock'unu ve READY girişini bloklar) | `TEL_bmsTempHighestC` | ✅ DOĞRULANDI (0xE001 byte[6:7]) | ✅ CANLI |
| `BMS_WARN_MAX_CHARGE_CURRENT_CENTI_A` | 328 | 1100 (11.0 A) — CONFIG, ekip onayı bekliyor | centi-A | `VcuLogic::isCurrentWarning` ← `hasWarningCondition` | `TEL_bmsCurrentCentiA` | ✅ DOĞRULANDI (0xE000 byte[0:1] + saha gözlemi: şarjda +9.9 A) | ✅ CANLI |
| `BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A` | 329 | 1300 (13.0 A) — CONFIG, ekip onayı bekliyor | centi-A | `VcuLogic::isCurrentCritical` ← `hasCriticalCondition` (READY girişini ve reset'i de bloklar) | `TEL_bmsCurrentCentiA` | ✅ DOĞRULANDI | ✅ CANLI |
| `BMS_WARN_MAX_DISCHARGE_CURRENT_CENTI_A` | 330 | 900 (9.0 A) | centi-A | `VcuLogic::isCurrentWarning` ← `hasWarningCondition` | `TEL_bmsCurrentCentiA` | ✅ DOĞRULANDI (deşarjda −0.1…−1.5 A gözlendi) | ✅ CANLI |
| `BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A` | 331 | 1500 (15.0 A) | centi-A | `VcuLogic::isCurrentCritical` ← `hasCriticalCondition` | `TEL_bmsCurrentCentiA` | ✅ DOĞRULANDI | ✅ CANLI |
> **ŞARTNAME KİLİDİ (sıcaklık, Bölüm 3 6.e.iii):** `BMS_WARN_MAX_TEMP_C=55` /
> `BMS_CRITICAL_MAX_TEMP_C=70` şartname idealinin birebir kendisidir ve
> derleme-zamanı kilitlidir: `SystemConfig.h` içindeki `static_assert`
> (CRIT − WARN == 15 °C sabit aralık) + `VcuLogic.h` içindeki `static_assert`
> (BmsAlgo.h HMI eşikleri `BMS_TEMP_OVERTEMP_WARN_C/CRIT_C` == SystemConfig
> değerleri). Ayrıca 55 °C uyarısı, `RELAY_ROLES_ASSIGNED=1` iken uyarı
> flaşörüne bağlıdır (şartname 6.e.ii — bkz.
> `Documents/RELAY_CHANNEL_TABLE.md` ve `VcuLogic.h::flasherDesiredState`,
> `FLASHER_HYSTERESIS_C=2` histerezisli).

> **DÜZELTME (2026-07-13):** Bu tabloda önceden `BMS_CRITICAL_MIN_/MAX_CELL_VOLTAGE_MV`
> (`SystemConfig.h`) diye iki satır vardı, "CANLI" ve `VcuLogic::hasCriticalCondition`
> tüketicisi olarak işaretliydi — bu **yanlıştı**. `VcuLogic::hasCriticalCondition`
> hücre voltajı için bu makroları HİÇ kullanmıyordu; gerçek eşik seti her zaman
> `BmsAlgo.h`'deki `BMS_CELL_UNDERVOLT_CRIT_MV`/`BMS_CELL_OVERVOLT_CRIT_MV`
> idi (bkz. bölüm 3 tablosu). `SystemConfig.h`'deki iki makro aynı değerlerle
> (2500/3650 mV) hiçbir yerden referans edilmeyen kullanılmayan bir kopyaydı;
> grep ile doğrulanıp SİLİNDİ. Hücre voltajı CRITICAL eşiğinin TEK doğruluk
> kaynağı `BmsAlgo.h`'dir (bölüm 3). NOT (2026-07-13 GÜVENLİK-EŞİĞİ
> DÜZELTMESİ): `VcuLogic::hasCriticalCondition`/`hasWarningCondition` bu
> makroların **mV** değil, bunlardan türetilen **`_DECI_MV`** eşdeğerini
> kullanır (`TEL_bmsCellVoltageMin/MaxDeciMv` alanının ölçeğiyle uyumlu —
> bkz. bölüm 3 altındaki "GÜVENLİK-EŞİĞİ DÜZELTMESİ" alt bölümü).

> **Birim kararı (G5):** Akım için tek birim **centi-Amper (0.01 A)**'dir — parser
> çıktısı (`TEL_bmsCurrentCentiA` = ham 0.1A × 10), eşikler ve `isCurrentWarning/
> isCurrentCritical` hepsi bu ölçekte hizalıdır; eski "centi-mA" yorumu eşikleri
> 1000× yüksek tutup aşırı akım korumasını kör bırakıyordu.
>
> **Saha doğrulaması ve kalibrasyon (Temmuz 2026):** Akım sinyali sahada
> doğrulandı — şarjda +9.9 A, deşarjda gaza bağlı −0.1…−1.5 A; işaret
> konvansiyonu + şarj / − deşarj (`BmsModel.h` ile uyumlu). Şarj eşikleri buna
> göre 11/13 A'e kalibre edildi (eski 0.9/1.0 A gerçek şarj akımının çok
> altındaydı ve her şarjda yanlış FAULT üretirdi). Nihai değerler BMS/şarj
> cihazı spec'iyle **ekip onayı bekliyor** (CONFIG).

Freshness/timeout eşikleri (aynı dosya, "CAN Freshness Thresholds" bölümü, satır 207 vd.)
da pack/paket seviyesinde VCU kararını besler:

| Sabit | Satır | Değer | Tüketici | Sinyal Durumu | Karar Yolunda CANLI mı? |
| --- | --- | --- | --- | --- | --- |
| `CAN_MOTOR_STATUS_TIMEOUT_MS` | 208 | 500 ms | `CanManager::updateMotorStatusValidity` → `TEL_motorTimeoutActive` → `VcuLogic::hasCriticalCondition` (IDLE dışında critical) | ✅ DOĞRULANDI (frame varlığı, ölçeğe bağlı değil) | ✅ CANLI |
| `CAN_BMS_STATUS_TIMEOUT_MS` | 209 | 500 ms | `CanManager::updateBmsValidity` → `bms_evaluate_freshness` (G12: E000 **ve** E001 ID bazında ayrı izlenir; biri bayatlarsa timeout) → `TEL_bmsTimeoutActive` → `VcuLogic::hasCriticalCondition` (IDLE dışında critical) | ✅ DOĞRULANDI (E000+E001 frame varlığı) | ✅ CANLI |
| `CAN_CHARGER_TIMEOUT_MS` | 213 | 2000 ms | Charger setpoint'lerini "bayat" işaretler; freshness sonucu (`CAN_chargerValid`) `getTelemetryData()` üzerinden `TEL_chargerActive` olarak yayınlanır → `VcuLogic` S1/S2 mod anahtarlaması girdisi (`RELAY_ROLES_ASSIGNED=1`, şartname 8.2.a.iii: charger aktifken S1 kapalı + READY reddi) | ✅ DOĞRULANDI | ⚠️ S1/S2 mod girdisi, yine `CAN_Event`/FAULT ÜRETMEZ (bilinçli tasarım, opsiyonel akış) |

---

## 3. `lib/BmsAlgo/BmsAlgo.h` — Hücre-Bazlı Eşikler (Gösterim/Uyarı Katmanı)

Kaynak: `lib/BmsAlgo/BmsAlgo.h`. Tüketici tek fonksiyon: `computePack()`
(`lib/BmsAlgo/BmsAlgo.cpp:30-108`). Çıktı (`BmsComputed.warningLevel`) yalnızca
`buildBmsNextionCommands()` üzerinden Nextion ekranına (`warn` alanı) gidiyor —
VCU karar mantığına hiç girmiyor.

| Sabit | Satır | Değer | Birim | Rol |
| --- | --- | --- | --- | --- |
| `BMS_BALANCE_THRESHOLD_MV` | 25 | 50 | mV | Pasif dengeleme tetik bandı (delta eşiği) — kimyadan bağımsız, DEĞİŞMEDİ |
| `BMS_BALANCE_TOP_MARGIN_MV` | 30 | 5 | mV | Dengeleme marjı (en yüksek hücreye yakın diğerleri) — kimyadan bağımsız, DEĞİŞMEDİ |
| `BMS_SOC_EMPTY_MV` | 51 | 2500 | mV | SoC %0 referansı — LiFePO4 spec min (2.50 V) |
| `BMS_SOC_FULL_MV` | 52 | 3650 | mV | SoC %100 referansı — LiFePO4 spec maks (3.65 V) |
| `BMS_CELL_UNDERVOLT_CRIT_MV` | 66 | 2500 | mV | Hücre CRITICAL alt sınır — LiFePO4 spec min |
| `BMS_CELL_OVERVOLT_CRIT_MV` | 67 | 3650 | mV | Hücre CRITICAL üst sınır — LiFePO4 spec maks |
| `BMS_TEMP_OVERTEMP_CRIT_C` | 75 | 70 | °C | Hücre CRITICAL sıcaklık — sistem politikasıyla (70 °C FAULT) hem değer hem semantik (>=) olarak hizalı: tam 70 °C'de ekran CRITICAL gösterirken VCU FAULT'a geçer |
| `BMS_CELL_UNDERVOLT_WARN_MV` | 75 | 2800 | mV | Hücre WARNING alt sınır — CRIT'e 300 mV marj |
| `BMS_CELL_OVERVOLT_WARN_MV` | 76 | 3550 | mV | Hücre WARNING üst sınır — CRIT'e 100 mV marj |
| `BMS_TEMP_OVERTEMP_WARN_C` | 84 | 55 | °C | Hücre WARNING sıcaklık — sistem politikasıyla (55 °C UYARI) hem değer hem semantik (>=) olarak hizalı |
| `BMS_CELL_UNDERVOLT_CRIT_DECI_MV` | 107 | 25000 | deci-mV | = yukarıdaki `_CRIT_MV` × 10 — VcuLogic.h/DeratingPolicy.h için (bkz. GÜVENLİK-EŞİĞİ DÜZELTMESİ altbölümü) |
| `BMS_CELL_OVERVOLT_CRIT_DECI_MV` | 108 | 36500 | deci-mV | = yukarıdaki `_CRIT_MV` × 10 |
| `BMS_CELL_UNDERVOLT_WARN_DECI_MV` | 109 | 28000 | deci-mV | = yukarıdaki `_WARN_MV` × 10 |
| `BMS_CELL_OVERVOLT_WARN_DECI_MV` | 110 | 35500 | deci-mV | = yukarıdaki `_WARN_MV` × 10 |

Değerler 24S LiFePO4 spec'ine (2.50–3.65 V/hücre) uyarlanmıştır — bkz. bölüm 5.

### GÜVENLİK-EŞİĞİ DÜZELTMESİ (2026-07-13) — hücre voltajı deci-mV/mV birim uyumsuzluğu

**Bulgu:** `BMS_CELL_UNDERVOLT/OVERVOLT_WARN/CRIT_MV` (yukarıdaki tablo) tek bir
sabit seti olarak İKİ farklı karşılaştırma bağlamında kullanılıyordu:

1. `BmsAlgo.cpp::computePack` — `BmsPackData::cellVoltageMv[]` ile karşılaştırır.
   Bu dizi GERÇEKTEN mV (main.cpp, `TEL_bmsCellVoltages[]`'ten kopyalar — o da
   `CanParse::parseLbBmsE015..E020`'de ham CAN byte'ının `/10`'a bölünmesiyle
   üretilir). **Bu kullanım her zaman DOĞRUYDU, DEĞİŞTİRİLMEDİ.**
2. `VcuLogic.h::hasWarningCondition/hasCriticalCondition` ve
   `DeratingPolicy.h::computeTorqueAllowPercent` — `TEL_bmsCellVoltageMin/
   MaxDeciMv` ile karşılaştırır. Bu alanlar `CanParse::parseLbBmsE001`'de HİÇ
   `/10` YAPILMADAN doğrudan yazılır — gerçekte **deci-mV** (~28000-40000).

**Önceki (hatalı) davranış:** (2) numaralı yol mV-ölçekli eşiklerle (2500-3650)
karşılaştırıyordu. Gerçekçi bir hücre voltajı (deci-mV, ör. 33500) bu
eşiklerin çok üzerinde olduğundan **overvolt WARN/CRITICAL her zaman
tetiklenir**, **undervolt WARN/CRITICAL ise neredeyse hiç tetiklenmezdi**.
`hasCriticalCondition` READY/DRIVE'da FAULT'a geçişi de tetiklediğinden
(`VcuLogic.cpp run()`), olası saha belirtisi yalnızca "BMS canlıyken araç
READY'ye giremiyor" değil, **DRIVE sırasında beklenmedik FAULT'a geçiş**
olabilirdi.

**Düzeltme:** `BmsAlgo.h`'ye yukarıdaki mV sabitlerinden TÜRETİLEN
(`× 10`) yeni `_DECI_MV` sabitleri eklendi (aynı fiziksel eşik, doğru
ölçek). `VcuLogic.h` ve `DeratingPolicy.h` artık bu `_DECI_MV` sabitlerini
kullanır. `BmsAlgo.cpp`'nin kendi kullanımı (mV sabitler, mV alan) DOKUNULMADI.
Wire formatı (`cellVMax`/`cellVMin` ×0.1 mV) ve CAN parse mantığı (`CanParse.cpp`)
DEĞİŞMEDİ — bu yalnızca dahili karşılaştırma eşiklerinin düzeltilmesidir.
CRITICAL uçları (2500/3650 mV) SystemConfig.h pack CRITICAL eşikleriyle
(600/876 deciV) hücre×24 ilişkisiyle birebir örtüşür; WARN bandı bu katmana
özgüdür (SystemConfig.h WARN eşikleriyle 1:1 eşleşmesi gerekmez).

Bu eşikler kod yolunda "ölü" değildir — `computePack()` her HMI tick'inde
çalışır ve `warningLevel`i gerçekten hesaplar. Girdi (`BmsPackData`) artık
gerçek veriyle doldurulur: hücre voltajları E015-E020'den (`TEL_bmsCellVoltages`,
DOĞRULANDI), paket sıcaklığı ise `packTempMaxC/MinC` alanlarından
(`TEL_bmsTempHighestC/LowestC`, 0xE001, DOĞRULANDI — `src/main.cpp` HMI task
doldurur). Eskiden `cellTempC[i]=0` yazıldığından ekranın sıcaklık uyarısı
HİÇ tetiklenmiyordu; `computePack` tMax/tMin'i artık hücre dizisinden DEĞİL
bu paket-seviyesi alanlardan alır (per-hücre sahte sıcaklık kopyalama yok) —
55/70 eşikleri ve >= semantiği ekranda da fiilen canlıdır.

---

## 4. Fiilen Ölü Eşikler ve Neden Önemli

**Kısmen Ölü** kategorisi BOŞALDI: hem sıcaklık hem akım eşikleri karar
mantığına bağlandı ve artık CANLI —

- Sıcaklık (`BMS_WARN_MAX_TEMP_C`, `BMS_CRITICAL_MAX_TEMP_C`):
  `VcuLogic::isTempWarning/isTempCritical` üzerinden — 55 °C ve üzeri UYARI,
  70 °C ve üzeri FAULT.
- Akım (`BMS_WARN_/CRITICAL_MAX_CHARGE_/DISCHARGE_CURRENT_CENTI_A`):
  `VcuLogic::isCurrentWarning/isCurrentCritical` üzerinden — saha
  kalibrasyonuyla (şarj 11/13 A, deşarj 9/15 A; ekip onayı bekliyor).

(Bkz. bölüm 2 tablosu.)

**Tamamen Ölü**: `SystemConfig.h`'de artık tamamen ölü hiçbir eşik kalmamıştır
— tek gerçek ölü kopya (`BMS_CRITICAL_MIN_/MAX_CELL_VOLTAGE_MV`, bkz. bölüm 2
düzeltme notu) 2026-07-13'te silindi. Hücre voltajı için karar mantığına
BAĞLI olan eşikler zaten hep `BmsAlgo.h`'de yaşıyordu (`BMS_CELL_UNDERVOLT_/
OVERVOLT_CRIT_MV`, bölüm 3) — 0xE015-E020 ve 0xE001 mesajlarının çözülmesiyle
CANLI hale gelen bunlardır, `SystemConfig.h`'deki silinen kopya DEĞİL.

**Canlı**: pack voltajı eşikleri (`TEL_bmsPackVoltageDeciV`, DOĞRULANDI),
sıcaklık eşikleri (`TEL_bmsTempHighestC`, DOĞRULANDI, 55/70 °C), akım
eşikleri (`TEL_bmsCurrentCentiA`, DOĞRULANDI, şarj 11/13 A / deşarj 9/15 A),
hücre voltaj eşikleri (`TEL_bmsCellVoltageMin/MaxDeciMv`, DOĞRULANDI)
ve motor/BMS freshness timeout'ları.

---

## 5. Bilinen Çelişki: `BmsAlgo.h` NMC Varsayımları vs LiFePO4 Paket Kimyası

> ✅ **ÇÖZÜLDÜ** (`AKS-bmsalgo-lifepo4-thresholds`, bkz. `lib/BmsAlgo/BmsAlgo.h`
> ve `lib/BmsAlgo/BmsNextionPacket.cpp`'nin güncel hali). Aşağıdaki tarihçe,
> çelişkinin neden var olduğunu ve nasıl giderildiğini kaydetmek için
> bilerek SİLİNMEDİ.

`BmsAlgo.h`'nin hücre eşikleri, bu PR'dan önce **4.2 V Li-ion (NMC) kimyası**
varsayımıyla yazılmıştı:

| BmsAlgo.h sabiti | Eski (NMC) değer | NMC'de tipik anlamı | 24S LiFePO4 gerçeği (SystemConfig.h pack spec'i) | Yeni (LiFePO4) değer |
| --- | --- | --- | --- | --- |
| `BMS_SOC_FULL_MV` | 4200 mV | Dolu hücre (NMC) | LiFePO4 hücre maksimumu **3650 mV** (3.65 V) | **3650 mV** |
| `BMS_CELL_OVERVOLT_CRIT_MV` | 4250 mV | NMC aşırı şarj | LiFePO4 spec maks zaten 3650 mV — bu eşik hiç aşılamaz aralıktaydı | **3650 mV** |
| `BMS_CELL_UNDERVOLT_CRIT_MV` | 3000 mV | NMC aşırı deşarj | LiFePO4 spec min **2500 mV** (2.50 V) — bu eşik LiFePO4 için ERKEN tetikleniyordu | **2500 mV** |
| `BMS_SOC_EMPTY_MV` | 3000 mV | Boş hücre (NMC) | LiFePO4 spec min 2500 mV | **2500 mV** |
| `BMS_CELL_UNDERVOLT_WARN_MV` | 3200 mV | NMC WARN alt | CRIT'e (yeni: 2500) 300 mV marj hedefleniyor | **2800 mV** |
| `BMS_CELL_OVERVOLT_WARN_MV` | 4150 mV | NMC WARN üst | CRIT'e (yeni: 3650) 100 mV marj hedefleniyor | **3550 mV** |

Sıcaklık (`BMS_TEMP_OVERTEMP_WARN_C`/`CRIT_C`, o dönemde 50/60°C) ve dengeleme
(`BMS_BALANCE_THRESHOLD_MV`/`TOP_MARGIN_MV`, 50/5 mV) eşikleri kimyadan
bağımsız olduğu için o PR'da DEĞİŞTİRİLMEMİŞTİ. *(Not: sıcaklık eşikleri daha
sonra sistem sıcaklık politikasıyla hizalanarak 55/70 °C yapıldı — bkz. bölüm 3.)*

Aynı NMC varsayımı üçüncü bir yerde daha vardı: `lib/BmsAlgo/BmsNextionPacket.cpp`
içindeki `cellBarFill()` fonksiyonunun **lokal** `kBarEmptyMv = 3000` /
`kBarFullMv = 4200` sabitleri — hücre bar doluluk yüzdesini aynı yanlış (NMC)
referans aralığına göre hesaplıyordu. Bu lokal kopya silindi; `cellBarFill()`
artık doğrudan `BmsAlgo.h`'deki `BMS_SOC_EMPTY_MV`/`BMS_SOC_FULL_MV`'yi
kullanıyor (tek kaynak).

Regresyon kilidi: `test/test_native_bms_algo/` (SoC haritalaması, WARN/CRITICAL
sınır semantiği, dengeleme, `cellBarFill` — bkz. `Documents/Test_Guide.md`).

---

## 6. Yeni Eşik Eklerken Karar Kuralı

- Eşik **pack-bazlı bir VCU güvenlik kararı** ise (FAULT/kontaktör tetikleyecekse)
  → `include/SystemConfig.h`'ye eklenir, `VcuLogic.h`'deki
  `hasWarningCondition`/`hasCriticalCondition`'a bağlanır.
- Eşik **hücre-bazlı bir gösterim/algoritma** ise (HMI uyarı rengi, dengeleme,
  SoC tahmini) → `lib/BmsAlgo/BmsAlgo.h`'ye eklenir, `computePack()` içinde
  kullanılır.
- Aynı fiziksel limitin iki dosyada da tutulması gerekiyorsa (ör.
  hücre_maks_mV × 24 = pack_maks_deciV), bu tutarlılık ilişkisi **her iki
  dosyada da yorumla belgelenmelidir** — sabit değerleri birbirinden bağımsız
  güncellenip sessizce ayrışmamalıdır.
