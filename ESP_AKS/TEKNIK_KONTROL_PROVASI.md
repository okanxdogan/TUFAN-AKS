# Teknik Kontrol Provası — 9.4.a.vi / 9.4.b.i–vi

Bu belge, AKS/UKS/Monitor uc bilesenli sistemin juri/teknik kontrol
denetiminde NASIL gosterilecegini, hangi komut/adimlarin izlenecegini ve
beklenen ciktinin ne oldugunu tek yerde toplar. `tools/e2e/` altindaki
otomatik dogrulayici (bkz. proje kokundeki calistiirma talimatlari) bu
belgedeki iddialarin **donanimsiz, tekrarlanabilir** kanitidir; sahada
gercek donanimla yapilacak gosterim ADIMLARI asagida verilmistir.

**Kapsam notu:** Bu belge yalniz UC REPO'NUN KAYNAK KODUNDA (yorumlar,
sabitler, testler) IZI BULUNAN sartname maddelerini detaylandirir. Asagidaki
tabloda kaynagi bulunamayan maddeler (9.4.a.vi; 9.4.b.i, ii, iv, v) acikca
"SARTNAME METNI EKLENECEK" olarak isaretlenmistir — bu satirlar UYDURULMAMISTIR,
ekip gercek sartname metnini elle doldurmalidir. Kaynagi bulunan maddeler
(9.4.b.iii, 9.4.b.vi) ve 9.2.a (tek yonluluk) tam detayla asagida verilmistir.

---

## 1. Madde madde: nasil gosterilecek / komut-adim / beklenen cikti

| Madde | Durum | Nasil gosterilecek | Komut / Adim | Beklenen cikti |
|---|---|---|---|---|
| **9.4.b.iii** | Kanitli (kaynak: `include/VehicleParams.h:5`, `src/main.cpp:648`) | Arac hizinin (km/h) telemetride ANLIK dogrulugu denetlenir. **ONKOSUL:** `VehicleParams.h` icindeki `WHEEL_DIAMETER_M` / `GEAR_RATIO` / `MOTOR_RPM_IS_WHEEL_RPM` gercek olcumlerle guncellenip `VEHICLE_PARAMS_CONFIRMED=1` yapilmis OLMALI — aksi halde boot loginda `#warning` + `ESP_LOGE` ile "ARAC PARAMETRELERI TEYITSIZ" uyarisi gorunur ve hiz verisi GECERSIZ kabul edilir. | 1) Aracı bilinen sabit bir hizda (ör. standli test / GPS referans) calistir. 2) UKS USART1 (115200 baud) ciktisindaki `CSV,...` satirinin 3. alanini (`spd_x10`) al, /10 ile km/h'ye cevir. 3) Monitor GUI'deki HIZ gostergesiyle karsilastir. | `spd_x10/10` (UKS CSV) == Monitor GUI HIZ gostergesi == referans hiz (± olcum toleransi). `VEHICLE_PARAMS_CONFIRMED=0` iken bu adim GECERSIZ sayilir (boot loginda ve Monitor pencere basliginda uyari gorunmelidir — bkz. `config.py CONFIG_CONFIRMED` benzeri, ama bu spesifik olarak AKS `VehicleParams.h` bayragidir). |
| **9.4.b.vi** | Kanitli (kaynak: `lib/OfflineBuffer/OfflineBuffer.h:8`, `src/main.cpp:499,555,615`) | 60 sn'lik RF kesintisi + toparlanma (replay) gosterimi. Detay: bolum 2 (asagida). | Bolum 2'deki adimlar. | Bolum 2'deki "beklenen cikti" satirlari. |
| 9.4.b.i | **SARTNAME METNI EKLENECEK** — repoda kaynak bulunamadi | — | — | — |
| 9.4.b.ii | **SARTNAME METNI EKLENECEK** — repoda kaynak bulunamadi | — | — | — |
| 9.4.b.iv | **SARTNAME METNI EKLENECEK** — repoda kaynak bulunamadi | — | — | — |
| 9.4.b.v | **SARTNAME METNI EKLENECEK** — repoda kaynak bulunamadi | — | — | — |
| 9.4.a.vi | **SARTNAME METNI EKLENECEK** — repoda kaynak bulunamadi | — | — | — |

> Not: 9.2.f ("izleme merkezi kaydinda kalan_enerji_Wh kolonu zorunludur",
> `include/VehicleParams.h:6`) ve 9.2.c.i (arac parametreleri tek kaynakta +
> teyitsiz-build korumasi, commit `fd157b2`) de bu teknik kontrolle iliskili
> olabilir; ekip sartname numarasini netlestirdiginde yukaridaki tabloya
> eklenmelidir.

### AÇIK İŞ — 9.2.c.ii: Lithium Balance c-BMS alan kapsamı eksik

2026-07-03 main-merge'i sırasında donanım BMS vendörünün Solion SK'dan
Lithium Balance c-BMS'e geçtiği ekipçe teyit edildi (bkz. `CanManager.cpp`,
`CanParse.cpp`, `SystemConfig.h`). **Oturum 3 (2026-07-09) itibarıyla CAN
sniffer logları (zemin gerçeği) ile `0xE000` ve `0xE001` ID'leri KESİN
olarak doğrulanmıştır**:
- **0xE000**: Pack Voltajı, Akım (centi-Amper, + şarj / − deşarj), SoC 1, SoC 2.
- **0xE001**: Temperature 1 ve Temperature 2 (byte[6:7], °C).
Bu sinyaller başarıyla okunur ve Nextion HMI + UKS'ye gönderilir. Tazelik
(freshness) her iki ID için AYRI izlenir (G12, `bms_evaluate_freshness`):
E000 veya E001'den biri kesilirse `TEL_bmsDataValid` düşer ve IDLE dışında
BMS timeout kritik sayılır.

**AÇIK İŞ (Kalan Alanlar)**: `TEL_bmsSystemState` (sistem durumu),
`TEL_bmsCellVoltageMaxDeciMv`, `TEL_bmsCellVoltageMinDeciMv` gibi alanlar
`0xE002-E005` ID'lerinde bulunuyor olabilir, ancak alan anlamları henüz
BİLİNMİYOR. Sonuç:

- `TEL_bmsSystemState` hep `0` kalır; `TelemetrySanitize::sanitizeSystemState(0)`
  bunu `4` (FAULT) yapar — **UKS ekranında BMS her zaman FAULT görünür**,
  gerçek bir arıza olmasa bile.
- Sıcaklık eşikleri (`BMS_WARN_MAX_TEMP_C`=55 / `BMS_CRITICAL_MAX_TEMP_C`=70,
  SystemConfig.h) artık `VcuLogic::isTempWarning`/`isTempCritical` üzerinden
  karar mantığına BAĞLI — 55 °C ve üzeri UYARI, 70 °C ve üzeri FAULT. Akım
  eşikleri de `isCurrentWarning`/`isCurrentCritical` üzerinden BAĞLI — saha
  gözlemiyle (şarjda +9.9 A) kalibre edildi: şarj WARN 11 A / CRITICAL 13 A,
  deşarj WARN 9 A / CRITICAL 15 A. Nihai değerler BMS/şarj cihazı spec'iyle
  ekip onayı bekliyor (CONFIG).

Boot logunda `ESP_LOGW(TAG, "BMS: sysState henuz parse edilmiyor ...")` uyarısı
bu durumu görünür kılar. Teknik kontrol sırasında sıcaklık, akım ve SoC'un
**gerçek** olduğu, ancak BMS hata durumunun (sysState) geçici olduğu belirtilmelidir. Kalan
CAN ID'leri araç yola çıktıktan sonra sniffer loglarıyla çözülmeye devam edilecektir
(bkz. `Documents/CAN_Message_Table.md`).

---

## 2. 9.2.a — RF hattinin tek yonlulugu: juriye kanit

**Iddia:** RF hatti (UKS<->AKS arasi LoRa, AYNI fiziksel link) icerik
olarak tek yonlu iki veri turu tasir: AKS->UKS yonunde telemetri (TEL,...
satirlari), UKS->AKS yonunde ise SADECE tek mesru byte (`0xB0`, heartbeat).
UKS->AKS'ta bunun disinda HICBIR komut/veri gonderilmez — RF hatti fiziksel
olarak iki yonlu (UART) olsa da, protokol/anlam duzeyinde AKS hicbir zaman
UKS'ten komut ALMAZ, yalnizca "canliyim" sinyalini siniflandirir. UKS->AKS eski komut kanali (0xA1-0xA4,
E-STOP/START/RESET/DRIVE_ENABLE) commit `9ec3396` (AKS) ve `d8b321f` (UKS)
ile SISTEMDEN TAMAMEN KALDIRILDI; acil durdurma arac ustundeki fiziksel
kontaktorle saglanir, RF'ten bagimsizdir.

### 2.1 Kod yapisi kaniti

- UKS `Core/Inc/lora.h:30`: `#define LORA_HEARTBEAT_BYTE 0xB0U` — RF
  hattindaki TEK TX kaynagi (`Core/Src/main.c:207-208`,
  `Lora_Send(&lora_ctx, &hb, 1U)`).
- AKS `lib/LoraLink/LoraRxHandler.h`: `lora_classify_rx_byte()` yalnizca iki
  sonuc uretir — `HEARTBEAT` (byte == `UKS_HEARTBEAT_BYTE` == `0xB0`) veya
  `UNKNOWN`. Baska hicbir byte'a ozel anlam YUKLENMEZ (dispatch/switch/komut
  isleme YOK).
- AKS `src/main.cpp` LoRa RX dongusu (`vTask_LoRa_UKS`) yalnizca
  `lora_classify_rx_byte` sonucunu loglar; hicbir aksiyon (kontaktor, HMI
  state degisikligi, vs.) TETIKLEMEZ.

### 2.2 UKS boot logu kaniti

UKS her acilista USART1 (115200 baud) uzerinden asagidaki satiri basar
(`Core/Src/main.c:138`):

```
RF hatti tek yonlu (9.2.a): TX yalnizca heartbeat
```

Bu satir, teknik kontrol sirasinda arac/yer istasyonu acildiginda
gozlemlenebilir — sahte/gecici bir log degil, uretim firmware'inin HER
boot'ta bastigi sabit metindir.

### 2.3 Tek-yonluluk bekcisi ciktisi (otomatik, donanimsiz)

`tools/e2e/test_contract_drift.py` icindeki asagidaki testler bu iddiayi
KAYNAK KODU uzerinden (calisan firmware degil, statik metin) dogrular:

- `test_no_removed_command_bytes_in_active_production_code` — UKS ve AKS
  production kaynaklarinda (yorumlar haric) `0xA1`-`0xA4` SIFIR gecer.
- `test_uks_active_code_has_no_button_estop_symbols` — UKS production
  kodunda buton/estop sembolu SIFIR gecer (fiziksel E-STOP artik RF disi).
- `test_uks_sends_heartbeat_0xb0` — UKS'in `0xB0`'i FIILEN `Lora_Send` ile
  gonderdigi POZITIF dogrulanir.
- `test_aks_handles_0xb0_via_lora_rx_handler` — AKS'in `0xB0`'i
  `lora_classify_rx_byte` ile FIILEN HEARTBEAT olarak islediği POZITIF
  dogrulanir.

Calistirma: `tools/e2e/run.sh` (ya da `run.ps1`) icinden
`test_contract_drift.py -k "removed_command_bytes or button_estop or sends_heartbeat or handles_0xb0"`
ile daraltilarak juri onunde canli calistirilabilir; PASS ciktisi yukaridaki
4 maddenin HEPSININ o an dogru oldugunu kanitlar.

---

## 3. 60 sn kesinti gosterimi: AKS log + Monitor GUI eslestirmesi

**Senaryo:** RF baglantisi (UKS heartbeat) ~60 sn kesilir (ör. UKS'in
anteni/gucu gecici kapatilir), ardindan geri gelir.

### 3.1 AKS tarafi (LINK log satirlari)

`src/main.cpp` link durum makinesi (`s_linkDown`) asagidaki iki log
satirini basar:

- Kesinti basladiginda: `ESP_LOGW("LINK", "UKS heartbeat timeout — link DOWN")`
- Kesinti bittiginde (heartbeat tekrar geldiginde), buffer'da orneklenmis
  paket varsa:

  ```
  Heartbeat alindi — link UP: <N> paket, ts araligi [<ilk_ts>..<son_ts>] replay ediliyor
  ```

  (kaynak: `src/main.cpp:557-562`; `<N>` = `ob_count()`, ts araligi
  kesinti sirasinda 1 Hz'e seyreltilerek buffer'a giren ilk/son
  `TEL_timestampMs` degerleridir).

### 3.2 Monitor tarafi (GUI gostergeleri)

`monitor.py::MonitorApp`:

- **Durum rozeti** (`status_badge`): kesinti sirasinda kirmizi "KOPUK",
  link geri gelince yesil "BAĞLI" (bkz. `_refresh_status_badge`).
- **Dosya etiketi** (`file_label`): kesinti/replay boyunca AYNI dosya
  adini gosterir — YENI DOSYA ACILMAZ (yalnizca gercek yeni-boot,
  `detect_new_boot`, dosya degistirir; bkz. `tools/e2e` Assert 1).
- **"Son kayittan bu yana" gostergesi** (`interval_label`): kesinti
  sirasinda artan sure sarı (>4 sn) / kirmizi (>5 sn) renge doner —
  9.2.h "en fazla 5 sn" kuralinin GUI'deki canli izdüsümüdür.
- **events log** (`events_*.log`): `LINK,DOWN alindi` / `LINK,UP alindi`
  satirlariyla ayni olayi metin olarak da kaydeder.

### 3.3 Donanimsiz kanit (tools/e2e)

`tools/e2e/test_outage_simulation.py::test_60s_outage_replay_preserves_single_file_and_coverage`
sanal saatle (gercek 60 sn beklenmeden) tam bu senaryoyu kosar ve UC
ASSERT ile kanitlar:

1. Kesinti/replay boyunca **yeni log dosyasi ACILMADI**
   (`file_open_count == 1`, Monitor'un GERCEK `detect_new_boot`
   fonksiyonuyla).
2. Kesinti araligina ait TUM ts_ms degerleri dosyada MEVCUT (kayip yok).
3. Dosyanin kapsadigi zaman cizelgesinde (sirali/essiz ts_ms) hicbir
   bosluk 5000 ms'yi ASMIYOR (9.2.h kuralinin dogrulanmasi).

Calistirma: `tools/e2e/run.sh -k test_60s_outage`. PASS ciktisi, juriye
"kesinti sirasinda veri kaybi yok + tek dosyada devam ediyor + 5 sn kurali
korunuyor" iddiasinin STATIK/DETERMINISTIK kanitidir; sahada GERCEK 60 sn
bekleyerek yapilacak gosterim icin adim 3.1/3.2'deki log/GUI satirlari
izlenmelidir.

---

## 4. E22 bench dogrulama prosedurune ileriye donuk referans

E22-400T30D-V2 register haritasi (`Core/Inc/e22_regs.h` / `include/E22Regs.h`)
suan icin **V2 VARSAYIMIDIR** ve bench'te dogrulanmadi (bkz. `e22_regs.h`
basindaki "DOGRULAMA NOTU"). Bench'te gercek modulden okunan register
dump'i (`E22REG,0x%02X,0x%02X` formati, `Core/Src/lora.c` / `src/main.cpp`
+ `src/e22_diagnostic.cpp`) ile bu dosyalardaki hedef degerlerin
karsilastirilmasi **P10** kapsaminda ayri bir belgede (**`BENCH_E22_TEYIT.md`**,
henuz yazilmadi) yapilacaktir. Bu belge yazildiginda, teknik kontrol
sirasinda E22 config dogrulama adimi icin BENCH_E22_TEYIT.md'ye
yonlendirme yapilmalidir.

---

## 5. tools/e2e ile ilgili genel not

Bu belgedeki tum "donanimsiz kanit" bolumleri `tools/e2e/` (bu reponun
kokunde) altindaki pytest suite'i ile tekrarlanabilir:

```
cd tools/e2e
./run.sh        # ya da: powershell -File run.ps1
```

UKS ve Monitor repo yollari varsayilan olarak kesfedilir; farkli bir
makinede farkli konumdaysa `TUFAN_UKS_REPO` / `TUFAN_MONITOR_REPO` ortam
degiskenleriyle override edilir (bkz. `tools/e2e/conftest.py`).
