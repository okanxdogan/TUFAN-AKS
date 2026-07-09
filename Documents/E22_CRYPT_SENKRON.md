# E22 CRYPT Senkronizasyon Notu (G7)

Bu not, RF katmanındaki **heartbeat-injection açığını** (G7) kapatan sıfır-dışı
E22 CRYPT anahtarının çapraz-repo senkronunu belgeler.

## Neden

E22 sözleşmesi broadcast (ADDH/ADDL/NETID=0) + transparan mod + tek sabit byte
(0xB0) link canlılığı. `CRYPT=0x0000` iken 433.125 MHz'deki **herhangi** bir E22
modülü saniyede bir 0xB0 basarsa, gerçek UKS kapalıyken bile AKS linki "UP"
sanar → offline örnekleme hiç başlamaz → kesinti verisi **kalıcı kaybolur**.
Sıfır-dışı ortak gizli anahtar bunu neredeyse bedavaya kapatır. Heartbeat
protokolü (0xB0) DEĞİŞMEZ; yalnızca RF katmanı şifrelenir.

## Değer (tek gerçek kaynak: `E22_CRYPT_KEY`)

| Register | Adres | Değer |
|----------|-------|-------|
| CRYPT_H  | 0x07  | **0x5A** |
| CRYPT_L  | 0x08  | **0x3C** |

16-bit anahtar: `E22_CRYPT_KEY = 0x5A3C`.

## Durum: SENKRON (bu commit seti)

Aşağıdaki dört dosya **aynı commit setinde** güncellendi:

1. **AKS** — `ESP_AKS/include/E22Regs.h`
   `E22_CRYPT_KEY=0x5A3CU`, `E22_CFG_CRYPT_H=0x5AU`, `E22_CFG_CRYPT_L=0x3CU`.
   H/L ile key tutarlılığı `E22Config.cpp`'de `static_assert` ile derleme-
   zamanı bağlı.
2. **AKS** — `ESP_AKS/tools/e2e/contract.py`
   `E22_CRYPT_H=0x5A`, `E22_CRYPT_L=0x3C`.
3. **UKS** — `TUFAN-UKS-TELEMETRY/UKS-Telemetry/Core/Inc/e22_regs.h`
   `E22_VAL_CRYPT_H=0x5AU`, `E22_VAL_CRYPT_L=0x3CU`. `E22_WRITE_BLOCK_INIT`
   makrosu bu sabitleri (hardcode değil) referans alır; UKS'in E22 config
   yazım yolu bu register'ları modüle yazar (AKS tarafı
   `e22_buildWriteCommand` de aynı şekilde CRYPT_H/L'yi yazıyor).
4. **AKS** — `ESP_AKS/test/test_native_e22_config/test_e22_config.cpp`
   `e22_buildWriteCommand` testindeki beklenen CRYPT byte'ları `0x5A/0x3C`
   olarak güncellendi (aksi halde native test kırmızı kalırdı).

> ⚠️ **GİZLİLİK:** Bu bir paylaşılan gizli anahtardır. Repo public olacaksa
> değeri gizli-config/secret mekanizmasına taşımayı değerlendirin; şimdilik
> her iki firmware de derleme-zamanı sabit olarak taşıyor.

## Geçmiş (aynı hata tekrarlanmasın)

- Bu senkronizasyon daha önce bir kez denenmiş: commit `90c8e1b` ("fix:e22
  uks ile uyumlu") CRYPT'i **yalnızca AKS tarafında** 0x5A3C'ye çekmiş, UKS
  `e22_regs.h` güncellenmeden bırakılmış. AKS/UKS arasında CRYPT anahtarı
  uyuşmayınca RF linki kırılmış (encrypted/plain uyumsuzluğu heartbeat'i
  geçersiz kılar).
- Sorun fark edilince aynı commit içinde bu doküman **silinerek** ve AKS
  CRYPT değeri **0x0000'a geri alınarak** "düzeltilmiş" — yani gerçek düzeltme
  değil, yarım-uygulamanın iptali yapılmış. Link eski (güvensiz ama çalışan)
  haline dönmüş.
- Ders: CRYPT değişikliği **iki repoyu aynı anda** güncellemeden asla tek
  başına yapılmamalı — drift-guard testi (aşağıda) bunu otomatik yakalar,
  bu yüzden değişiklik sonrası **mutlaka** çalıştırılmalı.

## Drift-guard (elle çalıştırma)

Çapraz-repo senkronu koruyan test:
`ESP_AKS/tools/e2e/test_contract_drift.py::test_e22_register_targets_match_across_repos`
(UKS `E22_VAL_CRYPT_H/L` == AKS `E22_CFG_CRYPT_H/L` == `contract.py` E22_CRYPT_H/L).

Bu test **UKS reposunu gerektirir** (`conftest.py` `TUFAN_UKS_REPO` ile bulur,
yoksa `../TUFAN-UKS-TELEMETRY`, `../../TUFAN-UKS-TELEMETRY`,
`../../uks/TUFAN-UKS-TELEMETRY`, `../uks/TUFAN-UKS-TELEMETRY` aday yollarını
otomatik dener). UKS bulunamazsa test toplanırken RuntimeError verir — CI
kurulmadı, elle çalıştırılır:

```
# UKS repo'yu göster ve E22 drift bekçisini çalıştır:
$env:TUFAN_UKS_REPO = 'C:\path\to\TUFAN-UKS-TELEMETRY'   # (pwsh, UKS-Telemetry/ İÇEREN kök dizin)
cd ESP_AKS/tools/e2e
pytest test_contract_drift.py -k e22_register_targets -q
```

UKS `e22_regs.h` yukarıdaki 0x5A/0x3C değerleriyle güncellenmeden bu test
KIRMIZI kalır — bu, çift-commit senkronunun kaçırılmadığını garanti eder.

## Dağıtım (ÖNEMLİ — deploy edilmiş modüller)

AKS boot config yolu, modülün mevcut register'larını okuyup sözleşmeyle
karşılaştırır ve **eşitse yazmayı atlar** (`e22_regsEqual`). E22 CRYPT'i geri
OKUYAMADIĞI için (okuma hep 0) bu karşılaştırma CRYPT'i **kapsamaz** — yani
daha önce `CRYPT=0` ile provizyonlanmış bir modülde register'lar sözleşmeye
uyduğundan **yazma atlanır ve yeni CRYPT hiç yazılmaz**.

Bu yüzden mevcut sahadaki her E22 için CRYPT'i **bir kez zorla yaz**:
- `E22_DIAGNOSTIC_MODE` build'i ile config yazımını tetikle, ya da
- modülü fabrika ayarına alıp yeniden provizyonla, ya da
- (geçici) boot yolunda `LO_needsWrite`'ı bir defalığına zorla.

Bu bilinçli olarak koda gömülü bir "her boot yeniden yaz" davranışı DEĞİLDİR
(flash aşınması + kapsam sürünmesi). Anahtar rotasyonunda da aynı zorla-yazma
adımı gerekir. Sahadaki zorla-yazma checklist'i ayrı olarak paylaşılacaktır
(bkz. görev raporu — bu doküman kod değişikliği değil talimat içerir).
