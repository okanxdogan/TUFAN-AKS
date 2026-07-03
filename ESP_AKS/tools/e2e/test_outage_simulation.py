"""Gorev 2: 60 sn kesinti provasi (sanal saat, gercek bekleme YOK).

5 Hz canli -> 60 sn kesinti -> 1 Hz offline ornekleme (P6 davranisi,
aks_loop_sim.run_outage_simulation) -> link up -> tik basina
<=REPLAY_BURST_PER_TICK replay + 1 canli TX. Her emitted paket GERCEK UKS
kabul kurallarindan (contract.parse_uks_frame) ve ardindan Monitor'un
GERCEK csv_logger fonksiyonlarindan (monitor.py'nin serial_worker() CSV
isleme yolunun bire bir Python izdüsümü) gecirilir.

Uc kabul asserti (kabul kriterleri listesindeki sirayla):
  1) Kesinti/replay boyunca yeni log dosyasi hic ACILMADI.
  2) Kesinti araligina ait tum ts_ms degerleri dosyada mevcut.
  3) Dosyadaki (sirali, essiz) ts_ms degerleri arasinda hicbir ardisik
     fark 5000 ms'yi asmiyor (9.2.h "en fazla 5 sn" kuralinin, replay
     nedeniyle TX-sirasinda dogal olarak geriye siçrayan ts'ler yerine
     GERCEK ZAMAN CIZELGESI uzerinde denetlenmesi — bkz. asagidaki not).
"""

from __future__ import annotations

import contract
from aks_loop_sim import run_outage_simulation


def _monitor_ingest(forward_lines: list[str], csv_logger_module):
    """monitor.py::serial_worker'in CSV-satiri isleme govdesinin (satir
    ~125-147) GERCEK csv_logger fonksiyonlariyla calisan Python izdüsümü.

    monitor.py'nin kendisi (tkinter/pyserial/matplotlib bagimliligi
    tasidigindan) IMPORT EDILMEZ; yalnizca ayni akisi orkestre eden bu
    yardimci, gercek csv_logger.parse_csv_line / detect_new_boot /
    format_record / HEADER'i cagirir.
    """
    file_open_count = 0
    written_lines: list[str] = []  # HEADER haric, yazilan her "kayit" satiri
    prev_seq = None

    def open_new_file():
        nonlocal file_open_count
        file_open_count += 1
        written_lines.append(csv_logger_module.HEADER)

    open_new_file()  # monitor.py serial_worker basinda tek seferlik open_log_file()

    for line in forward_lines:
        parsed = csv_logger_module.parse_csv_line(line)
        assert parsed is not None, f"Monitor gercek parser reddetti: {line!r}"

        curr_seq = parsed["seq"]
        if csv_logger_module.detect_new_boot(prev_seq, curr_seq):
            open_new_file()  # monitor.py: log_file.close() + open_log_file()
        prev_seq = curr_seq

        record = csv_logger_module.format_record(parsed, battery_capacity_wh=1000.0)
        written_lines.append(record)

    return file_open_count, written_lines


def test_60s_outage_replay_preserves_single_file_and_coverage(csv_logger_module):
    sim = run_outage_simulation()

    assert len(sim.buffered_outage_ts) == 60, (
        "60 sn kesinti x 1 Hz orneklem = 60 paket beklenir (AKS'in kendi "
        "test_60s_outage_simulation_stays_within_capacity testiyle ayni "
        f"beklenti); gelen: {len(sim.buffered_outage_ts)}"
    )
    assert len(sim.buffered_outage_ts) <= contract.OB_CAPACITY, (
        "kesinti orneklemi OB_CAPACITY'yi asmamali (P6 sozlesmesi)"
    )

    # Her emitted paketin GERCEK UKS kabul kurallarindan (contract.
    # parse_uks_frame) gectigini kanitla: TEL satirina donusturup geri
    # parse ediyoruz (round-trip), reddedilen TEK paket olmamali.
    forward_lines = []
    for pkt in sim.packets:
        tel_line = contract.build_tel_line(pkt.fields)
        uks_parsed = contract.parse_uks_frame(tel_line.rstrip("\r\n"))
        assert not isinstance(uks_parsed, contract.UksRejection), (
            f"simule edilen {pkt.kind} paketi UKS tarafindan reddedildi "
            f"(tick={pkt.tick_now_ms}): {uks_parsed}"
        )
        forward_lines.append(contract.build_forward_line(uks_parsed))

    # En az bir replay + bir canli paket gonderildigini dogrula (senaryonun
    # gercekten kesinti+replay'i egzersiz ettigini kanitlamak icin).
    kinds = {p.kind for p in sim.packets}
    assert kinds == {"replay", "live"}
    replay_count = sum(1 for p in sim.packets if p.kind == "replay")
    assert replay_count == len(sim.buffered_outage_ts), (
        "buffer'a giren her paket sonunda replay edilmis olmali (kayip yok)"
    )

    file_open_count, written_lines = _monitor_ingest(forward_lines, csv_logger_module)

    # ---- Assert 1: yeni dosya ACILMADI ----
    assert file_open_count == 1, (
        f"Monitor {file_open_count} kez dosya acti; kesinti/replay tek "
        "dosyada devam etmeliydi (detect_new_boot yanlislikla True donmus "
        "olabilir — seq replay sirasinda ARDISIK artmali)."
    )

    # written_lines[0] HEADER; geri kalanlar ';'-ayrikli kayitlar.
    header = written_lines[0]
    records = written_lines[1:]
    assert header == contract.MONITOR_HEADER

    recorded_ts = [int(r.split(";")[0]) for r in records]

    # ---- Assert 2: kesinti araligindaki ts'ler dosyada mevcut ----
    recorded_ts_set = set(recorded_ts)
    missing = [t for t in sim.buffered_outage_ts if t not in recorded_ts_set]
    assert not missing, (
        f"kesinti araligindan {len(missing)} ts_ms degeri dosyada yok: "
        f"{missing[:10]}..."
    )

    # ---- Assert 3: sirali/essiz ts_ms'ler arasinda 5000 ms'yi asan bosluk yok ----
    # NOT: written_lines/records TX SIRASINDADIR (replay eski ts'leri, canli
    # guncel ts'yi tasir — bu ikisi TX aninda kasitli olarak iç ice gecer,
    # bkz. UKS Monitor csv_logger.detect_new_boot dokumantasyonu: "ts geriye
    # gitmesi replay'in normal bir parçasıdır"). Bu yuzden "ardisik ts farki"
    # TX-sira listesinde degil, dosyanin KAPSADIGI GERCEK ZAMAN CIZELGESINDE
    # (sirali/essiz ts_ms kumesi) denetlenir — 9.2.h'nin olcmek istedigi
    # "kayit araliklari arasi en fazla 5 sn" kapsam-bosluk kuralinin dogru
    # yorumu budur (offline 1 Hz orneklem + canli TX'in birlikte, telemetri
    # ZAMAN CIZGISINDE hicbir >5sn kor nokta birakmamasi).
    sorted_unique_ts = sorted(recorded_ts_set)
    gaps = [
        b - a for a, b in zip(sorted_unique_ts, sorted_unique_ts[1:])
    ]
    assert all(g <= 5000 for g in gaps), (
        f"zaman cizelgesinde 5000 ms'yi asan bosluk(lar) var: "
        f"{[g for g in gaps if g > 5000]}"
    )
