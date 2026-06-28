# BMS Display Edge Cases & Risk Report

Rol 3 (Sistem Entegratörü, Test & Proje Yöneticisi) tarafından üretilmiştir.
Kapsam: 24 hücreli batarya gösterge zinciri — `BmsPackData` (ham) -> `computePack()`
-> `BmsComputed` (yorumlanmış) -> Nextion komutları -> ekran.

Bu doküman, sözleşme (`ESP_AKS/lib/BmsModel/BmsModel.h`) ve algoritma
(`ESP_AKS/lib/BmsAlgo/*`) sınırlarındaki edge case'leri, beklenen davranışı ve
tasarım risklerini listeler. Otomatik testler:
`ESP_AKS/test/test_native_bms_edge/` (yalnızca `BmsModel.h`'ye bağımlı).

## 1. Edge Case Tablosu

| # | Edge Case | Girdi (BmsPackData) | Beklenen Davranış | Test |
| --- | --- | --- | --- | --- |
| 1 | Tüm hücreler 0 mV (sensör kopması / boş çerçeve) | `cellVoltageMv[*]=0` | "Ölü sensör" sezgiseli yakalar; toplam=0, delta=0; undervolt da tetiklenir. Ekranda CRIT / geçersiz gösterilmeli | `test_all_cells_zero_*` |
| 2 | `isValid=false` paket | içerik iyi olsa bile `isValid=false` | Tüketici paketi YORUMLAMAZ; güvenli tarafta kalır (algoritma sözleşmesi: CRITICAL + zararsız varsayılan) | `test_invalid_pack_not_consumable`, `test_invalid_flag_independent_of_content` |
| 3 | Tek hücre çok yüksek (4200+ mV) | bir hücre = 4250 mV | `maxCellIndex` doğru; `cellDeltaMv` büyür; overvolt eşiği (>4250 CRIT) aşılır | `test_single_cell_overvolt_delta`, `test_overvolt_cell_above_plausible` |
| 4 | Tek hücre çok düşük (2500 mV) | bir hücre = 2500/2499 mV | `minCellIndex` doğru; 2500 sınırda (>=2500 OK), 2499 undervolt; algoritmada <3000 CRIT | `test_single_cell_undervolt_2500`, `test_cell_below_undervolt_threshold` |
| 5 | Aşırı sıcaklık (>70 °C) | bir hücre = 71/85 °C | `hasOvertemp`=true; algoritma >60 °C CRIT; ekranda CRIT | `test_overtemp_detected`, `test_overtemp_at_index_23` |
| 6 | Aşırı soğuk (< -20 °C) | bir hücre = -25 °C / INT16_MIN | `hasUndertemp`=true; negatif int16 doğru saklanır | `test_undertemp_negative_detected`, `test_int16_temp_min_extreme` |
| 7 | Akım int32 sınırına yakın | `packCurrentMa=INT32_MAX/MIN` | Değer kayıpsız saklanır; işaret (+şarj/−deşarj) korunur | `test_current_max_int32_stored`, `test_current_min_int32_stored`, `test_current_sign_discharge_negative` |
| 8 | **Paket toplam gerilimi taşması** | tüm hücreler 4200 mV => 100800 mV | int32 ile doğru toplam (100800); uint16 ile WRAP (35264) — **KRİTİK BAYRAK** | `test_pack_sum_exceeds_uint16_range`, `test_pack_sum_uint16_wraps_around` |
| 9 | İndeks sınırı: uç hücre index 0/23'te | min/max index 0 veya 23 | `maxCellIndex`/`minCellIndex` uç indeksleri doğru döndürür; off-by-one yok | `test_max_cell_at_index_0/23`, `test_min_cell_at_index_0/23` |
| 10 | Eşik sınır davranışı | tam eşik değerleri (70 °C, -20 °C, 2500 mV) | "kesin büyüktür/küçüktür" (strictly >/<): tam eşikte tetiklenmez | `test_temp_at_threshold_not_overtemp`, `test_temp_at_lower_threshold_not_undertemp`, `test_single_cell_undervolt_2500` |

## 2. Tasarım Riskleri

| Risk | Açıklama | Etki | Öneri |
| --- | --- | --- | --- |
| **R1 — packVoltageMv taşması (KRİTİK)** | `BmsComputed.packVoltageMv` `uint16_t`. 24×4200=100800 mV > 65535. Hatta NOMİNAL 24×3650=87600 mV bile taşar. uint16 toplama sessizce sarılır (100800 -> 35264). | Ekranda ~100 V yerine ~35 V gösterilir. Sürücü dolu paketi boş sanır — güvenlik riski. NORMAL çalışmada da gerçekleşir, yalnızca uç durumda değil. | Rol 2 tipi gözden geçirsin: `uint32_t packVoltageMv` (mV) VEYA `uint16_t packVoltageDeciV` (0.1 V; 1008 deciV << 65535, Telemetry zaten deciV kullanır). |
| R2 — Eşik sınır belirsizliği | Algoritma "strictly greater/less" kullanır (delta==50 => denge yok). Sözleşme-tarafı doğrulayıcı da aynı kuralı izler. Sınır değerinin hangi tarafa düştüğü dokümante edilmezse tüketiciler farklı yorumlayabilir. | Sınır değerlerinde tutarsız uyarı/denge davranışı. | Eşiklerin "dahil/hariç" semantiği `BmsAlgo.h`'de net (mevcut); HMI ve testler bunu birebir aynalamalı (yapıldı). |
| R3 — Sensör arızası "iyi görünen" geçersiz veri | `isValid=true` ama tüm hücreler 0 mV — fiziksel olarak imkansız. Tek başına `isValid` yeterli değil. | Ölü sensör "boş paket" gibi gösterilebilir. | Algoritma/HMI ek mantık-bütünlük kontrolü (örn. tüm hücreler 0 veya tümü eşit-imkansız) uygulamalı; ekranda ayrı "SENSOR FAULT" durumu. |
| R4 — Stale / timeout verisi | `read()` taze veri yoksa `isValid=false` döndürür (sözleşme). Ancak HMI task'ı son geçerli kareyi cache'lerse, kaynak sustuğunda eski değer ekranda kalabilir. | Sürücü güncel olmayan veriye güvenir. | HMI task'ında motor tarafındaki `TEL_*TimeoutActive` kalıbına benzer bir BMS timeout sayacı; süre dolunca `valid.txt="TIMEOUT"`. |
| R5 — Akım birim/ölçek karışıklığı | `BmsPackData.packCurrentMa` mA; CAN tarafı `TEL_bmsCurrentCentiMa` (0.01 mA). Birim uyumsuzluğu entegrasyonda ×100 hatası yaratabilir. | Akım 100 kat yanlış. | Adaptör katmanında birim dönüşümü açıkça yapılmalı ve test edilmeli. |
| R6 — socPercent clamp | SoC basit lineer OCV; aralık dışı (örn. 0 mV veya 4300 mV) değerler 0..100'e clamp'lenmeli. | Clamp yoksa <0 / >100 saçma SoC. | Algoritma clamp'i garanti eder (sözleşmede belirtilmiş); HMI yine de 0..100 doğrular. |

## 3. Test Çalıştırma

```
pio test -e native -f test_native_bms_edge
```

(Bu doküman/testler `pio` çalıştırmaz; yalnızca yapı sağlanır. Testler saf
host-native'dir ve hiçbir donanım/IDF bağımlılığı içermez.)
