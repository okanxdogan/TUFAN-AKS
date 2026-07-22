# HMI Field Map

This document defines the current AKS -> Nextion field mapping used by the firmware in Phase 4.

## Runtime Data Model

The firmware now builds one `HMI_DisplayData` snapshot per HMI refresh cycle.

Fields currently included:

| Firmware Field | Source | Description |
| --- | --- | --- |
| `HMI_currentSpeed` | `TEL_motorRpm` | Main speed / RPM value shown on screen |
| `HMI_currentBattery` | `TEL_bmsSoc` | Battery state of charge |
| `HMI_motorRpm` | `TEL_motorRpm` | Raw motor RPM |
| `HMI_motorTorqueFeedback` | `TEL_motorTorqueFeedback` | Signed torque feedback |
| `HMI_motorErrorFlags` | `TEL_motorErrorFlags` | Motor driver error flags |
| `HMI_motorDataValid` | `TEL_motorDataValid` | Freshness indicator |
| `HMI_motorTimeoutActive` | `TEL_motorTimeoutActive` | Timeout indicator |
| `HMI_bmsTemperatureC` | `TEL_bmsTemperatureC` | BMS temperature |
| `HMI_bmsPackVoltageDeciV` | `TEL_bmsPackVoltageDeciV` | BMS pack voltage |
| `HMI_bmsPackCurrentCentiA` | `TEL_bmsCurrentCentiA` | BMS pack current |
| `HMI_contactorClosed` | `RelayManager::getRelayState()` | True if all positive contactors are closed |
| `HMI_vcuState` | `VcuLogic::getState()` | Current VCU state |
| `HMI_headlightOn` | `VcuLogic::isHeadlightOn()` | Headlight state (physical switch). Screen only **shows** it; `false` whenever `RELAY_ROLES_ASSIGNED=0` (honest state) |

## Nextion Object Names

The current firmware expects these object names on the Nextion page:

| Nextion Object | Type | Firmware Command |
| --- | --- | --- |
| `speed` | numeric | `speed.val=<value>` |
| `bat` | numeric | `bat.val=<value>` |
| `rpm` | numeric | `rpm.val=<value>` |
| `torque` | numeric | `torque.val=<value>` |
| `temp` | numeric | `temp.val=<value>` |
| `packv` | float (1 dp) | `packv.val=<deciV>` |
| `packa` | float (2 dp) | `packa.val=<centiA>` |
| `state` | text | `state.txt="..."` |
| `motorErr` | text | `motorErr.txt="..."` |
| `valid` | text | `valid.txt="..."` |
| `contactor` | text | `contactor.txt="..."` |
| `far` | **Picture** | `far.pic=<ID>` — headlight status indicator (see below) |

### Float (xfloat) fields

`packv` is a Nextion **float** component with 1 decimal place (`"00.0"`) and
`packa` a **float** component with 2 decimal places (`"00.00"`). A Nextion
xfloat interprets the integer `.val` it receives as `display_value × 10^(decimal places)`:

- `packv`: source is deci-volts (`× 0.1 V`), which already equals `V × 10`, so `.val` is sent unscaled (e.g. `790` → `79.0`).
- `packa`: source is centi-amps (`× 0.01 A`), which already equals `A × 100`, so `.val` is sent unscaled (e.g. `1250` → `12.50`, `-2000` → `-20.00`).

Scaling lives in `HMI_packVoltageToXfloat` / `HMI_packCurrentToXfloat`
(`HMIHelpers.h`). `temp` remains an integer `number` component and is not scaled.

## Text Formatting Rules

| UI Field | Output |
| --- | --- |
| `state` | `INIT`, `IDLE`, `READY`, `DRIVE`, `ESTOP`, `FAULT` |
| `motorErr` | Hex string such as `0x00`, `0x04`, `0xFF` |
| `valid` | `VALID`, `INVALID`, or `TIMEOUT` |
| `contactor` | `CLOSED` or `OPEN` |

## Refresh Behavior

- HMI refresh task runs at `10 Hz`.
- The display driver caches the last transmitted snapshot.
- A Nextion field is only updated if its value changed, the screen is being populated for the first time, or its round-robin resync slot is due (see below).

### Nextion reset (brown-out) recovery

A Nextion brown-out/reset reverts every component to its Editor defaults while
the ESP-side caches (`HMI_lastScreenData`, `BmsNextionCache`) still hold the
last transmitted values — without recovery, unchanged fields would never be
re-sent and the screen would stay at defaults (observed in the field).

The firmware detects the reset and repopulates the screen:

- **Detection** — the Nextion *Startup* event (`0x00 0x00 0x00 0xFF 0xFF 0xFF`)
  it emits on power-up is caught by a pure byte-stream state machine
  (`lib/DisplayHMI/NextionResetDetect.h`) wired in parallel to the
  `readTouchCommand()` RX path. It tolerates fragmented arrival and does not
  interfere with the `0x5A CMD ~CMD` touch-frame parser.
- **Recovery** — on detection the driver (`DisplayHMI::HMI_handleNextionReset`):
  1. re-sends `bkcmd=0` (it is not persistent across a Nextion reset),
  2. invalidates its scalar-field cache (`forceFullRefresh()`), so the next
     `updateScreen()` re-sends all fields exactly like the first call after boot,
  3. raises a one-shot flag consumed by the HMI task via
     `DisplayHMI::consumeResetFlag()`, which resets `BmsNextionCache` and
     re-arms `BMS_firstRun` — this flag affects **only** the 24-cell BMS panel.
- **TX budget** — the cell/bar/balance repopulation is spread across multiple
  10 Hz cycles by the existing `buildBmsNextionCommands` `maxBytes=90` budget
  (cache sentinels + `isWarm=false`, same mechanism as boot), keeping each
  cycle within the 9600-baud UART budget (~96 bytes per 100 ms).
- **Logging** — a rate-limited WARN (`Nextion reset algilandi ...`, at most one
  per `HMI_RESET_WARN_LOG_INTERVAL_MS`, total counter included) is emitted.

This recovery is fully local to the HMI path; LoRa telemetry
(`lib/Telemetry`, `UplinkScheduler`) is unaffected.

### Round-robin resync (safety net for undetected resets)

The Startup event itself can be corrupted or lost while the supply is
collapsing (the RX line is unreliable during a brown-out), in which case the
reset detector never fires. A periodic, event-independent resync layer covers
this blind spot (`lib/DisplayHMI/ResyncPolicy.h`):

- Every `HMI_RESYNC_INTERVAL_MS` (default 500 ms, `SystemConfig.h`) exactly
  **one** scalar field is force-sent regardless of the change cache, then the
  rotation advances: `speed → bat → rpm → torque → temp → packv → packa →
  state → motorErr → valid → contactor → far (headlight pic) → back to start`.
- No bursts: one command (≤ `HMI_RESYNC_CMD_MAX_BYTES` = 26 B) per trigger,
  ~52 B/s at the default interval — enforced against the 9600-baud budget by a
  `static_assert` in `SystemConfig.h`. The `static_assert` formula is
  **independent of the field count** (one field per trigger), so growing the
  rotation from 11 to 12 fields does not change the peak UART load.
- Worst-case self-heal time after an undetected reset:
  `12 fields × 500 ms = 6 s`. This is exactly why the headlight is displayed
  through a resync-covered field and the screen holds **no** local headlight
  state: after a Nextion brown-out the `far.pic` indicator re-syncs to the real
  ESP-owned state within one rotation instead of showing a stale icon.

The 24-cell BMS panel has its own rotation
(`lib/BmsAlgo/BmsNextionPacket.h::bmsNextionCacheInvalidateSlot`):

- Every `BMS_RESYNC_INTERVAL_MS` (default 1000 ms) one of 27 slots (24 cell
  triples `cellN/jN/balN` + `cellmax` + `cellmin` + `warn`) has its
  `BmsNextionCache` entries invalidated with values that cannot occur in
  production (65534 mV / 255); the existing change-compare + `maxBytes=90`
  path then re-emits that slot.
- Invalidation is *sticky*: if the byte budget runs out in a cycle, the cache
  mismatch persists and the slot is re-emitted on a later cycle — a resync is
  never lost.
- Cell slots wait for the next 1 Hz `updateCells` tick (≤ 1 s extra latency);
  summary slots re-emit on the next cycle.
- Worst-case self-heal time for the full panel:
  `27 slots × 1000 ms + ~2 s tail ≈ 29 s` (the safety-critical scalar fields
  above heal in ~5.5 s; the slower detail-panel tour is a deliberate budget
  trade-off, proven by a combined `static_assert` in `SystemConfig.h`:
  52 B/s + 48 B/s ≤ 15% of the 960 B/s raw UART capacity).

## Command Inputs

Touch commands are 3-byte frames `0x5A <CMD> <~CMD>` (header, command, bitwise-NOT
checksum) parsed by `DisplayHMI::readTouchCommand`. IDs currently recognized by firmware:

| Command ID | Meaning | Full frame (header CMD ~CMD) |
| --- | --- | --- |
| `1` | `START` | `0x5A 0x01 0xFE` |
| `2` | `RESET` | `0x5A 0x02 0xFD` |
| `3` | `EMERGENCY_STOP` | `0x5A 0x03 0xFC` |
| `4` | `DRIVE_ENABLE` | `0x5A 0x04 0xFB` |
| `5` | **UNUSED — RESERVED** (do not reassign) | — |

### Command `5` — unused / reserved

Command `5` was previously `HEADLIGHT_TOGGLE` (frame `0x5A 0x05 0xFA`). The
headlight is now controlled by a **physical switch** (`HEADLIGHT_SWITCH_PIN`,
şartname B2 9.19.c), so the screen no longer sends any headlight command — the
firmware no longer handles command 5. The ID is **kept permanently free**: if it
were reassigned to another command, an old screen project still emitting
`0x5A 0x05 0xFA` would trigger the wrong action. Command IDs 1-4 remain
START/RESET/EMERGENCY_STOP/DRIVE_ENABLE.

### `far` (Picture) — headlight status indicator contract

The headlight is displayed with a Nextion **Picture** component, **objname
`far`**. The firmware sends `far.pic=<ID>` where `<ID>` is
`HMI_PIC_HEADLIGHT_ON` / `HMI_PIC_HEADLIGHT_OFF` (`SystemConfig.h`, **CONFIG** —
placeholder `1`/`0`, to be replaced with the real Nextion resource IDs once the
screen project is prepared). The command is generated by the pure helper
`HMI_formatPicCommand` (`HMIHelpers.h`, native-tested) and gated by the same
change-compare + resync path as every other field (sent when the value changes,
on the first refresh, or when the `HMI_RESYNC_HEADLIGHT` resync slot is due).

**The screen does NOT control the headlight and holds NO local headlight
state.** The single owner of the state is the ESP (`VcuLogic::isHeadlightOn`,
driven by the physical switch). This is deliberate: a Nextion brown-out reset is
an observed event in this system — if the screen kept its own headlight state,
the icon would diverge from reality after a reset. Because the ESP owns the
state and it rides the round-robin resync (`far` is a resync field), the
indicator re-syncs to the true state within one resync rotation (≤ 6 s) instead
of showing a stale icon. The Picture component's touch event must be left
**empty** in the Nextion project. See `RELAY_CH_HEADLIGHT` and the headlight
switch table in [RELAY_CHANNEL_TABLE.md](RELAY_CHANNEL_TABLE.md).
