# LoRa Link Analysis

This note captures the current AKS-side telemetry bandwidth estimate and the practical assumptions for Phase 3 reliability work.

> **Merge note (2026-07-03):** an earlier revision on `main` (commit
> `594a93e`) briefly replaced the payload described below with a
> semicolon-separated `zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh`
> format. That format is the **TUFAN-Monitor CSV log line**, not the
> AKS→UKS LoRa wire format — UKS's `telemetry.c::Decode_Line` parser
> requires the 19-field `TEL,...` frame described here (see
> `tools/e2e/contract.py`). This doc has been corrected back to describe
> the actual wire format; the merge kept the tested `TEL,...` encoder.

## Current AKS Telemetry Payload

Current uplink format (AKS→UKS, v2):

- ASCII CSV line
- Prefix: `TEL,<version>,<sequence>,...` (19 comma-separated fields total, `TEL` tag included)
- Terminator: `\r\n`
- Rate: `5 Hz` (`LORA_TX_PERIOD_MS = 200`)

Representative packet:

```text
TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,12345,1413\r\n
```

Typical payload size is approximately `50-80 bytes` depending on numeric field widths.
A conservative planning budget of `90 bytes` per packet gives:

- `90 bytes * 5 Hz = 450 bytes/s`
- `450 bytes/s * 10 bits/byte ~= 4500 bit/s` on the UART side including start/stop overhead

## Interpretation

Important distinction:

- The ESP32 <-> E22-400T30D-V2 UART link is configured for `9600 baud`.
- The radio air data rate of the E22 module (currently `9.6 kbps`, see `E22Regs.h` REG0) may be lower than the UART baud.
- Therefore, final field testing must confirm that the selected E22 radio configuration can drain the UART input fast enough at the chosen packet rate.

## Current Reliability Policy

Implemented now:

- RF link is content-wise one-way: AKS→UKS carries telemetry, UKS→AKS carries only the `0xB0` heartbeat byte (no commands — see `LoraRxHandler.h`, 9.2.a).
- Link-down detection via heartbeat timeout (`LINK_TIMEOUT_MS = 3000`) with a boot-grace window (`BOOT_LINK_GRACE_MS = 5000`) so a UKS that's silent from power-on doesn't look falsely "up".
- OfflineBuffer ring buffer (`OB_CAPACITY = 75`) retains telemetry during link loss, sampled at 1 Hz (`OFFLINE_SAMPLE_PERIOD_MS = 1000`) while offline.
- On reconnect: up to `REPLAY_BURST_PER_TICK = 3` buffered packets are replayed per TX tick, plus 1 live packet, until the buffer drains.
- All replayed and live packets pass through `TelemetrySanitize::sanitizeForUplink` immediately before `sendStatus` (S4) so UKS's accept ranges are never violated.
- No AKS retransmission / no AKS-level ACK handling.
- AUX gate checked before each TX attempt; if busy, TX for that tick is skipped (packet stays queued, not dropped).
- Sequence counter (`TEL_sequenceCounter`) increments only on actual TX (live or replay) — TUFAN-Monitor's `detect_new_boot` relies on this being strictly monotonic.

Implication:

- Packet loss is observable at UKS by sequence gaps.
- Lost packets (beyond `OB_CAPACITY`) are not resent — oldest buffered packet is dropped first.
- If AUX is busy, AKS retries the same sample next period.

## Recommended Field Checks

Before locking Phase 3 complete, validate:

1. Actual E22 air data rate and module configuration in hardware (bench dump vs `E22Regs.h`, see `TEKNIK_KONTROL_PROVASI.md` §4 / `BENCH_E22_TEYIT.md`, P10).
2. Whether `5 Hz` remains loss-free in practice.
3. Whether AUX busy events appear frequently under worst-case telemetry load.
4. Whether UKS parser cleanly handles skipped sequence numbers.

## If Link Margin Is Poor

Preferred mitigation order:

1. Reduce telemetry field count.
2. Reduce transmit rate below `5 Hz`.
3. Move from verbose ASCII to compact framed binary payloads.
4. Add selective ACK / retry only if field testing proves it necessary.
