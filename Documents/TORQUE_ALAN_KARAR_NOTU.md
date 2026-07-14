# "torque" Alanı (4. Alan) — Semantik Uyumsuzluk Karar Notu

## Durum

AKS↔UKS telemetri sözleşmesinde (`UKS-Telemetry/README.md`, `tools/e2e/contract.py`
`FIELD_RANGES`, UKS `Core/Inc/telemetry.h` `TelData_t`) 4. alan `torque` adıyla
`int16_t` (`-32768..32767`) olarak tanımlıdır.

AKS encoder'ı (`ESP_AKS/lib/Telemetry/Telemetry.cpp::sendStatus`) bu alana
FİİLEN `TEL_motorVoltageDeciV` (motor voltajı, deciV, `uint16_t`, tip sınırı
`0..65535`) yazar — gerçek tork değil. `TEL_motorVoltageDeciV` sözleşmenin
`int16` üst sınırını (`32767`) aşabilir; aşarsa UKS `Parse_Int` **tüm
frame'i** reddeder (`parse_fail`), o anki RPM/BMS gibi diğer tüm geçerli
alanlar da birlikte kaybolur.

## Bu görevde yapılan (kalıcı çözüm DEĞİL)

`TelemetrySanitize::sanitizeMotorVoltForTorqueField()` bu değeri `32767`'ye
kırpar (`sanitizeForUplink` içinde çağrılır) — frame reddini imkânsız hale
getirir, ancak alan adı (`torque`) ile içeriği (motor voltajı) arasındaki
uyumsuzluğu **çözmez**. Bkz. `TelemetrySanitize.h`, `Telemetry.h`/`.cpp`
yorumları, `Documents/UKS_LoRa_Protocol.md` "Alan Aralıkları" tablosu.

## Kalıcı çözüm seçenekleri (ekip kararı gerekli)

### (i) Alanı yeniden adlandır: `motorVoltDeciV`, aralığı `0..65535` yap

- Protokol **v3** bump gerektirir (`ver` sözleşmede halihazırda `2`).
- İki repo **aynı commit setinde** güncellenmeli:
  - UKS `Core/Inc/telemetry.h` (`TelData_t` alan tipi/adı), `Core/Src/telemetry.c`
    `Decode_Line` (`Parse_Int` → `Parse_U32`, aralık `0..65535`).
  - AKS `tools/e2e/contract.py` (`FIELD_RANGES["torque"]` → yeniden adlandır +
    aralık), `UKS-Telemetry/README.md` "UKS Uyum Sözleşmesi" tablosu.
  - `sanitizeMotorVoltForTorqueField` kırpması bu durumda **kaldırılabilir**
    (gerçek üst sınır `65535` olur, `uint16_t` zaten bu sınırın içinde).
- Artı: alan adı gerçeği yansıtır, veri kaybı olmaz (kırpma yok).
- Eksi: protokol version bump + iki repo native/e2e test güncellemesi +
  UKS/Monitor tarafında bu alanı okuyan her yer (varsa) gözden geçirilmeli.

### (ii) Alanı gerçek tork için rezerve et, motor sürücüsü entegrasyonuna kadar `0` gönder

- `sendStatus` bu alana `TEL_motorVoltageDeciV` yerine sabit `0` (veya gerçek
  tork verisi motor sürücüsünden gelene kadar `TEL_motorTorqueRaw` benzeri
  yeni, ayrı bir alan) yazar.
- Motor voltajının telemetride görünmesi isteniyorsa, ayrı bir alan/protokol
  eklenmesi gerekir (yeni alan = yine protokol değişikliği, aynı iki-repo
  kısıtı geçerli).
- Artı: alan adı/içerik tutarlılığı korunur, protokol aralığı değişmez.
- Eksi: motor voltajı telemetriden kalkar (şu an fiilen izlenen tek "motor"
  sinyali budur) — motor sürücüsü entegre olana kadar bu alan hiçbir bilgi
  taşımaz.

## ARA KARAR (2026-07-13)

Alan 4 **mevcut haliyle kalıyor**: kaynak `TEL_motorVoltageDeciV`, sözleşme
etiketi `torque`. `TelemetrySanitize::sanitizeMotorVoltForTorqueField()`
clamp'i frame reddini imkânsız kıldığı için bu haliyle **çalışma riski
yoktur** (yalnızca semantik/etiket uyumsuzluğu — bkz. yukarıdaki "Durum").
Kalıcı çözüm ((i) ya da (ii)) **motor sürücüsü entegrasyonuna ertelendi**:
o noktada motor voltajının telemetride nasıl temsil edileceği (gerçek tork
mu, ayrı bir alan mı, yeniden adlandırma mı) zaten yeniden gözden
geçirilecek — bu kararı şimdiden vermek erken ve tersine çevrilebilir bir
maliyet taşıyor. Bkz. `Documents/MOTOR_ENTEGRASYON_NOTU.md`.

### v3'e bindirilecek adaylar

Protokol versiyon bump'ı (v3) gerektiren, birbirinden bağımsız ama AYNI
bump'a bindirilirse tekrarlanan iki-repo/test-güncelleme maliyetini
paylaşacak üç aday listelendi (henüz hiçbiri için RFC yazılmadı):

- **(a) Alan 4 kararı** — bu notun (i)/(ii) seçenekleri arasında.
- **(b) UKS `bms_current_centima` → `centia` yeniden adlandırması** — alan
  ADI yanıltıcı (centi-mA çağrıştırıyor, gerçek birim centi-A/0.01 A); bkz.
  `Core/Inc/telemetry.h`'deki mevcut yorum notu (Documents/UKS_LoRa_Protocol.md
  "Alan Aralıkları" tablosundaki `current` satırı ile koordineli).
- **(c) VCU state alanı eklenmesi** — UKS `README.md` §11 "Bilinen Açık
  Konular"da zaten planlı ("AKS telemetri paketine VCU durumu eklenmesi...
  protokol versiyon bump'ı ve UKS parser'ında koordineli güncelleme
  gerektirecek, 19 alan kuralını bozmadan").

**RFC şimdi yazılmayacak** — bu yalnızca "aynı bump'a bindirilecek adaylar"
listesidir, seçeneklerin değerlendirilmesi/onayı ayrı bir iş kalemidir.

## Kapsam dışı (bu görevde YAPILMADI)

Bu görevde protokol **değiştirilmedi** — yalnızca (a) clamp eklendi, (b) kod
yorumları ve (c) dokümantasyon gerçeği yansıtacak şekilde güncellendi. (i)/(ii)
arasındaki seçim ekip kararı gerektirir; seçim yapıldığında ilgili değişiklik
iki repoyu aynı commit setinde güncellemeli ve `tools/e2e` drift-guard
testleri + native testler çalıştırılmalıdır (bkz. `CLAUDE.md` Kural 1/2/3).
