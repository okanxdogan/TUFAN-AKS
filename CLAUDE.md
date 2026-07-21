# AKS ↔ UKS Telemetri Protokolü — Çalışma Kuralları

Bu depo (ESP_AKS) `TUFAN-UKS-TELEMETRY` (yol: `../uks/TUFAN-UKS-TELEMETRY`,
STM32 tarafı) ile bir protokol sözleşmesi paylaşır. Bu dosyadaki kurallar,
bu sözleşmeyi bozmadan çalışmak için zorunludur.

## 1. Protokol Sözleşmesi

AKS→UKS telemetri formatı 19 alanlı `"TEL,..."` CSV'dir (v2).

Tek doğruluk kaynağı hiyerarşisi (bkz. `../uks/TUFAN-UKS-TELEMETRY/DEGISIKLIK_NOTU.md`):

1. **Kod** — `ESP_AKS/lib/Telemetry/Telemetry.cpp::sendStatus()` (AKS tarafı) ve
   `TUFAN-UKS-TELEMETRY/UKS-Telemetry/Core/Src/telemetry.c::Decode_Line()` (UKS
   tarafı). Protokolün fiilen çalışan davranışı budur; bir doküman kodla
   çelişirse **kod kazanır**, doküman güncellenir.
2. **UKS Uyum Sözleşmesi** — `UKS-Telemetry/README.md` içindeki "UKS Uyum
   Sözleşmesi" bölümü.
3. **Diğer dokümanlar** — kök `README.md` §4 (UKS tarafı deposunda),
   `Documents/UKS_LoRa_Protocol.md` vb.

Alan sayısı/sırası/aralığı değişikliği = **protokol değişikliği**: iki repo
**aynı commit setinde** güncellenmeli ve `tools/e2e` drift-guard testleri
çalıştırılmalı.

## 2. Çapraz-Repo Değişiklik Kuralı

E22 register'ları (özellikle **CRYPT**), TEL alan tanımları, heartbeat/timeout
sabitleri **tek taraflı değiştirilmez**. Geçmişte commit `90c8e1b`'de CRYPT tek
taraflı değiştirilip link kırılmıştı (bkz. `Documents/E22_CRYPT_SENKRON.md`
"Geçmiş" bölümü). Böyle bir değişiklikten sonra mutlaka:

```
cd ESP_AKS/tools/e2e && pytest -v
```

(`TUFAN_UKS_REPO` ortam değişkeni gerekebilir.)

## 3. Test Zorunluluğu

Her kod değişikliğinden sonra:

- İlgili native test suite'i: `pio test -e native -f test_native_<ilgili>` (örn.
  `test_native_telemetry`, `ESP_AKS/test/` altında listelidir).
- `tools/e2e` suite'i (`test_frame_contract.py`, `test_contract_drift.py`,
  `test_outage_simulation.py`).

Golden vektörleri (`test_frame_contract.py`, `test_telemetry_format.cpp`)
kırmadan değişiklik yap. Kırılıyorsa bu **bilinçli bir protokol değişikliğidir**
ve iki repo + dokümanlar birlikte güncellenir (bkz. Kural 1).

## 4. Güvenlik Kuralı (Ek B)

FAULT/kontaktör kararları **yalnızca doğrulanmış CAN sinyallerinden** türetilir.
"DOĞRULANMADI / TODO / stub" işaretli alanları karar mantığına bağlama;
bağlanacaksa önce sniffer teyidi ve ekip onayı gerekir.

## 5. Yorum/Doküman Drift'i

Kod davranışı değişince **aynı commit'te** ilgili yorumları ve `.md`
dosyalarını güncelle. "AÇIK İŞ" durum değişikliklerinde
`tools/e2e/test_contract_drift.py` içindeki xfail izleyicisini de güncelle —
`xfail(strict=True)` olduğu için, işaretlenmiş bir alan artık gerçekten
destekleniyorsa test **XPASS** ile suite'i bilerek kırar; bu, izleyicinin
güncellenmesi gerektiğinin sinyalidir.
