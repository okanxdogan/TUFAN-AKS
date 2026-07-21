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
| OUT2 | **Headlight (far)** — toggled by a screen button, BMS-independent | **Out of bank** |
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
| `RELAY_CH_HEADLIGHT` (`RELAY_CH_POS_2`) | 2 | **Headlight (far)** — toggled by the screen button (`HMI_CMD_HEADLIGHT_TOGGLE=5`, frame `0x5A 0x05 0xFA`). Boot OFF, BMS-independent. **Outside** the bank mask — `allOff`/`allOn` (FAULT/E-STOP/READY) never change it. | B2 9.19.c | TBD during harness validation |
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

Fan (OUT7) and headlight (OUT2) are **independent of the S1/S2 mode** — they are outside the bank mask, so none of the transitions above changes them. The fan tracks the verified BMS max temperature (40 °C ON / 35 °C OFF, hysteresis) in every state including FAULT/E-STOP; the headlight only changes on a screen-button toggle.

`TEL_chargerActive` is derived from the charger CAN stream freshness (`CanManager::updateChargerValidity`, `CAN_CHARGER_TIMEOUT_MS`) and is internal-only — it is never serialized into the LoRa TEL frame (19 fields, v2).

## Note

The physical output-to-load mapping still requires validation against the electrical drawing. Keep `RELAY_ROLES_ASSIGNED=0` until the hardware team confirms the harness; the S1/S2/flasher roles above are the software-side contract that will be enabled at that point.
