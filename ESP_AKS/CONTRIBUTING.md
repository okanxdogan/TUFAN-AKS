# ESP_AKS — Katkı Notları

## Kod yerleşim konvansiyonu (M5)

Tek kural: **saf (donanımsız) ve native test edilen her modül `lib/<Modül>/`
altında yaşar** — header'ı ve varsa `.cpp`'si aynı pakette. `include/` yalnızca
**proje-geneli config header'ları** içindir (ör. `SystemConfig.h`, `E22Regs.h`,
`VehicleParams.h`); test edilen modül header'ları oraya konmaz.

Böylece "modül nerede?" sorusunun tek yanıtı olur: PlatformIO LDF paketi
otomatik keşfeder, native testler `test/test_native_<modül>/` altında ilgili
`lib/` paketini derler. Örnek: `LinkMonitor` ve `LoraRxHandler` saf/test edilen
LoRa yardımcılarıdır → `lib/LoraLink/` içindedirler (`include/`'ta değil).
