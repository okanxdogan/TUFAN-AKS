# LoRa Link Analysis

This note captures the current AKS-side telemetry bandwidth estimate and the practical assumptions for Phase 3 reliability work.

## Current AKS Telemetry Payload

Current uplink format (TEKNOFEST-compliant):

- ASCII semicolon-separated line
- Fields: `zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh`
- Terminator: `\r\n`
- Rate: `5 Hz`

Representative packet:

```text
10000;0;32;780;0\r\n
```

Typical payload size is approximately `15-25 bytes` depending on numeric field widths.
A conservative planning budget of `30 bytes` per packet gives:

- `30 bytes * 5 Hz = 150 bytes/s`
- `150 bytes/s * 10 bits/byte ~= 1500 bit/s` on the UART side including start/stop overhead

## Interpretation

Important distinction:

- The ESP32 <-> E32 UART link is configured for `9600 baud`.
- The radio air data rate of the E32 module may be lower than the UART baud.
- Therefore, final field testing must confirm that the selected E32 radio configuration can drain the UART input fast enough at the chosen packet rate.

## Current Reliability Policy

Implemented now:

- TEKNOFEST-compliant telemetry format: `zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh` (semicolon-separated)
- Millisecond timestamp from boot (monotonic clock) replaces sequence counter
- LoRa RX restricted to E-Stop only (TEKNOFEST rule: UKS-to-vehicle is E-Stop-only)
- OfflineBuffer ring buffer (300 entries = 60 s at 5 Hz) retains telemetry during link loss
- Link-down detection via heartbeat timeout (3 s); buffered packets replayed FIFO on reconnect
- No AKS retransmission
- No AKS-level ACK handling
- Latest-sample-wins behavior for live telemetry
- AUX gate checked before each TX attempt

## Recommended Field Checks

Before locking Phase 3 complete, validate:

1. Actual E32 air data rate and module configuration in hardware.
2. Whether `5 Hz` remains loss-free during simultaneous RX command traffic.
3. Whether AUX busy events appear frequently under worst-case telemetry load.
4. Whether UKS parser cleanly handles skipped sequence numbers.

## If Link Margin Is Poor

Preferred mitigation order:

1. Reduce telemetry field count.
2. Reduce transmit rate below `5 Hz`.
3. Move from verbose ASCII to compact framed binary payloads.
4. Add selective ACK / retry only if field testing proves it necessary.
