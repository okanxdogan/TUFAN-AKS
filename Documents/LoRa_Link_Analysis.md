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

> **Link flapping fix (2026-07-07):** field logs showed the AKS↔UKS link
> constantly cycling DOWN→UP (`LINK: UKS heartbeat timeout` every ~5-6 s).
> Root cause: on the single-frequency half-duplex E22 channel, AKS's
> continuous 5 Hz TX left almost no gap for UKS's 1 Hz `0xB0` heartbeat to
> get through, so it only arrived every ~5-6 s — longer than the old
> `LINK_TIMEOUT_MS = 3000`. Fix: `LORA_TX_PERIOD_MS` lowered `200 -> 500`
> (5 Hz → 2 Hz, opening up channel time for the heartbeat) and
> `LINK_TIMEOUT_MS` raised `3000 -> 9000` (margin over the observed ~5-6 s
> heartbeat interval). The bandwidth figures below are updated for the new
> 2 Hz rate.

> **Air rate revision (2026-07-17):** field range testing found the link
> unreliable at `1.5 km`, well beyond the actual required range (max `500 m`
> for this course). Air rate was first dropped `9.6 -> 2.4 kbps` (with TX
> rate temporarily at `1 Hz`); that intermediate 2.4 kbps/1 Hz configuration
> was never deployed to the field — the same day it was revised again to
> `2.4 -> 4.8 kbps` (`REG0 = 0x63`, bit[2:0] = `011`) with TX rate restored
> to `2 Hz` (`LORA_TX_PERIOD_MS = 500`), trading some of the extra range
> margin back for sensitivity/precision headroom since 500 m does not need
> the full range 2.4 kbps would have bought. UKS's `TEL_LINK_TIMEOUT_MS`
> stayed at `2000 ms` throughout. AKS commits: `9230936` (9.6->2.4 kbps),
> `c083139` (2.4->4.8 kbps); synchronized with UKS the same day.

## Current AKS Telemetry Payload

Current uplink format (AKS→UKS, v2):

- ASCII CSV line
- Prefix: `TEL,<version>,<sequence>,...` (19 comma-separated fields total, `TEL` tag included)
- Terminator: `\r\n`
- Rate: `2 Hz` (`LORA_TX_PERIOD_MS = 500`)

Representative packet:

```text
TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,12345,1413\r\n
```

Typical payload size is approximately `50-80 bytes` depending on numeric field widths.
A conservative planning budget of `90 bytes` per packet gives:

- `90 bytes * 2 Hz = 180 bytes/s`
- `180 bytes/s * 10 bits/byte ~= 1800 bit/s` on the UART side including start/stop overhead

On the air side, at the `4.8 kbps` E22 air data rate (`REG0 = 0x63`, bit[2:0] = `011`):

- `90 bytes * 10 bits/byte / 4800 bit/s ~= 190 ms` airtime per packet
- Live-only occupancy: `190 ms / 500 ms tick ~= 38%`
- During offline-buffer drain (live + 1 replay frame back-to-back): `~380 ms / 500 ms tick ~= 76%` peak occupancy

## Interpretation

Important distinction:

- The ESP32 <-> E22-400T30D-V2 UART link is configured for `9600 baud`.
- The radio air data rate of the E22 module (currently `4.8 kbps`, `REG0 = 0x63` bit[2:0] = `011`, see `E22Regs.h`) is lower than the UART baud.
- Therefore, final field testing must confirm that the selected E22 radio configuration can drain the UART input fast enough at the chosen packet rate.

## Current Reliability Policy

Implemented now:

- RF link is content-wise one-way: AKS→UKS carries telemetry, UKS→AKS carries only the `0xB0` heartbeat byte (no commands — see `LoraRxHandler.h`, 9.2.a).
- Link-down detection via heartbeat timeout (`LINK_TIMEOUT_MS = 9000`) with a boot-grace window (`BOOT_LINK_GRACE_MS = 5000`) so a UKS that's silent from power-on doesn't look falsely "up".
- OfflineBuffer ring buffer (`OB_CAPACITY = 600`) retains telemetry during link loss, sampled at 1 Hz (`OFFLINE_SAMPLE_PERIOD_MS = 1000`) while offline — 600 records @ 1 Hz gives ~10 minutes of coverage (record size 88 bytes, ~52,800 bytes static buffer).
- On reconnect: up to `REPLAY_BURST_PER_TICK = 1` buffered packets are replayed per TX tick, plus 1 live packet, until the buffer drains.
- **G11-b (2026-07-13) — LoRa UART init self-healing:** if `EspLoraHal::begin()`
  fails after its bounded `LORA_UART_MAX_INIT_ATTEMPTS` (G11), `vTask_LoRa_UKS`
  no longer parks in a permanent empty loop. It retries `begin()` (+
  `configureE22()`) every `LORA_INIT_RETRY_INTERVAL_MS` (30 s, see
  `lora_task_retry_due()` in `lib/LoraLink/UartInitRetry.h`, natively tested
  in `test/test_native_uart_init_retry`) until it succeeds. The watchdog is
  fed throughout the wait (both inside `EspLoraHal::begin()`'s own retry loop
  and the outer 30 s wait). `LoRa_IsTelemetryDisabled()` reports `true` for
  the whole disabled window and flips back to `false` on recovery — the
  vehicle is never affected either way (telemetry loss never triggers FAULT).
  On successful recovery, the `UplinkScheduler` (link FSM, offline buffer,
  replay) and the boot-grace timestamp are constructed **fresh, at the
  recovery moment** — they were already positioned after the init step in
  the task body, so no separate reset logic was needed; this means UKS is
  not falsely assumed "UP" using a stale boot time from before the outage.
  `LoRa_IsLinkDown()`'s cross-task pointer (`s_uplink`) is null-checked before
  dereferencing, so other tasks querying link state during the (re)init
  window get a safe default (`false`, i.e. "not down") rather than crashing.

## Replay-Mode Budget

When the link reconnects (link UP), the offline buffer begins to drain while the live stream continues. To prevent buffer overrun at the 9600 baud UART limit (`~480 bytes per 500 ms tick`), the replay burst must be strictly limited.

- Live stream: `1 frame/tick` (`~90 bytes`)
- Replay stream: `REPLAY_BURST_PER_TICK = 1` (`~90 bytes`)
- Total TX load: `~180 bytes / tick`

This keeps the TX load well under the `480 bytes / tick` budget (previously `~192 bytes / 200 ms tick` before the tick period was lengthened to 500 ms as part of the link-flapping fix — the budget only got roomier, so this does not reopen the issue described below). Previous configurations using a burst of 3 generated `~360 bytes / tick` at the old 200 ms tick, which caused the 256-byte TX buffer to fill, blocking the UART write. This blocking starved the LoRa RX heartbeat handler, causing phantom link timeouts (re-triggering a link DOWN state). With `REPLAY_BURST_PER_TICK = 1`, the task loop remains unblocked and RX processing (which is evaluated before TX) operates securely.
- All replayed and live packets pass through `TelemetrySanitize::sanitizeForUplink` immediately before `sendStatus` (S4) so UKS's accept ranges are never violated.
- No AKS retransmission / no AKS-level ACK handling.
- AUX gate checked before each TX attempt; if busy, TX for that tick is skipped (packet stays queued, not dropped).
- Sequence counter (`TEL_sequenceCounter`) increments only on actual TX (live or replay) — TUFAN-Monitor's `detect_new_boot` relies on this being strictly monotonic.

Implication:

- Packet loss is observable at UKS by sequence gaps.
- Lost packets (beyond `OB_CAPACITY`) are not resent — oldest buffered packet is dropped first.
- If AUX is busy, AKS retries the same sample next period.

## UKS-side TEL Timeout Margin

This section analyzes the **reverse-direction risk**: could the same kind of
half-duplex channel congestion that previously delayed the UKS→AKS heartbeat
(see "Link flapping fix" note above) also delay AKS→UKS `TEL` frames enough
to trip UKS's own link-down watchdog?

**UKS-side constant:** `TEL_LINK_TIMEOUT_MS = 2000` (`UKS-Telemetry/Core/Inc/telemetry.h`).
If no valid `TEL` frame arrives for more than this duration, UKS declares
`LINK,DOWN` (symmetric to AKS's heartbeat-based detection, see `UYUM_NOTU.md`
bölüm 2). Nominal `TEL` cadence is `LORA_TX_PERIOD_MS = 500` ms, so the
nominal margin is `2000 / 500 = 4x` — the header comment there calls this out
explicitly ("4x marj birakiyor").

### Can a TEL frame actually be skipped?

Per-tick TX (`UplinkScheduler::onTxTickLinkUp`, `src/main.cpp::LoRa_txSend`)
attempts up to `REPLAY_BURST_PER_TICK=1` replay frame **then** 1 live frame,
each gated by `isAuxReady()`. If AUX is busy, `LoRa_txSend` returns `false`
and that attempt is skipped for the tick — **not queued for later within the
same tick**, it simply waits for the next 500 ms tick. Since the replay and
live checks happen back-to-back (microseconds apart), an AUX-busy tick
effectively skips **both** attempts for that tick, not just one. A skipped
live packet is not buffered as offline data since the link is still UP; the
next tick reads a fresh live sample. (The prior UART-ring-blocking hazard —
`uart_write_bytes` blocking and stalling the whole task loop, including RX/
heartbeat processing — was already closed by the G10 fix: `LoRa_txSend` now
checks `uart_get_tx_buffer_free_size` and **defers** (non-blocking) instead
of blocking when the ring lacks room for a full frame.)

### Worst-case gap vs. TEL_LINK_TIMEOUT_MS

| Consecutive fully-skipped ticks | Gap seen at UKS | vs. 2000 ms | Result |
|---|---|---|---|
| 0 | 500 ms | — | LINK stays UP |
| 1 | 1000 ms | < 2000 | LINK stays UP |
| 2 | 1500 ms | < 2000 | LINK stays UP |
| 3 | 2000 ms | == 2000 (strict `>` check in `main.c`) | LINK stays UP (boundary, not triggered) |
| 4 | 2500 ms | > 2000 | **False `LINK,DOWN`** |

So the current configuration tolerates **3 consecutive fully-skipped TX
ticks** (1.5 s of AUX-busy or deferred TX) before UKS would falsely declare
the link down; a 4th consecutive miss is required to trip it.

### Why 3-in-a-row is considered low-probability today (not field-proven)

- **AUX-busy duration is air-time-bounded, not tick-period-bounded.** At the
  configured `4.8 kbps` air rate, one `LORA_TEL_FRAME_MAX_BYTES=120` frame
  takes ≈200 ms on air; the busiest tick (live+replay during a replay drain)
  sends at most 2 frames ≈400 ms. That leaves ≈100 ms of slack inside every
  500 ms tick in the busiest realistic case — tighter than under the retired
  `9.6 kbps` setting (which left ≈300 ms), but a single tick's own AUX
  activity should still clear before the *next* tick fires.
- **Replay mode does not change the TX cadence.** It adds a second frame to
  the *same* 500 ms tick (still one grid, see "Replay-Mode Budget" above),
  it does not insert extra ticks. The combined peak throughput
  (`480 B/s ≤ 768 B/s` budget, `SystemConfig.h` `static_assert`) is
  well inside the UART's capacity, so replay draining is not, by itself, a
  mechanism for stacking multiple consecutive skips.
- **The original heartbeat-flapping mechanism doesn't have a direct mirror
  here.** That issue was UKS's *own* module being squeezed for airtime by
  AKS's then-continuous 5 Hz TX. AKS's TEL cadence, by contrast, is gated by
  **AKS's own local `AUX` pin** (its own module's busy/ready state), not by
  whether UKS happens to be transmitting. UKS only sends a single heartbeat
  byte at ~1 Hz (≈1 ms air time) — a brief, infrequent RX event on the AKS
  side — so it is not expected to compound into multi-tick AUX-busy stretches
  on the AKS→UKS direction.

**Residual risk:** this is a static/link-budget argument, not a field
measurement. "Recommended Field Checks" item 3 below (AUX-busy frequency)
should specifically also track whether **2+ consecutive** TX ticks are ever
skipped in practice; if UKS logs a `LINK,DOWN` while AKS's own tick log shows
no >2000 ms gap in its TX attempts, that indicates genuine RF/channel loss
rather than a false positive from local scheduling — worth distinguishing
before assuming the margin is real in the field.

**Constants intentionally left unchanged** (field-calibrated): `LORA_TX_PERIOD_MS=500`,
`TEL_LINK_TIMEOUT_MS=2000`, `LINK_TIMEOUT_MS=9000`. This section only adds
analysis + a drift-guard invariant (`tools/e2e/test_contract_drift.py`:
`TEL_LINK_TIMEOUT_MS ≥ 3 × LORA_TX_PERIOD_MS`) so that if someone later
raises `LORA_TX_PERIOD_MS` unilaterally without revisiting this margin, the
e2e suite fails instead of silently eroding the 3-tick tolerance computed
above.

## Recommended Field Checks

Before locking Phase 3 complete, validate:

1. ~~Actual E22 air data rate and module configuration in hardware (bench dump vs `E22Regs.h`)~~ — **DONE 2026-07-15**: bench dump matched `E22Regs.h`/`e22_regs.h` targets exactly, see `TEKNIK_KONTROL_PROVASI.md` §4 / `BENCH_E22_TEYIT.md` "Sonuç Kaydı" (P10).
2. Whether `2 Hz` remains loss-free in practice.
3. Whether AUX busy events appear frequently under worst-case telemetry load.
4. Whether UKS parser cleanly handles skipped sequence numbers.

## If Link Margin Is Poor

Preferred mitigation order:

1. Reduce telemetry field count.
2. Reduce transmit rate below `2 Hz`.
3. Move from verbose ASCII to compact framed binary payloads.
4. Add selective ACK / retry only if field testing proves it necessary.
