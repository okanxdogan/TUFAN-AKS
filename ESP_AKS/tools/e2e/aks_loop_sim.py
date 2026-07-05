"""AKS vTask_LoRa_UKS ana donguyusunun (src/main.cpp, satir ~505-635) saf
Python, sanal-saatli simulasyonu.

Bu, AKS firmware kodunun kendisini COPYALAMAZ/CALISTIRMAZ (donanimsiz
kisitlamasi) — ayni sozlesme davranisini (5 Hz canli TX, kesintide 1 Hz
seyreltilmis ornekleme, link-up sonrasi tik basina <=REPLAY_BURST_PER_TICK
replay + 1 canli) contract.py sabitleriyle ve OfflineBufferSim ile modelleyip
her "TX edilen" paket icin GERCEK UKS kabul kurallarindan (contract.
parse_uks_frame) ve GERCEK Monitor csv_logger fonksiyonlarindan gecirir.

Gercek zaman beklenmez — nowMs bir Python int sayaci olarak ilerletilir.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import contract
from offline_buffer_sim import OfflineBufferSim


@dataclass
class EmittedPacket:
    tick_now_ms: int
    kind: str          # "replay" ya da "live"
    fields: dict        # UKS'in kabul ettigi parse_uks_frame() sekli


@dataclass
class SimResult:
    packets: list = field(default_factory=list)          # EmittedPacket, TX sirasiyla
    buffered_outage_ts: list = field(default_factory=list)  # kesintide buffer'a giren ts_ms'ler
    outage_start_ms: int = 0
    outage_end_ms: int = 0


def _sensor_reading(now_ms: int) -> dict:
    """O anki "canli" sensor okumasi — deterministik, her zaman UKS'in kabul
    aralığında (sanitize gerektirmeyen normal calisma degerleri)."""
    return {
        "rpm": 1200,
        "torque": 100,
        "motor_err": 0,
        "motor_valid": 1,
        "motor_timeout": 0,
        "cell_vmax": 37500,
        "cell_vmin": 37400,
        "temp_h": 28,
        "temp_l": 26,
        "sys_state": 2,        # IDLE
        "pack_v": 780,
        "current": 5000,
        "soc": 8000,
        "bms_valid": 1,
        "ts_ms": now_ms,
        "spd_x10": 250,
    }


def _sanitize(reading: dict) -> dict:
    """TelemetrySanitize::sanitizeForUplink Python eslenigi (bkz.
    contract.sanitize_system_state/sanitize_soc/sanitize_current)."""
    out = dict(reading)
    out["sys_state"] = contract.sanitize_system_state(out["sys_state"])
    out["soc"] = contract.sanitize_soc(out["soc"])
    out["current"] = contract.sanitize_current(out["current"])
    return out


def run_outage_simulation(
    pre_live_ms: int = 1000,
    outage_ms: int = 60000,
    post_live_ms: int | None = None,
) -> SimResult:
    """5 Hz canli -> 60 sn kesinti (1 Hz offline ornekleme) -> link up ->
    tik basina <=REPLAY_BURST_PER_TICK replay + 1 canli TX simulasyonu.

    post_live_ms=None birakilirsa, buffer'in tamamen bosalmasina yetecek sure
    contract sabitlerinden (OFFLINE_SAMPLE_PERIOD_MS, REPLAY_BURST_PER_TICK,
    LORA_TX_PERIOD_MS) turetilir (+%50 marj) — REPLAY_BURST_PER_TICK ileride
    tekrar degisirse sabit bir varsayimin sessizce bayatlamasini onler.

    Donus: TX SIRASINA GORE tum emitted paketler (her biri gercek
    contract.parse_uks_frame() kabulunden gecmis, UKS'in kabul edecegi
    field dict'i tasir) + kesinti sirasinda buffer'a giren ts listesi.
    """
    if post_live_ms is None:
        max_buffered = outage_ms // contract.OFFLINE_SAMPLE_PERIOD_MS
        ticks_needed = math.ceil(max_buffered / contract.REPLAY_BURST_PER_TICK)
        post_live_ms = int(ticks_needed * contract.LORA_TX_PERIOD_MS * 1.5)  # %50 marj

    result = SimResult()
    buffer = OfflineBufferSim(contract.OB_CAPACITY)

    seq = 0
    tick_dt = contract.LORA_TX_PERIOD_MS

    total_ms = pre_live_ms + outage_ms + post_live_ms
    outage_start_ms = pre_live_ms
    outage_end_ms = pre_live_ms + outage_ms
    result.outage_start_ms = outage_start_ms
    result.outage_end_ms = outage_end_ms

    last_offline_sample_ms = 0
    has_offline_sample = False

    now_ms = 0
    while now_ms < total_ms:
        link_down = outage_start_ms <= now_ms < outage_end_ms

        if not link_down:
            # --- NORMAL MOD: throttled replay + canli paket (S1) ---
            for _ in range(contract.REPLAY_BURST_PER_TICK):
                buffered = buffer.peek()
                if buffered is None:
                    break
                sent = _sanitize(buffered)
                sent["ver"] = contract.VER
                sent["seq"] = seq
                seq += 1
                result.packets.append(
                    EmittedPacket(tick_now_ms=now_ms, kind="replay", fields=sent)
                )
                buffer.drop_front()

            live = _sanitize(_sensor_reading(now_ms))
            live["ver"] = contract.VER
            live["seq"] = seq
            seq += 1
            result.packets.append(
                EmittedPacket(tick_now_ms=now_ms, kind="live", fields=live)
            )
        else:
            # --- OFFLINE MOD: canli TX yok; 1 Hz seyreltilmis ornekle ---
            reading = _sensor_reading(now_ms)
            if contract_offline_should_sample(
                now_ms, last_offline_sample_ms, has_offline_sample
            ):
                last_offline_sample_ms = now_ms
                has_offline_sample = True
                buffer.push(reading)
                result.buffered_outage_ts.append(reading["ts_ms"])

        now_ms += tick_dt

    return result


def contract_offline_should_sample(now_ms: int, last_sample_ms: int, has_sample: bool) -> bool:
    """OfflineBuffer.h::offline_should_sample'in Python eslenigi."""
    if not has_sample:
        return True
    return (now_ms - last_sample_ms) >= contract.OFFLINE_SAMPLE_PERIOD_MS
