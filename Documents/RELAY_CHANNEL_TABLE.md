# Relay Channel Table

All relay outputs are driven through the MCP23S17 and are active-low at the hardware level.

Current software meaning:

- `RelayManager::allOn()` closes every channel in `RELAY_CONTACTOR_BANK_MASK`.
- `RelayManager::allOff()` de-energizes every channel in `RELAY_CONTACTOR_BANK_MASK` (safety open — şartname Bölüm 3, 8.2.a.vi).
- Channels **outside** the bank mask are untouched by `allOn`/`allOff`; their last commanded state is preserved in the shadow register and remains consistent with the `verifyOutputs()` readback check.

## `RELAY_ROLES_ASSIGNED` build flag (SystemConfig.h)

The channel→physical-load mapping has **not** been confirmed by the hardware team yet. The S1/S2/flasher role logic is therefore gated behind the compile-time flag `RELAY_ROLES_ASSIGNED` (default **0**):

| Flag | `RELAY_CONTACTOR_BANK_MASK` | Behavior |
| --- | --- | --- |
| `0` (default) | `0x3FF` (all 10 channels) | Legacy single-bank behavior, byte-for-byte identical to before. A `#warning` is emitted at compile time. Flasher / S1-S2 logic is compiled out. |
| `1` | `0x1FF` (channels 0-8, flasher excluded) | S1/S2 mode switching (şartname 8.2.a) + temperature warning flasher (şartname 6.e.ii) active. `RELAY_DRIVE_BANK_MASK = 0x0FF` (S2 + channels 1-7; S1 deliberately excluded so READY keeps S1 open). |

## Channel Map

| Channel Macro | Index | Role (when `RELAY_ROLES_ASSIGNED=1`) | Şartname | Physical Load |
| --- | --- | --- | --- | --- |
| `RELAY_CH_S2_DRIVE` (`RELAY_CH_POS_0`) | 0 | **S2 — drive-line contactor** | 8.2.a.vii: closed in drive; 8.2.a.iii: open while charging; 8.2.a.vi: open on safety problem | TBD during harness validation |
| `RELAY_CH_POS_1` … `RELAY_CH_POS_7` | 1-7 | Drive bank (switched together with S2 via `RELAY_DRIVE_BANK_MASK`) | — | TBD during harness validation |
| `RELAY_CH_S1_CHARGE` (`RELAY_CH_POS_8`) | 8 | **S1 — charge-line contactor** (closed in IDLE while charger CAN stream is fresh; open in READY/DRIVE; open on FAULT/E-STOP) | 8.2.a.iii / 8.2.a.vii / 8.2.a.vi | TBD during harness validation |
| `RELAY_CH_FLASHER` (`RELAY_CH_POS_9`) | 9 | **Temperature warning flasher** (audible+visual). Driven by `VcuLogic` from the verified BMS max temperature: ON at ≥55 °C, OFF below 53 °C (`FLASHER_HYSTERESIS_C=2`). **Outside** the contactor bank mask — `allOff()` never extinguishes it, so it stays on through FAULT/E-STOP while the temperature holds. | 6.e.ii / 6.e.iii | TBD during harness validation |

With `RELAY_ROLES_ASSIGNED=0` all ten channels are plain positive-contactor bank outputs (previous table).

## S1/S2 mode summary (`RELAY_ROLES_ASSIGNED=1`)

| Mode | S1 (ch 8) | S2 + drive bank (ch 0-7) | Source |
| --- | --- | --- | --- |
| IDLE, charger active (`TEL_chargerActive`) | CLOSED | OPEN (START_REQUEST rejected: "charger aktif — sarj modunda READY yasak") | 8.2.a.iii |
| READY / DRIVE | OPEN | CLOSED (`RELAY_DRIVE_BANK_MASK`, not `allOn`) | 8.2.a.vii |
| FAULT / EMERGENCY_STOP | OPEN | OPEN (`allOff` over `RELAY_CONTACTOR_BANK_MASK`) | 8.2.a.vi |

`TEL_chargerActive` is derived from the charger CAN stream freshness (`CanManager::updateChargerValidity`, `CAN_CHARGER_TIMEOUT_MS`) and is internal-only — it is never serialized into the LoRa TEL frame (19 fields, v2).

## Note

The physical output-to-load mapping still requires validation against the electrical drawing. Keep `RELAY_ROLES_ASSIGNED=0` until the hardware team confirms the harness; the S1/S2/flasher roles above are the software-side contract that will be enabled at that point.
