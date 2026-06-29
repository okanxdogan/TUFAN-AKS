# BMS Display Integration Plan

> **NOT (karar):** 24 hücreli gösterge zinciri **yalnızca simülasyon/demo** olarak
> bırakılmıştır. Gerçek Solion SK BMS, CAN 0x111/0x112 üzerinden 24 ayrı hücre
> DEĞİL yalnızca max/min özeti gönderir; gerçek BMS verisi `TelemetryData` ->
> `updateScreen` yolundan gösterilir. Bu nedenle aşağıdaki "Seçenek A/B"
> entegrasyonu UYGULANMAMIŞTIR ve `RealCellDataSource` bir stub olarak durur.

Rol 3 (Sistem Entegratörü) tarafından üretilmiştir. 24 hücreli batarya gösterge
arayüzünün firmware-tarafı entegrasyon planı. Bu doküman SADECE plan ve
sözleşmedir; `main.cpp`, `platformio.ini`, `SystemConfig.h` değiştirilmemiştir —
orchestrator final entegrasyonda uygular.

## 1. Veri Akış Şeması

```
[Rol 1: Sahte Veri Üreteci]        [Gerçek BMS (CAN/UART)]
   SimCellDataSource                  RealCellDataSource
            \                               /
             \  (HAL: ICellDataSource)     /
              \___________________________/
                          |
                   BmsPackData (ham, yorumlanmamış)
                          |
                [Rol 2: BmsAlgo::computePack()]
                          |
                   BmsComputed (yorumlanmış: packVoltage, delta,
                                soc, balanceFlag[24], warningLevel)
                          |
            [buildBmsNextionCommands()] (saf, UART'a yazmaz)
                          |  emit callback
                   uart_write_bytes + HMI_sendEndBytes (0xFF 0xFF 0xFF)
                          |
                   [Nextion Ekran — 24 hücre bar + soc + delta + uyarı]
```

Ana kart, verinin simülatörden mi gerçek BMS'ten mi geldiğini AYIRT EDEMEZ
(HAL kuralı). Yalnızca `ICellDataSource` arayüzünü tanır.

## 2. Modül Sahiplikleri

| Modül | Yol | Sahip | Sorumluluk |
| --- | --- | --- | --- |
| `BmsModel` | `lib/BmsModel/BmsModel.h` | Paylaşılan (orchestrator) | `BmsPackData`, `BMS_CELL_COUNT`, `ICellDataSource` sözleşmesi |
| `BmsSim` | `lib/BmsSim/` | Rol 1 | `SimCellDataSource` (senaryo A/B/C), `RealCellDataSource` |
| `BmsAlgo` | `lib/BmsAlgo/` | Rol 2 | `computePack()`, `BmsComputed`, `buildBmsNextionCommands()` |
| Edge testleri | `test/test_native_bms_edge/` | Rol 3 | Sözleşme edge case doğrulaması |
| Dokümantasyon | `Documents/BMS_Display_*.md` | Rol 3 | Edge case / risk + entegrasyon planı |

## 3. main.cpp Entegrasyonu — FreeRTOS Task

Mevcut `main.cpp` task'ları (referans): `vTask_CAN_Comm` (prio 5),
`vTask_HMI_Display` (prio 2, 10 Hz), `vTask_VCU_Logic` (prio 10),
`vTask_LoRa_UKS` (prio 8). HMI task `xTaskCreatePinnedToCore` ile, 4096 stack ile
oluşturuluyor.

İki seçenek:

### Seçenek A (Önerilen) — mevcut `vTask_HMI_Display` genişletilir
- Aynı 10 Hz döngüde, mevcut HMI snapshot mantığından sonra BMS bloğu eklenir.
- Avantaj: tek UART (Nextion) tek task tarafından sürülür — yarış yok, ek
  senkronizasyon gerekmez.
- Akış (her döngüde):
  1. `source.read(raw)` — `ICellDataSource` global/static örnek
  2. `if (!raw.isValid) { /* TIMEOUT/INVALID alanlarını yaz */ }`
  3. `BmsComputed comp = computePack(raw);`
  4. `buildBmsNextionCommands(comp, raw, emitCb, ctx);` (mevcut change-cache
     kalıbı korunabilir — bkz. `HMI_sendNumericIfChanged`)

### Seçenek B — ayrı `vTask_BMS_Display`
- Ayrı task (örn. prio 2, 4096 stack, 5–10 Hz), kendi `ICellDataSource` örneği.
- UART paylaşımı için mutex GEREKİR (Nextion UART'ı HMI task ile ortak).
- Sadece BMS güncellemesi yüksek hızda gerekiyorsa tercih edilir; aksi halde
  Seçenek A daha basit ve güvenlidir.

### Timeout / stale koruması (R4)
Motor tarafındaki `TEL_motorTimeoutActive` kalıbına benzer: son geçerli
`read()` üzerinden geçen tick sayılır; eşik aşılınca `warntxt.txt="TIMEOUT"` ve
hücre barları "stale" göstergesine alınır.

## 4. platformio.ini — Native Test Env Include'ları

`test_native_bms_edge` SADECE `BmsModel.h`'ye bağlıdır; ek bir kütüphane
linklenmesi GEREKMEZ. Mevcut `[env:native]` blokuna eklenecek include yolu:

```ini
[env:native]
; ... mevcut build_flags ...
build_flags =
    -std=gnu++17
    -I include
    -I src
    -I lib/CanManager
    -I lib/Telemetry
    -I lib/DisplayHMI
    -I lib/RelayManager
    -I lib/BmsModel        ; <-- EKLE: BmsModel.h (test_native_bms_edge için zorunlu)
    -I test/support/idf_stubs
    -I test/support
    -D VCU_LOGIC_TESTABLE
    -D NATIVE_BUILD
```

Notlar:
- `test_filter = test_native_*` zaten `test_native_bms_edge`'i kapsar; ek filtre
  gerekmez.
- `bms_edge_helpers.h` test dizini içinde olduğundan ek `-I` gerektirmez.
- BmsModel header-only olduğundan `lib_ignore`'a dokunmaya gerek yoktur.
- Tam entegrasyon (BmsAlgo/BmsSim'i de native test etmek) istenirse ayrıca
  `-I lib/BmsAlgo -I lib/BmsSim` eklenir; bu edge-case env'i için GEREKLİ DEĞİL.

## 5. Önerilen Nextion Ekran Alanları

`HMI_Field_Map.md` formatına uygun. `buildBmsNextionCommands()` çıktısı ile
birebir eşleşir (komut gövdesi; 0xFF 0xFF 0xFF end-byte'lar firmware ekler).

### Nextion Object Names

| Nextion Object | Type | Firmware Command | Kaynak (BmsComputed) |
| --- | --- | --- | --- |
| `cell0` .. `cell23` | numeric | `cellN.val=<mV>` | `raw.cellVoltageMv[N]` (demo) |
| `j0` .. `j23` | progress bar | `jN.val=<0..100>` | `raw.cellVoltageMv[N]` -> doluluk (demo) |
| `bal0` .. `bal23` | numeric (0/1) | `balN.val=<0\|1>` | `comp.balanceFlag[N]` |
| `cellmax` | numeric | `cellmax.val=<mV>` | **ŞİMDİLİK DEMO:** `comp.cellMaxMv`. Gerçek zamanlıya geçişte `TEL_bmsCellVoltageMaxDeciMv/10` (updateScreen, `BMS_USE_REALTIME_MINMAX`) |
| `cellmin` | numeric | `cellmin.val=<mV>` | **ŞİMDİLİK DEMO:** `comp.cellMinMv`. Gerçek zamanlıya geçişte `TEL_bmsCellVoltageMinDeciMv/10` (updateScreen, `BMS_USE_REALTIME_MINMAX`) |
| `warn` | numeric | `warn.val=<0\|1\|2>` | `comp.warningLevel` |
| `warntxt` | text | `warntxt.txt="OK\|WARN\|CRIT"` | `bmsWarningText(warningLevel)` |

### 24 Hücre Bar Önerisi
- 24 ayrı progress bar (`j0`..`j23`) veya `cellN.val` numeric alan + bar.
- Bar dolum oranı: `(cellMv - 3000) / (4200 - 3000)` * 100, 0..100'e clamp.
- `balN`=1 olan hücre barı vurgulanır (renk/ikon) — dengeleme aktif göstergesi.

### Uyarı / Tehlike Alanı
- `warntxt`: OK (yeşil) / WARN (sarı) / CRIT (kırmızı).
- `isValid=false` veya timeout: ayrı `bmsvalid.txt="INVALID"/"TIMEOUT"` alanı
  (motor tarafındaki `valid` kalıbının BMS karşılığı).

## 6. Açık Aksiyonlar (Orchestrator)

1. **R1 (KRİTİK):** `BmsComputed.packVoltageMv` tipini `uint32_t` (mV) veya
   `uint16_t` deciV'e çevir — aksi halde toplam gerilim NORMAL çalışmada bile
   sessizce sarılır. (Detay: `BMS_Display_Edge_Cases.md` R1.)
2. `platformio.ini [env:native]` -> `-I lib/BmsModel` ekle.
3. HMI task'ını Seçenek A ile genişlet; BMS timeout sayacı ekle.
4. Nextion HMI dosyasını (Object name'leri yukarıdaki tabloya göre) KULLANICI
   oluşturur.
