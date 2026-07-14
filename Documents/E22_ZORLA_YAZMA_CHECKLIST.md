# E22 Sahada Zorla-Yazma Checklist'i (G7 CRYPT kurtarma)

> Bu belge, `Documents/E22_CRYPT_SENKRON.md` "Dağıtım" bölümünde vaat
> edilen "sahadaki zorla-yazma checklist'i ayrı olarak paylaşılacaktır"
> notunun karşılığıdır. Kod değişikliği İÇERMEZ — yalnızca MEVCUT koda
> nasıl geçici bir müdahale yapılıp geri alınacağını tarif eder.

## Ne zaman gerekli

**Yalnızca** G7-FIX-2'den ÖNCE (bkz. `E22_CRYPT_SENKRON.md` "G7-FIX-2 —
Kör noktanın koda gömülü çözümü") flaşlanmış/provizyonlanmış, yani
`CRYPT=0x0000` ile flash'a KALICI (`0xC0`) yazılmış modüller için. G7-FIX-2
sonrası flaşlanan HER modül zaten her boot'ta CRYPT'i `0xC2`/RAM ile
otomatik tazeler (best-effort, kalıcı yazmaya gerek KALMADAN link çalışır)
— bu modüller için aşağıdaki adımlar GEREKSİZDİR.

**Nasıl anlaşılır bir modülün eski (G7-FIX-2 öncesi) flash'landığı:**
Modülün linki, karşı taraf `CRYPT=0x5A3C` ile konuşurken çalışmıyorsa VE
her iki firmware de güncel (G7-FIX-2 dahil) ise, muhtemel neden budur —
flash'taki CRYPT hâlâ eski (`0x0000` veya başka bir değer).

## ÖNEMLİ düzeltme (bu belge yazılırken tespit edildi)

`E22_CRYPT_SENKRON.md`'deki "Dağıtım" bölümü üç yol sayar; bunlardan biri
**artık doğru değil**:

- ~~"`E22_DIAGNOSTIC_MODE` build'i ile config yazımını tetikle"~~ — **YANLIŞ**.
  `src/e22_diagnostic.cpp`'nin kendi başlık yorumu açıkça der ki: *"Hiçbir
  kalıcı yazma komutu göndermez"* — bu mod yalnızca `0xC1` (okuma) gönderir,
  hiçbir `0xC0`/`0xC2` yazma komutu YOKTUR. Yani bugünkü haliyle
  `E22_DIAGNOSTIC_MODE`, CRYPT'i (veya başka bir register'ı) ZORLA
  YAZDIRAMAZ — yalnızca OKUYUP göstermek için kullanılabilir (bkz.
  `Documents/BENCH_E22_TEYIT.md` Yöntem B). Diagnostic mod'a bir yazma
  yolu eklemek bu belgenin (ve bu görevin) kapsamı DIŞINDA bir kod
  değişikliği olur.

Bu yüzden aşağıda yalnızca GERÇEKTEN çalışan iki yol var: **Yöntem A**
(geçici kod zorlaması, normal firmware) ve **Yöntem B** (harici EBYTE
config aracı, firmware'e hiç dokunmadan).

## Yöntem A — Geçici kod zorlaması (normal firmware ile, KALICI/flash yazar)

Bu, `E22_CRYPT_SENKRON.md`'nin "(geçici) boot yolunda `LO_needsWrite`'ı
bir defalığına zorla" dediği yoldur — kodda literal olarak `LO_needsWrite`
adında bir değişken YOK; kastedilen, aşağıdaki YEREL `needsWrite`
değişkenidir.

### AKS tarafı

1. `lib/LoraLink/LoraLink.cpp` içinde `configureE22()` fonksiyonunu aç.
2. `bool needsWrite = true;` satırından hemen sonra, `e22_regsEqual(...)`
   bloğunun içindeki `needsWrite = false;` atamasını GEÇİCİ olarak yorum
   satırı yap (ya da fonksiyonun en başına `needsWrite` mantığını atlayıp
   doğrudan `bool needsWrite = true;` sabitleyen tek satırlık bir geçici
   değişiklik ekle) — amaç: modül zaten "sözleşmeyle uyumlu" görünse bile
   `0xC0` kalıcı yazma yolunun ÇALIŞTIRILMASINI garanti etmek.
3. `pio run -e esp32dev --target upload` ile TEK SEFERLİK flaşla.
4. Seri monitörde (`pio device monitor -b 115200`) "E22 config dogrulandi:
   NETID=... REG0=... ..." satırının BAŞARIYLA geldiğini doğrula (bkz.
   `LoraLink.cpp` satır ~128).
5. **GERİ AL:** adım 2'deki geçici değişikliği KALDIR (git diff ile
   kontrol et — `git diff lib/LoraLink/LoraLink.cpp` boş dönmeli).
6. `pio run -e esp32dev --target upload` ile NORMAL (düzeltilmiş)
   firmware'i TEKRAR flaşla — bu artık kalıcı yazmayı normal
   read-before-write mantığıyla (gerektiğinde) yapacak sürüm.

### UKS tarafı (aynı mantık)

1. `Core/Src/lora.c` içinde `Lora_Init()`'teki `uint8_t needs_write = 0U;`
   satırının hemen altına, döngüden ÖNCE geçici olarak `needs_write = 1U;`
   ekle (döngünün sonucu ne olursa olsun `needs_write`'ı 1'e sabitler).
2. Tek seferlik flaşla, USART1 (115200 baud) boot logunda "[CFG] En az bir
   alan hedeften farkli — flash'a yaziliyor." + ardından "Yazma sonrasi"
   dump'ının geldiğini doğrula (bkz. `Documents/BENCH_E22_TEYIT.md` Adım 2).
3. **GERİ AL:** geçici `needs_write = 1U;` satırını KALDIR (`git diff
   Core/Src/lora.c` boş dönmeli).
4. Normal (düzeltilmiş) firmware'i TEKRAR flaşla.

> **DİKKAT:** Adım 5/3'teki geri-alma ZORUNLUDUR. Kalıcı olarak
> `needsWrite=true`/`needs_write=1U` bırakmak, HER boot'ta gereksiz bir
> `0xC0` flash yazımına yol açar (flash aşınması — E22_CRYPT_SENKRON.md'nin
> "bu bilinçli olarak koda gömülü bir 'her boot yeniden yaz' davranışı
> DEĞİLDİR" uyarısının tam olarak önlemeye çalıştığı şey budur).

## Yöntem B — Harici EBYTE config aracı (firmware'e hiç dokunmadan)

Modülü geçici olarak ana devreden (AKS/UKS kartından) çıkarıp bir
USB-TTL adaptörle doğrudan PC'ye bağlayın, EBYTE'ın kendi PC
yapılandırma yazılımıyla (E22 config modu: M0=0, M1=1) `CRYPT_H`
(adres `0x07`) ve `CRYPT_L` (adres `0x08`) register'larına doğrudan
`0x5A`/`0x3C` yazın. Bu yöntem HİÇBİR firmware değişikliği/flaşlaması
gerektirmez, ama modülün fiziksel olarak sökülüp bağımsız programlanmasını
gerektirir — sahada birden çok modül varsa Yöntem A genelde daha pratiktir.

## Doğrulama — yazım sonrası çift-yönlü link testi

Zorla-yazma (Yöntem A veya B) sonrası, HER İKİ tarafın da güncel (normal,
geçici-değişiklik-geri-alınmış) firmware ile çalıştığından emin olduktan
sonra:

1. Her iki kartı da güç ver, normal firmware ile çalıştır (AKS `pio run -e
   esp32dev --target upload`, UKS normal build).
2. **AKS→UKS yönü:** UKS USART1 (115200 baud) çıktısında `TEL,...`
   satırlarının DÜZENLİ aralıklarla (2 Hz, `LORA_TX_PERIOD_MS=500`)
   geldiğini doğrula — bu, AKS'in CRYPT'li frame'lerinin UKS tarafından
   DOĞRU deşifre edilip ayrıştırıldığını kanıtlar (yanlış CRYPT ile frame
   bozuk/gürültü olarak gelir, `TEL,` ile başlamaz).
3. **UKS→AKS yönü:** AKS boot logunda `LINK,UP`/heartbeat kabul edildiğine
   dair kanıt ara — pratik yöntem: UKS'i ~15-20 sn kapatıp tekrar aç, AKS
   loglarında `ESP_LOGW("LINK", "UKS heartbeat timeout — link DOWN")`
   ardından `ESP_LOGI("LINK", "Heartbeat alindi — link UP: ...")`
   satırlarının GÖRÜLDÜĞÜNÜ doğrula (bkz. `TEKNIK_KONTROL_PROVASI.md`
   bölüm 3.1) — bu, UKS'in `0xB0` heartbeat'inin AKS tarafından (doğru
   CRYPT ile deşifre edilip) HEARTBEAT olarak sınıflandırıldığını
   kanıtlar.
4. Her iki yön de PASS ise, bu modül çifti artık G7 (heartbeat-injection)
   açığına karşı KORUNUYOR ve link normal çalışıyor demektir — checklist
   TAMAMLANDI.

Her iki yön de test EDİLMELİDİR — yalnızca birinin çalışması (ör. AKS→UKS
TEL akıyor ama UKS→AKS heartbeat tanınmıyor) CRYPT uyuşmazlığının hâlâ
var olduğunu gösterebilir (bkz. `E22_CRYPT_SENKRON.md` "Geçmiş" — commit
`90c8e1b` tam olarak bu asimetrik-bozulma senaryosuydu).

## İlgili dosyalar

- `Documents/E22_CRYPT_SENKRON.md` — "Dağıtım" ve "G7-FIX-2" bölümleri
- `lib/LoraLink/LoraLink.cpp::configureE22` — AKS `needsWrite` mantığı
- `Core/Src/lora.c::Lora_Init` — UKS `needs_write` mantığı
- `Documents/BENCH_E22_TEYIT.md` — dump alma/karşılaştırma prosedürü
  (zorla-yazma SONRASI doğrulama için de kullanılabilir)
- `src/e22_diagnostic.cpp` — salt-okunur tanı modu (yazma YAPMAZ, bkz.
  yukarıdaki "ÖNEMLİ düzeltme")
