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

## Nextion Object Names

The current firmware expects these object names on the Nextion page:

| Nextion Object | Type | Firmware Command |
| --- | --- | --- |
| `speed` | numeric | `speed.val=<value>` |
| `bat` | numeric | `bat.val=<value>` |
| `rpm` | numeric | `rpm.val=<value>` |
| `torque` | numeric | `torque.val=<value>` |
| `temp` | numeric | `temp.val=<value>` |
| `packv` | float (2 dp) | `packv.val=<deciV × 10>` |
| `packa` | float (2 dp) | `packa.val=<centiA>` |
| `state` | text | `state.txt="..."` |
| `motorErr` | text | `motorErr.txt="..."` |
| `valid` | text | `valid.txt="..."` |
| `contactor` | text | `contactor.txt="..."` |

### Float (xfloat) fields

`packv` and `packa` are Nextion **float** components with 2 decimal places
(`"00.00"`). A Nextion xfloat interprets the integer `.val` it receives as
`display_value × 100`. The firmware therefore scales before sending:

- `packv`: source is deci-volts (`× 0.1 V`), so `.val = deciV × 10` (e.g. `800` → `8000` → `80.00`).
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
- A Nextion field is only updated if its value changed or the screen is being populated for the first time.

## Command Inputs

Touch command IDs currently recognized by firmware:

| Command ID | Meaning |
| --- | --- |
| `1` | `START` |
| `2` | `RESET` |
| `3` | `EMERGENCY_STOP` |
| `4` | `DRIVE_ENABLE` |
