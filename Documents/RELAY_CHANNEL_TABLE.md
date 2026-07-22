# Relay Channel Table

All relay outputs are driven through the MCP23S17 and are active-low at the hardware level.

Current software meaning:

- `RelayManager::allOn()` closes every channel in `RELAY_CONTACTOR_BANK_MASK`.
- `RelayManager::allOff()` de-energizes every channel in `RELAY_CONTACTOR_BANK_MASK` (safety open — şartname Bölüm 3, 8.2.a.vi).
- Channels **outside** the bank mask are untouched by `allOn`/`allOff`; their last commanded state is preserved in the shadow register and remains consistent with the `verifyOutputs()` readback check. Flasher (OUT9), cooling fan (OUT7) and headlight (OUT2) are all out-of-bank.

## Channel decisions (hardware-team approved)

| OUT | Function | Bank membership |
| --- | --- | --- |
| OUT0 | **S2 — drive-line contactor** | Drive bank + contactor bank |
| OUT1 | **HV− contactor** — opens/closes together with S2 in the drive bank | Drive bank + contactor bank |
| OUT2 | **Headlight (far)** — controlled by a **physical switch** (`HEADLIGHT_SWITCH_PIN` = GPIO27, INPUT_PULLUP), BMS-independent; the screen only **shows** its status (`far.pic`) | **Out of bank** |
| OUT3–OUT6 | empty / spare | Contactor bank (drive bank) |
| OUT7 | **Cooling fan** — automatic, temperature-driven | **Out of bank** |
| OUT8 | **S1 — charge-line contactor** | Contactor bank (not drive bank) |
| OUT9 | **Warning flasher** (audible+visual) | **Out of bank** |

**Not connected to the AKS (documented here, no firmware):**
- **Horn (korna) + wiper (silecek): AKS DIŞI — donanımsal devre (B2 9.17 / B2 9.12.c).** Wired as a stand-alone hardware circuit; the AKS drives no channel for them.
- **Brake lamp (fren lambası): AKS'ye bağlanması YASAK — B2 9.19.b + 9.7.f-g (mekanik NC anahtar zorunlu).** Must be driven by a mechanical normally-closed switch, never by firmware.

## `RELAY_ROLES_ASSIGNED` build flag (SystemConfig.h)

The channel→physical-load mapping has **not** been confirmed by the hardware team yet. The S1/S2/flasher/fan/headlight role logic is therefore gated behind the compile-time flag `RELAY_ROLES_ASSIGNED` (default **0**):

| Flag | `RELAY_CONTACTOR_BANK_MASK` | Behavior |
| --- | --- | --- |
| `0` (default) | `0x3FF` (all 10 channels) | Legacy single-bank behavior, byte-for-byte identical to before. A `#warning` is emitted at compile time. Flasher / fan / headlight / S1-S2 logic is compiled out. |
| `1` | `0x17B` (flasher 9 + fan 7 + headlight 2 excluded) | S1/S2 mode switching (şartname 8.2.a) + temperature warning flasher (6.e.ii) + cooling fan (B3 7.a-b) + headlight toggle (B2 9.19.c) active. `RELAY_DRIVE_BANK_MASK = 0x07B` (S2 + HV− + spares 3-6; S1, fan and headlight excluded so READY keeps S1 open and never touches fan/headlight). |

## Channel Map

| Channel Macro | Index | Role (when `RELAY_ROLES_ASSIGNED=1`) | Şartname | Physical Load |
| --- | --- | --- | --- | --- |
| `RELAY_CH_S2_DRIVE` (`RELAY_CH_POS_0`) | 0 | **S2 — drive-line contactor** | 8.2.a.vii: closed in drive; 8.2.a.iii: open while charging; 8.2.a.vi: open on safety problem | TBD during harness validation |
| `RELAY_CH_HVNEG` (`RELAY_CH_POS_1`) | 1 | **HV− contactor** — drive-bank member; opens/closes together with S2 | 8.2.a | TBD during harness validation |
| `RELAY_CH_HEADLIGHT` (`RELAY_CH_POS_2`) | 2 | **Headlight (far)** — driven by `VcuLogic` from a **physical switch** on `HEADLIGHT_SWITCH_PIN` (**GPIO27**, direct ESP32 GPIO with INPUT_PULLUP; active-low, switch to GND). Debounce `HEADLIGHT_DEBOUNCE_MS`=40 ms; switch type `HEADLIGHT_SWITCH_LATCHING` (default **1** = latching/maintained). Latching → far follows the switch **position** (survives ESP reset — if the switch is still "on", far comes back on). BMS-independent. **Outside** the bank mask — `allOff`/`allOn` (FAULT/E-STOP/READY) never change it. The screen no longer controls the headlight; it only **shows** the state (`far.pic`). Pure decision logic: `lib/VcuLogic/HeadlightSwitch.h`. | B2 9.19.c | TBD during harness validation |
| `RELAY_CH_POS_3` … `RELAY_CH_POS_6` | 3-6 | empty / spare (drive bank) | — | Boş/yedek |
| `RELAY_CH_FAN` (`RELAY_CH_POS_7`) | 7 | **Cooling fan** — driven by `VcuLogic` from the verified BMS max temperature: ON at ≥40 °C (`FAN_ON_TEMP_C`), OFF at ≤35 °C (`FAN_OFF_TEMP_C`). **Outside** the bank mask — stays on through FAULT/E-STOP so a hot pack keeps cooling. Stale/timed-out BMS data leaves it untouched. | B3 7.a-b | TBD during harness validation |
| `RELAY_CH_S1_CHARGE` (`RELAY_CH_POS_8`) | 8 | **S1 — charge-line contactor** (closed in IDLE while charger CAN stream is fresh; open in READY/DRIVE; open on FAULT/E-STOP) | 8.2.a.iii / 8.2.a.vii / 8.2.a.vi | TBD during harness validation |
| `RELAY_CH_FLASHER` (`RELAY_CH_POS_9`) | 9 | **Temperature warning flasher** (audible+visual). Driven by `VcuLogic` from the verified BMS max temperature: ON at ≥55 °C, OFF below 53 °C (`FLASHER_HYSTERESIS_C=2`). **Outside** the contactor bank mask — `allOff()` never extinguishes it, so it stays on through FAULT/E-STOP while the temperature holds. | 6.e.ii / 6.e.iii | TBD during harness validation |

With `RELAY_ROLES_ASSIGNED=0` all ten channels are plain positive-contactor bank outputs (previous table).

## S1/S2 mode summary (`RELAY_ROLES_ASSIGNED=1`)

| Mode | S1 (ch 8) | S2 + HV− + drive bank (ch 0,1,3-6) | Source |
| --- | --- | --- | --- |
| IDLE, charger active (`TEL_chargerActive`) | CLOSED | OPEN (START_REQUEST rejected: "charger aktif — sarj modunda READY yasak") | 8.2.a.iii |
| READY / DRIVE | OPEN | CLOSED (`RELAY_DRIVE_BANK_MASK`, not `allOn`) | 8.2.a.vii |
| FAULT / EMERGENCY_STOP | OPEN | OPEN (`allOff` over `RELAY_CONTACTOR_BANK_MASK`) | 8.2.a.vi |

Fan (OUT7) and headlight (OUT2) are **independent of the S1/S2 mode** — they are outside the bank mask, so none of the transitions above changes them. The fan tracks the verified BMS max temperature (40 °C ON / 35 °C OFF, hysteresis) in every state including FAULT/E-STOP; the headlight follows the driver's physical switch (`HEADLIGHT_SWITCH_PIN`), independent of BMS and vehicle state.

## Headlight physical switch (`HEADLIGHT_SWITCH_PIN`, `RELAY_ROLES_ASSIGNED=1`)

The headlight (OUT2) is controlled by a **physical driver switch**, not the touchscreen (şartname B2 9.19.c: "farlar sürücünün basacağı bir düğme ile açılıp kapanabilmeli"). The screen now only **displays** the state (`far.pic`), it never controls the light.

| Setting | Value | Notes |
| --- | --- | --- |
| `HEADLIGHT_SWITCH_PIN` | **GPIO27** | Direct ESP32 GPIO, INPUT_PULLUP. Chosen over the MCP23S17 J22 GPB4-GPB7 fallback so the input path is **independent of SPI** (works even if the relay expander resets). Free pin, not a strapping pin, supports internal pull-up. **CONFIG — awaiting hardware-team confirmation** (they route the switch wiring to this pin). |
| `HEADLIGHT_SWITCH_ACTIVE_LEVEL` | `0` | Active-low: switch to GND → LOW = "on" position / pressed. |
| `HEADLIGHT_SWITCH_LATCHING` | `1` (default) | `1` = maintained/latching (automotive norm, recommended): far follows the switch **position**; after an ESP reset the far comes back on if the switch is still on → post-reset desync impossible. `0` = momentary: toggle on the press edge, far OFF at boot. |
| `HEADLIGHT_DEBOUNCE_MS` | `40` | Read at the VCU task period (20 ms); unstable transitions are ignored. |

Wiring: connect the switch between GPIO27 and GND. The pure decision logic (debounce + latching/momentary) lives in `lib/VcuLogic/HeadlightSwitch.h` (native-tested); `main.cpp` binds `gpio_get_level(HEADLIGHT_SWITCH_PIN)` to the `VcuLogic` reader hook.

`TEL_chargerActive` is derived from the charger CAN stream freshness (`CanManager::updateChargerValidity`, `CAN_CHARGER_TIMEOUT_MS`) and is internal-only — it is never serialized into the LoRa TEL frame (19 fields, v2).

## Faz 1 — kanal↔klemens doğrulaması (DOĞRULANDI, 2026-07-22)

**Yöntem:** Çıplak kartta (klemensler boş, HV ayrık), her yazılım kanalı sırayla tek tek sürüldü ve kartın durum LED'i ile eşlendi. **10 kanalın 10'u da şemayla BİREBİR uyuştu** — kart çizildiği gibi üretilmiş, sapma yok.

| Kanal | Klemens | Röle ref. | Durum LED'i | Test noktası |
| --- | --- | --- | --- | --- |
| ch0 | OUT0 | K1 | D8 | TP3 |
| ch1 | OUT1 | K3 | D13 | TP5 |
| ch2 | OUT2 | K6 | D19 | TP9 |
| ch3 | OUT3 | K9 | D24 | TP11 |
| ch4 | OUT4 | K2 | D9 | TP4 |
| ch5 | OUT5 | K4 | D14 | TP6 |
| ch6 | OUT6 | K7 | D20 | TP8 |
| ch7 | OUT7 | K10 | D25 | TP12 |
| ch8 | OUT8 | K5 | D15 | TP7 |
| ch9 | OUT9 | K8 | D23 | TP10 |

**⚠️ UYARI:** Röle referansları (K1, K3, K6 …) klemens sırasıyla **KARIŞIK** (ör. OUT0=K1 ama OUT4=K2, OUT8=K5). Kablo bağlarken röle numarasına (Kx) **DEĞİL**, klemens **OUT etiketine** bakılacak.

## Faz 2 — kablolama talimatı (yük atamaları, henüz çekilmedi)

Donanım ekibi harness'i çekerken her klemense aşağıdaki yük bağlanacak (klemens = **OUT etiketi**, röle numarası değil):

| Klemens | Fiziksel yük |
| --- | --- |
| OUT0 | S2 sürüş kontaktörü |
| OUT1 | HV− kontaktörü |
| OUT2 | Far |
| OUT3–OUT6 | boş / yedek |
| OUT7 | Soğutma fanı |
| OUT8 | S1 şarj kontaktörü |
| OUT9 | Flaşör |

Ayrıca **far düğmesi**: `HEADLIGHT_SWITCH_PIN` = **GPIO27** ile **GND** arasına bağlanacak (INPUT_PULLUP, aktif-düşük).

## Note

Doğrulama iki aşamalıdır; bu ikisi ayrı tutulmalıdır:

- **DOĞRULANDI (Faz 1, bu test — 2026-07-22):** yazılım kanalı `N` ↔ kart klemensi `OUT N` eşlemesi. Çıplak kartta LED tablosuyla 10/10 birebir uyuştu (yukarı).
- **HENÜZ DEĞİL (Faz 2, donanım ekibi):** kart klemensi ↔ fiziksel yük kablolaması. Harness çekilene kadar açık.

Bu nedenle **`RELAY_ROLES_ASSIGNED=0` KALIR** — bayrak Faz 1 ile açılmaz; yalnızca Faz 2 kablolaması tamamlandıktan sonra 1 yapılacak. Yukarıdaki S1/S2/flaşör rolleri, o noktada etkinleşecek yazılım tarafı sözleşmedir.
