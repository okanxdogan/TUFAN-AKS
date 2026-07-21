# Araç Parametreleri Teyit Checklist'i (9.2.c.i / 9.4.b.iii)

> **ÖNEMLİ — bu doküman yazılırken tespit edildi:** Bu checklist'in ilk üç
> maddesi (`VEHICLE_PARAMS_CONFIRMED`, gerçek değerler, native testlerin
> güncellenmesi) **repoda ZATEN yapılmış durumda** (bkz. commit `fb329f0`
> "feat: gercek arac parametreleri girildi (0.56m / GR=1 direkt tahrik) -
> CONFIRMED=1"). Bu doküman o yüzden bir "sıfırdan yapılacaklar" listesi
> DEĞİL, **durum + kalan tek gerçek açık iş** (saha hız karşılaştırma
> provası) kaydıdır. Kod değerleri bu görevde DEĞİŞTİRİLMEDİ.

## 1. Değerler + `VEHICLE_PARAMS_CONFIRMED` — ✅ TAMAMLANDI

Kaynak: `include/VehicleParams.h` (commit `fb329f0`, 2026-07).

| Parametre | Değer | Teyit kaynağı |
| --- | --- | --- |
| `WHEEL_DIAMETER_M` | `0.56f` | Mekanik ekip, 2026-07 — yüklü araçta yuvarlanma çevresi bantla ölçülüp D=çevre/π |
| `GEAR_RATIO` | `1.0f` | Güç aktarma ekibi, 2026-07 — direkt tahrik |
| `MOTOR_RPM_IS_WHEEL_RPM` | `1` | Direkt tahrikte motor rpm = teker rpm |
| `VEHICLE_PARAMS_CONFIRMED` | `1` | Yukarıdaki üçü teyitli olduğu için `1` |

`VEHICLE_PARAMS_CONFIRMED=1` olduğundan `#warning` ve boot'taki
`ESP_LOGE("ARAC PARAMETRELERI TEYITSIZ ...")` (bkz. `src/main.cpp`
`#if !VEHICLE_PARAMS_CONFIRMED` bloğu) artık **derlenmiyor/basılmıyor** —
bu, doğru davranıştır.

## 2. `rpmToSpeedKmhX10` sınır analizi (gerçek D/GR ile) — ✅ TAMAMLANDI

Zaten `test/test_native_telemetry/test_telemetry_format.cpp` içinde
(`test_rpm_to_speed_clamp_just_above_threshold_rpm` /
`test_rpm_to_speed_no_clamp_just_below_threshold_rpm` yorumlarında) türetilmiş
ve kilitlenmiş durumda. Formül ve sonuç burada tekrar özetlenir:

```
km/h = rpm_teker × π × D × 60 / 1000        (D=0.56, GR=1 → rpm_teker = rpm)
spd_x10 = km/h × 10
TEL_SPD_X10_MAX = 3000  (UKS Decode_Line spd_x10 sanity sınırı, 0..3000)
```

Clamp'in devreye girdiği eşik rpm:

```
3000 = rpm × π × 0.56 × 60/1000 × 10
rpm  = 3000 / (π × 0.56 × 0.6) ≈ 2842.4
```

Yani **rpm ≥ 2843'te** clamp devreye girer (300.0 km/h sanity sınırına
takılır); **rpm ≤ 2842'de** clamp devreye GİRMEZ, ham (kırpılmamış) değer
döner. Bu iki sınır native testlerle KİLİTLİ (aşağıya bkz.).

`TEL_RPM_MAX` (UKS sanity, 20000 — `tools/e2e/contract.py`) ile
karşılaştırıldığında: 2842 rpm, 20000'in ÇOK altında — yani gerçek
D=0.56/GR=1 geometrisiyle, motor teorik olarak 2843-20000 rpm aralığında
dönerken spd_x10 HER ZAMAN 3000'e kırpılı gider (gerçek hız gizlenir, ama
paket reddi OLMAZ). Bu, `Telemetry.h`'deki mevcut NOT ile tutarlıdır
("TEL_SPD_X10_MAX'e clamp gerçek hızı gizleyebilir, ama gerçek D/GR
girildiğinde 300 km/h zaten fiziksel olarak erişilemez bir değer olacaktır" —
bu artık gerçek değerlerle YAZILMIŞ durumda, dolayısıyla üst sınır 300 km/h
motosiklet/elektromobil sınıfı için makul bir "asla erişilmeyecek tavan"dır).

## 3. Native hız testleri gerçek değerlerle güncel mi? — ✅ TAMAMLANDI

`test/test_native_telemetry/test_telemetry_format.cpp`:

- `test_rpm_to_speed_typical` — `rpmToSpeedKmhX10(1500)` üretim sarmalayıcısını
  (gerçek `WHEEL_DIAMETER_M`/`GEAR_RATIO`/`MOTOR_RPM_IS_WHEEL_RPM` makrolarını
  kullanır) **1583** (158.3 km/h) bekliyor — yorum açıkça "0.56m/GR=1, 2026-07"
  diyor.
- `test_rpm_to_speed_clamp_just_above_threshold_rpm` (rpm=2843) /
  `test_rpm_to_speed_no_clamp_just_below_threshold_rpm` (rpm=2842) — yukarıdaki
  ≈2842.4 sınırını iki tarafından da kilitliyor.
- `rpmToSpeedKmhX10Impl` (parametrik çekirdek) testleri BİLİNÇLİ olarak
  YEREL literal D/GR değerleri kullanıyor (üretim makrolarına DEĞİL) — böylece
  gerçek değerler ileride tekrar değişirse (ör. lastik aşınması, farklı teker)
  bu testler KIRILMAZ. Bu görevde ayrıca bir **gerçekçi-aralık taraması**
  eklendi (`test_impl_realistic_range_sweep_stays_within_bounds`,
  D=0.4–0.7 m × GR=1–10 × geniş bir rpm kümesi) — taşma/negatiflik
  olmadığını (sonuç her zaman `[0, TEL_SPD_X10_MAX]` aralığında) genel
  olarak doğrular, yalnız bugünkü D/GR'ye özel değil.

## 4. Monitor `CONFIG_CONFIRMED` ile senkron mu? — N/A (yanlış varsayım, açıklandı)

`TUFAN-Monitor/config.py`'deki `CONFIG_CONFIRMED` **tekerlek/dişli
parametreleriyle İLGİLİ DEĞİL** — `BATTERY_CAPACITY_WH` (100 Ah paket,
batarya ekibi teyidi) için ayrı bir bayraktır ve zaten `True`. Monitor,
hızı KENDİSİ hesaplamaz — AKS'in zaten hesaplayıp UKS üzerinden gönderdiği
`spd_x10` değerini olduğu gibi gösterir (bkz. UKS `Core/Src/main.c` CSV
forward satırı, `tools/e2e/contract.py build_forward_line`). Yani AKS
tarafında D/GR teyit edilince Monitor tarafında SENKRONİZE edilecek ayrı bir
sabit YOK — bu checklist maddesi yanlış bir varsayıma dayanıyordu.

Tek gevşek bağlantı: `config.py`'deki `MAX_SPEED_KMH = 150` (grafik Y ekseni
üst sınırı, saf GUI ölçeklemesi). Gerçek D/GR ile üretilebilecek üst sınır
300 km/h'ye kadar (clamp) çıkabildiğinden, sahada gerçek üst hız gözlenirse
bu eksen sınırı ayrıca (gerekirse) büyütülebilir — bu bir DOĞRULUK sorunu
değil, yalnız bir görüntüleme rahatlığı sorunudur; blok değildir.

## 5. Saha hız karşılaştırma provası — ⏳ AÇIK (tek gerçek kalan iş)

Bkz. `TEKNIK_KONTROL_PROVASI.md` madde **9.4.b.iii** — prosedür ORADA zaten
yazılı, ama bu doküman o testin FİİLEN YAPILDIĞINI kaydetmiyor. Bu,
kod/doküman değil, **fiziksel bir saha testi** olduğundan bu görevde
yapılamaz; ekip için checklist:

- [ ] Aracı bilinen sabit bir hızda çalıştır (standlı test / GPS referans /
  bilinen bir mesafe + kronometre).
- [ ] UKS USART1 (115200 baud) çıktısındaki `CSV,...` satırının 3. alanını
  (`spd_x10`) oku, `/10` ile km/h'ye çevir.
- [ ] Monitor GUI'deki HIZ göstergesiyle karşılaştır (Monitor kendi
  hesaplamaz, aynı `spd_x10`'u gösterir — bkz. madde 4 — bu yüzden bu adım
  esas olarak UART/parse zincirinin bütünlüğünü sınar, D/GR doğruluğunu
  DEĞİL).
- [ ] Üçünü (referans hız, UKS `spd_x10/10`, Monitor GUI) karşılaştır — ±
  ölçüm toleransı içinde eşleşmeli.
- [ ] Sonucu bu bölüme (madde 5) TARİH + ölçüm değerleriyle KAYDET; bu
  yapıldığında `TEKNIK_KONTROL_PROVASI.md` madde 9.4.b.iii'yi "kanıtlı,
  saha teyitli" olarak güncelle.

**NOT:** Bu adım D=0.56/GR=1'in FİZİKSEL DOĞRULUĞUNU test eder (mekanik
ekibin ölçümü — bkz. madde 1 — halihazırda "TEYİTLİ" diyor, ama bu, o
ölçümün UÇTAN UCA telemetri zincirinde doğru şekilde YANSIDIĞININ bağımsız
bir doğrulaması değildir). Şimdiye kadarki tüm işler (madde 1–3) kodun D/GR
değerlerini DOĞRU KULLANDIĞINI kanıtlıyor; madde 5 D/GR'nin KENDİSİNİN
fiziksel gerçeklikle örtüştüğünü kanıtlayacak.

## İlgili dosyalar

- `include/VehicleParams.h` — `WHEEL_DIAMETER_M`, `GEAR_RATIO`,
  `MOTOR_RPM_IS_WHEEL_RPM`, `VEHICLE_PARAMS_CONFIRMED`
- `lib/Telemetry/Telemetry.h` — `rpmToSpeedKmhX10`/`rpmToSpeedKmhX10Impl`,
  `TEL_SPD_X10_MAX`
- `test/test_native_telemetry/test_telemetry_format.cpp` — sınır testleri +
  gerçekçi-aralık taraması
- `tools/e2e/contract.py` — `TEL_RPM_MAX`, `TEL_SPD_X10_MAX` (UKS tarafı
  sözleşme eşdeğerleri)
- `TEKNIK_KONTROL_PROVASI.md` madde 9.4.b.iii — saha provası prosedürü
- `TUFAN-Monitor/config.py` — `CONFIG_CONFIRMED` (BATTERY_CAPACITY_WH için,
  D/GR için DEĞİL — bkz. madde 4)
