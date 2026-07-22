> ⚠️ **BU DİZİN (repo kökü) ASIL ARAÇ FİRMWARE'İNİ İÇERMEZ.** Bu, bataryayı ilk keşif/test amaçlı kullanılan bir CAN sniffer aracıdır. Asıl AKS firmware'i için `ESP_AKS/` dizinine bakınız (`ESP_AKS/README.md`).

# TUFAN-AKS

TUFAN-AKS is the Vehicle Control Unit (VCU / AKS) firmware for the TUFAN electric vehicle platform. The project targets ESP32 with ESP-IDF through PlatformIO, uses FreeRTOS for task-based concurrency, and keeps all application code in C++17 without Arduino core dependencies.

## Build Environment

- PlatformIO environment: `env:esp32dev`
- Framework: ESP-IDF
- Language standard: C++17
- Main transport interfaces: CAN, SPI, UART

## Repository Layout

The current firmware repository is organized around seven runtime nodes / code areas:

1. `src/main.cpp`
Application entry point, task creation, watchdog refresh points, and inter-task queue wiring.

2. `src/VcuLogic.*`
Event-driven VCU state machine for `INIT`, `IDLE`, `READY`, `DRIVE`, `EMERGENCY_STOP`, and `FAULT`.

3. `lib/CanManager`
TWAI/CAN transport, motor driver communication, BMS frame parsing, and CAN-originated fault reporting.

4. `lib/RelayManager`
MCP23S17-based active-low relay control for the positive contactor bank.

5. `lib/DisplayHMI`
Nextion HMI UART transport and command handling.

6. `lib/Telemetry`
LoRa / telemetry packet formatting and UART uplink support.

7. `include/SystemConfig.h`
Shared pin map, message IDs, timing constants, LoRa mode defaults, and relay channel definitions.

## Current CAN Coverage

The active CAN message set is documented in [CAN_Message_Table.md](CAN_Message_Table.md).

Implemented frames:

- `0x100`: torque command
- `0x200`: motor status
- `0xE000`: Lithium Balance BMS config
- `0xE001`: Lithium Balance BMS live

## LoRa / E22 Baseline

The radio module is E22-400T30D-V2 (SX1268), a pin-compatible successor to
the retired E32-433T30D. Pin assignments are unchanged; the register-based
configuration protocol and config-mode pin levels are not (see
`include/E22Regs.h` for the address/value contract and
`lib/E22Config` for the pure command-build / response-parse helpers).

Startup mode for normal (transparent) operation:

- `M0 = 0`
- `M1 = 0`

Config mode (boot-time register sync against the contract):

- `M0 = 0`
- `M1 = 1`

Integration notes:

- Configure `LORA_M0_PIN` and `LORA_M1_PIN` before UART traffic starts.
- Use `LORA_AUX_PIN` as a readiness gate before telemetry transmission.
- Keep telemetry TX in transparent UART mode; radio register configuration
  happens once at boot in `vTask_LoRa_UKS` before entering the main loop.

## Relay Mapping Status

Röle katmanı artık şartname Bölüm 3'e (6.e.ii/6.e.iii sıcaklık uyarı flaşörü, 8.2.a S1/S2 kontaktör rolleri) + soğutma fanı (B3 7.a-b) + far (B2 9.19.c) rol makrolarıyla tanımlıdır — `RELAY_CH_S2_DRIVE=0` (sürüş hattı), `RELAY_CH_HVNEG=1` (HV−, S2 ile birlikte sürüş bankı), `RELAY_CH_HEADLIGHT=2` (far, bank dışı), OUT3-6 boş/yedek, `RELAY_CH_FAN=7` (soğutma fanı, bank dışı), `RELAY_CH_S1_CHARGE=8` (şarj hattı), `RELAY_CH_FLASHER=9` (uyarı flaşörü, bank dışı). Korna+silecek AKS DIŞI (donanımsal devre), fren lambası AKS'ye bağlanması YASAK (mekanik NC anahtar). Ayrıntılı tablo ve mod özeti: [RELAY_CHANNEL_TABLE.md](Documents/RELAY_CHANNEL_TABLE.md).

Doğrulama iki aşamalıdır: **Faz 1 (yazılım kanalı ↔ kart klemensi) DOĞRULANDI** — 2026-07-22, çıplak kartta (klemensler boş, HV ayrık) 10 kanal sırayla sürülüp durum LED'iyle eşlendi, 10/10 şemayla birebir uyuştu (ch0→OUT0/D8 … ch9→OUT9/D23; ⚠️ röle ref. K1/K3… klemens sırasıyla karışık — kablolamada OUT etiketine bakılır). **Faz 2 (klemens ↔ fiziksel yük kablolaması) HENÜZ DEĞİL** — donanım ekibi harness'i çekince tamamlanacak. Bu nedenle rol mantığı `RELAY_ROLES_ASSIGNED` derleme bayrağının (varsayılan **0**, `#warning` basar) arkasında KALIR; bayrak Faz 1 ile değil, yalnızca Faz 2 kablolaması bitince 1 yapılacak. LED/test-noktası tablosu ve Faz 2 yük atamaları: [RELAY_CHANNEL_TABLE.md](Documents/RELAY_CHANNEL_TABLE.md).

- **Bayrak=0 (varsayılan):** 10 kanalın tamamı tek pozitif kontaktör bankıdır (`RELAY_CONTACTOR_BANK_MASK=0x3FF`); `allOn()`/`allOff()` davranışı önceki sürümle bayt-bayt aynıdır. Flaşör / fan / far ve S1/S2 mantığı derlenmez.
- **Bayrak=1:** `RELAY_CONTACTOR_BANK_MASK=0x17B` (flaşör 9 + fan 7 + far 2 hariç) olur; `allOff()` güvenlik açması S1+S2+HV−+bank'ı açar ama flaşörü/fanı/farı söndürmez. Flaşör, doğrulanmış BMS sıcaklığından 55 °C'de yanar, 53 °C altında söner (`FLASHER_HYSTERESIS_C=2`). Soğutma fanı (şartname B3 7.a-b) aynı desenle 40 °C'de açılır, 35 °C'ye inince kapanır (`FAN_ON_TEMP_C`/`FAN_OFF_TEMP_C`, CONFIG); FAULT/E-STOP dahil her durumda çalışır (sıcak batarya soğutması kesilmez), bayat BMS verisinde dokunulmaz. Far (şartname B2 9.19.c) artık **fiziksel bir düğmeyle** (`HEADLIGHT_SWITCH_PIN`=GPIO27, doğrudan ESP32 GPIO + INPUT_PULLUP, SPI'dan bağımsız) kontrol edilir; ekran farı KONTROL ETMEZ, yalnız durumunu GÖSTERİR (`far.pic`, Nextion Picture bileşeni "far"). Düğme tipi `HEADLIGHT_SWITCH_LATCHING` (varsayılan 1 = kalıcı/anahtarlı, otomotiv normu — ESP reset'inde far anahtar konumundan geri gelir), debounce `HEADLIGHT_DEBOUNCE_MS`=40 ms; saf karar mantığı `lib/VcuLogic/HeadlightSwitch.h` (native test edilir). BMS'ten bağımsız, FAULT/E-STOP/READY'de korunur (bank dışı kanal). Eski ekran komutu 5 (`HEADLIGHT_TOGGLE`) KALDIRILDI ve kalıcı olarak rezerve edildi. S1/S2 mod anahtarlaması: şarjda (charger CAN akışı tazeyken, `TEL_chargerActive`) S1 kapalı + S2 açık ve READY reddedilir (8.2.a.iii); READY/DRIVE'da S1 açık + sürüş bankı (`RELAY_DRIVE_BANK_MASK=0x07B`) kapalı (8.2.a.vii); güvenlik probleminde hepsi açık (8.2.a.vi).

Sıcaklık eşikleri 55/70 °C (uyarı/kapanma, 15 °C sabit aralık — şartname 6.e.iii) `SystemConfig.h` ve `VcuLogic.h` içindeki `static_assert`'lerle derleme zamanında kilitlidir. Bayrak=1 varyantının testleri `pio test -e native_roles` ile çalışır (`test_roles_*` suite'leri).

**Bench'te yapılacaklar (HIL — kapsam dışı, native testlerle kanıtlanamaz):** G3 actuator geri-okuma doğrulaması (`RelayManager::verifyOutputs`, OLAT/IODIR readback + re-init/re-assert + actuator-fault) gerçek MCP23S17 ile bench'te doğrulanmalı: (1) çalışırken MCP23S17 gücünü kısa süre keserek brown-out reset tetikle, IODIR'in 0xFF'e döndüğünü ve VcuLogic'in FAULT'a geçtiğini osiloskopla/röle sesiyle teyit et; (2) MISO hattını fiziksel olarak ayırarak readback hatasında güvenli tarafa (kontaktör açık) düşüldüğünü doğrula.

## Contributors

Based on current git history, the repository contributors are:

- Sedat Ali Zevit - Seqat
- Şebnem Orel - sebnemorel
- incubation-0
- Mesalt-f4
- Order
- Nisa Köken - NisaKoken

## Development Rules

Contributor workflow and naming rules are documented in [CONTRIBUTING.md](CONTRIBUTING.md).

## Batarya Entegrasyonu Durumu

ESP-AKS ve Lithium Balance c-BMS entegrasyonu başarıyla devreye alınmıştır. Gerçek CAN sniffer loglarına göre yapılan son senkronizasyonların durumu aşağıdadır:

**Doğrulanan Veriler (DOĞRULANDI):**
- **0xE000**: Pack Voltajı (deciV), Pack Akımı (centiA, deşarjda negatif), SoC1 ve SoC2 (yüzde).
- **0xE001**: BMS Sıcaklığı (byte[6:7] üzerinden max/min seçimi) ve Hücre Özeti (byte[0:1]=min, byte[2:3]=max, byte[4:5]=avg hücre voltajı).
- **0xE015–0xE020**: 24 Hücrenin Bireysel Voltajları (her CAN frame 4 hücre barındırır, raw/10=mV).
- **0x1806E5F4**: Şarj cihazı gerilim ve akım hedefi (Charger command - Sadece okunuyor).

**Açık İşler ve Bilinmeyenler (BİLİNMİYOR):**
- **0xE002-0xE006**: BMS statik durumu ve limit parametreleri yayını olduğu düşünülüyor, alan anlamları tamamen çözülemedi. HMI karar mantığını etkilemez.
- **Bitrate Teyidi:** Gerçek araç testinde CAN bus hızının `500kbps` mi yoksa `125kbps` mi olacağı belirsizdir. `CanManager::begin()` içinde **otomatik hız bulucu (auto-baud)** devreye alınarak bu risk giderilmiştir (bkz. [BRING_UP_CHECKLIST.md](Documents/BRING_UP_CHECKLIST.md)).
