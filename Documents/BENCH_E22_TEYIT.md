# E22 Bench Doğrulama Prosedürü (P10) — "V2 varsayımı" teyidi

> **Durum:** UKS README §11 "Bilinen Açık Konular" ve §3 "DOĞRULAMA NOTU"
> — E22-400T30D-V2 register haritası (`Core/Inc/e22_regs.h` / AKS
> `include/E22Regs.h`) şu an **V2 VARSAYIMIDIR** (E22'nin V1/V2 firmware'leri
> arasında register haritası farklı — V2'de `NETID` eklendi, sonraki
> adresler kaydı). Bu belge, `TEKNIK_KONTROL_PROVASI.md` §4'ün referans
> verdiği, henüz yazılmamış olan **`BENCH_E22_TEYIT.md`**'dir — bu görevle
> yazıldı. Kod değişikliği İÇERMEZ; yalnızca mevcut kodun ZATEN ürettiği
> boot-log dump'larını nasıl yakalayıp karşılaştıracağınızı adım adım
> tarif eder.

## Neden gerekli

Her iki firmware de (AKS `LoraLink::configureE22`, UKS `Lora_Init`) E22
modülünü konfigüre etmeden ÖNCE mevcut register bloğunu (`ADDH..CRYPT_L`,
9 byte, `0xC1` okuma komutuyla) okuyup **her boot'ta** hex dump basar — bu,
kod DEĞİŞİKLİĞİ gerektirmeyen, HALİHAZIRDA çalışan bir bench-teyit
mekanizmasıdır. Eksik olan, bu iki dump'ın GERÇEK donanımdan alınıp
BİRBİRİYLE ve `e22_regs.h` sözleşme değerleriyle karşılaştırıldığı bir
prosedür kaydıdır — bu belge o kaydı sağlar.

## Ön koşullar

- AKS: ESP32 dev kit + E22-400T30D-V2 modülü kablolu (M0/M1/AUX/TX/RX +
  besleme), USB üzerinden bilgisayara bağlı.
- UKS: STM32 dev kit + E22-400T30D-V2 modülü kablolu, USART1 (115200 baud)
  bir USB-TTL adaptörle bilgisayara bağlı (`Core/Src/main.c`'de `printf`
  tüm boot-log çıktısını — `[CFG]`, `[AUX]`, `E22REG,...` dahil — bu porta
  yönlendirir, `MX_USART1_UART_Init` / `__io_putchar`).
- Bir seri terminal (PlatformIO `pio device monitor`, PuTTY, CoolTerm vb.)
  — çıktıyı dosyaya kaydedebilen biri tercih edilir (ör. PuTTY "Logging").

## Adım 1 — AKS tarafı dump'ı al

İki eşdeğer yol var; ikisi de kod değişikliği GEREKTİRMEZ (halihazırda
derlenebilir durumda).

### Yöntem A — Normal firmware boot (önerilen, varsayılan akışın gerçek dump'ı)

```powershell
pio run -e esp32dev --target upload
pio device monitor -e esp32dev -b 115200
```

Boot logunda şu satırı arayın: `--- E22 mevcut config yaniti ---` (bkz.
`src/main.cpp::E22_logHexDump`, `LoraLink::configureE22` içinden
"E22 mevcut config yaniti" etiketiyle çağrılır). Bu başlığın HEMEN ALTINDA
9 satır gelir:

```
E22REG,0xADR,0xDEG
```

(adres `0x00`'dan `0x08`'e, yani `ADDH..CRYPT_L`). Bu 9 satırı (başlık
HARİÇ) bir dosyaya kaydedin, ör. `aks_e22_dump.txt`.

> **NOT:** Bu, config yazımından ÖNCEKİ okumadır (read-before-write). Eğer
> modül zaten sözleşmeyle uyumluysa log'da ayrıca "E22 config zaten
> sozlesmeyle uyumlu, kalici (0xC0) yazma atlaniyor" satırını görürsünüz —
> bu NORMAL'dir, dump'ı geçersiz kılmaz (dump zaten yazmadan önce alınan
> okumadır). Modül farklıysa "E22 yazma onay yaniti" başlıklı İKİNCİ bir
> dump da gelir (yazma sonrası doğrulama) — o durumda karşılaştırma için
> İKİNCİ dump'ı kullanın (bench'teki nihai/hedef durumu yansıtır).

### Yöntem B — `E22_DIAGNOSTIC_MODE` (salt-okunur, yan etkisiz)

```powershell
pio run -e esp32dev_diag --target upload
pio device monitor -e esp32dev_diag -b 115200
```

`src/e22_diagnostic.cpp` yalnızca okur (`0xC1`), hiçbir kalıcı yazma komutu
GÖNDERMEZ, ardından normal transparan moda dönüp bir TX smoke testi yapar.
Çıktıda `--- Ham yanit (N byte) ---` başlığının altında aynı `E22REG,...`
9 satırını arayın. **Bu yöntem tercih edilir** eğer modülün MEVCUT
durumunu, olası bir yazma yan etkisi (flash aşınması, CRYPT'in C2/RAM ile
geçici üzerine yazılması) OLMADAN görmek istiyorsanız.

**Tanıdan sonra normal firmware'e geri dönün:**
```powershell
pio run -e esp32dev --target upload
```

## Adım 2 — UKS tarafı dump'ı al

UKS için ayrı bir "diagnostic mode" YOKTUR — normal üretim firmware'i HER
boot'ta aynı dump'ı zaten basar (`Core/Src/lora.c::Lora_Init` →
`E22_DumpBlock`). Kartı normal firmware ile flaşlayın (ya da zaten
flaşlıysa sadece resetleyin/güç verin), USART1'i (115200 baud) izleyin.
Boot logunda şu başlıklardan birini (koşula göre) arayın:

- `[CFG] Mevcut ayarlar (yazma oncesi):` — modül zaten farklıysa, yazmadan
  ÖNCEKİ okuma.
- `[CFG] Yazma sonrasi:` — yazma yapıldıysa, yazmadan SONRAKİ doğrulama
  okuması.
- (Blok hiç okunamadıysa `[CFG] Blok okunamadi ... yazma denenecek` sonrası
  yine `Yazma sonrasi` dump'ı gelir.)

Her başlığın altında AKS ile BİREBİR AYNI formatta 9 satır:
`E22REG,0xADR,0xDEG`. Bu satırları `uks_e22_dump.txt`'e kaydedin.

## Adım 3 — İki çıktıyı diff'le

Her iki dosyada da yalnızca `E22REG,...` satırlarını bırakacak şekilde
filtreleyip diff alın (PowerShell):

```powershell
Select-String -Path aks_e22_dump.txt -Pattern '^E22REG,' | ForEach-Object { $_.Line } > aks_e22.filtered.txt
Select-String -Path uks_e22_dump.txt -Pattern '^E22REG,' | ForEach-Object { $_.Line } > uks_e22.filtered.txt
Compare-Object (Get-Content aks_e22.filtered.txt) (Get-Content uks_e22.filtered.txt)
```

(bash/WSL eşdeğeri: `grep '^E22REG,' aks_e22_dump.txt > aks_e22.filtered.txt`
+ aynısı UKS için + `diff aks_e22.filtered.txt uks_e22.filtered.txt`)

`Compare-Object`/`diff` **BOŞ çıktı** vermeli (9 satır da birebir aynı) —
bu, her iki taraftaki `e22_regs.h` register-adres varsayımının GERÇEK
donanımla (ve birbiriyle) tutarlı olduğunu kanıtlar.

### Sözleşme değerleriyle karşılaştırma

Yukarıdaki dump'ları ayrıca `include/E22Regs.h` ORTAK BLOK sabitleriyle
(`E22_CFG_ADDH`, `E22_CFG_ADDL`, `E22_CFG_NETID`, `E22_CFG_REG0..REG3`)
elle karşılaştırın:

| Adres | Sözleşme (`E22_CFG_*`) | AKS dump | UKS dump | Eşleşiyor mu? |
| --- | --- | --- | --- | --- |
| 0x00 (ADDH) | `0x00` | | | |
| 0x01 (ADDL) | `0x00` | | | |
| 0x02 (NETID) | `0x00` | | | |
| 0x03 (REG0) | `0x64` | | | |
| 0x04 (REG1) | `0x00` | | | |
| 0x05 (REG2) | `0x17` | | | |
| 0x06 (REG3) | `0x00` | | | |
| 0x07 (CRYPT_H) | `0x5A` | | | |
| 0x08 (CRYPT_L) | `0x3C` | | | |

> **CRYPT (0x07/0x08) notu:** E22 CRYPT'i GERİ OKUYAMAZ — okuma her zaman
> `0x00` döner (bkz. `E22Regs.h` CRYPT yorumu). Yani bu iki satır dump'ta
> HER ZAMAN `0x00` görünür, bu bir HATA DEĞİLDİR ve "sözleşmeyle uyumsuz"
> anlamına GELMEZ — CRYPT'in fiilen yazılıp yazılmadığı bu dump'tan
> (kasıtlı olarak) doğrulanamaz; bkz. `Documents/E22_ZORLA_YAZMA_CHECKLIST.md`.

## Sonuç ve dokümantasyon güncellemesi

Diff BOŞ ve tablo tam eşleşiyorsa:

1. UKS `README.md` §11'deki "Register haritası UKS tarafında henüz **V2
   varsayımı** — bench dump ile teyit bekliyor" cümlesini "bench'te
   TEYİT EDİLDİ (tarih, bu belgeye atıf)" olarak güncelleyin.
2. UKS `README.md` §3'teki "DOĞRULAMA NOTU (bağlayıcı)" bloğunu aynı
   şekilde güncelleyin.
3. UKS `Core/Inc/e22_regs.h` başındaki "DOĞRULAMA NOTU" yorumunu güncelleyin
   (AKS `include/E22Regs.h`'deki adres bloğu yorumunda da aynı işaret
   varsa onu da).
4. `TEKNIK_KONTROL_PROVASI.md` §4'teki "henüz yazılmadı" ifadesini bu
   belgeye (`BENCH_E22_TEYIT.md`, artık yazıldı + SONUÇ tarihli) yönlendirecek
   şekilde güncelleyin.

Diff FARK gösteriyorsa (ör. bir register bench'te beklenenden farklı):
hangi adresin farklı olduğunu kaydedin, `e22_regs.h`/`E22Regs.h`'yi
DEĞİŞTİRMEDEN önce farkın gerçek bir V1/V2 haritası uyumsuzluğu mu yoksa
tek seferlik bir provizyon hatası mı olduğunu ayırt edin (bkz.
`Documents/E22_ZORLA_YAZMA_CHECKLIST.md` — CRYPT dışı bir register farklıysa
normal boot zaten otomatik `C0` ile düzeltir; ADDH..REG3 dışı bir varsayım
hatasıysa bu, iki repo + `tools/e2e` drift-guard'ının AYNI COMMIT'te
güncellenmesini gerektiren bir protokol değişikliğidir).

## İlgili dosyalar

- `include/E22Regs.h` (AKS) / `Core/Inc/e22_regs.h` (UKS) — sözleşme
  register adresleri ve hedef değerleri
- `src/main.cpp::E22_logHexDump` / `lib/LoraLink/LoraLink.cpp::configureE22`
  (AKS normal boot dump'ı)
- `src/e22_diagnostic.cpp` (AKS salt-okunur tanı modu, `E22_DIAGNOSTIC_MODE`)
- `Core/Src/lora.c::E22_DumpBlock` / `Lora_Init` (UKS boot dump'ı)
- `platformio.ini` `[env:esp32dev_diag]`
- `TEKNIK_KONTROL_PROVASI.md` §4
- `tools/e2e/test_contract_drift.py::test_e22_register_targets_match_across_repos`
  (register HEDEF değerlerinin iki repo arasında senkron kaldığını statik
  olarak doğrular — bu belgedeki bench dump'ı ise hedefin GERÇEK donanımla
  eşleştiğini doğrular; ikisi TAMAMLAYICIDIR, biri diğerinin yerine geçmez)
