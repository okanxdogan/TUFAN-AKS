"""Gorev 1: AKS golden satirlari -> UKS kabul kurallari -> Monitor csv_logger
uc bilesenli sozlesme dogrulayicisi.

Donanimsizdir: hicbir gercek UART/LoRa/seri port kullanilmaz. UKS tarafi
contract.parse_uks_frame() (telemetry.c Decode_Line'in Python eslenigi) ile,
Monitor tarafi ise TUFAN-Monitor'un GERCEK csv_logger fonksiyonlariyla
(import edilerek) test edilir.
"""

from __future__ import annotations

import contract


# ===========================================================================
# 1a) AKS golden satirlari (fixture) — kaynagi yorumla belirtilir.
# ===========================================================================

# Kaynak: ESP_AKS test/test_native_telemetry/test_telemetry_format.cpp
#         ::test_full_format_with_distinct_values (makeDistinctData() +
#         Telemetry::sendStatus format string, birebir beklenen ciktisi).
# Bu, AKS'in KENDI native test suite'inde dogrulanmis, gercek sendStatus()
# ciktisidir — burada tekrar UYDURULMADI.
AKS_GOLDEN_DISTINCT = (
    "TEL,2,0,1500,240,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,12345,1413\r\n"
)

# Kaynak: ESP_AKS test/test_native_telemetry/test_telemetry_format.cpp
#         ::makeZeroData() + test_no_write_before_begin/test_first_packet_*
#         (butun alanlar sifir; sysState=0 -> gercek AKS TelemetryData{}
#         value-init sonucu, sanitize UYGULANMADAN once bu deger uretilir).
# NOT: sysState=0, UKS'in sozlesmesine (1..4) gore GECERSIZDIR — bu satir
# bilerek "sanitize edilmemis ham AKS ciktisi" olarak kullanilir (bkz.
# test_zero_frame_needs_sanitize_before_uks_accepts), sanitizeForUplink
# UYGULANDIKTAN SONRAKI hali AKS_GOLDEN_ZERO_SANITIZED'dir.
AKS_GOLDEN_ZERO_RAW = "TEL,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\r\n"

# Kaynak: ESP_AKS test/test_native_telemetry/test_replay_sanitize_and_seq.cpp
#         ::test_replay_output_sanitizes_corrupted_system_state (sysState=7
#         girildi, sanitizeForUplink sonrasi beklenen tam payload).
AKS_GOLDEN_SANITIZED_FAULT = (
    "TEL,2,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0\r\n"
)

GOLDEN_LINES = [
    AKS_GOLDEN_DISTINCT,
    AKS_GOLDEN_SANITIZED_FAULT,
]


def _line_no_crlf(line: str) -> str:
    return line.rstrip("\r\n")


# ===========================================================================
# 1b) UKS kabul kurallarinin Python eslenigi — golden'lerin KABULU,
#     bilinclii bozuk setin REDDI.
# ===========================================================================


def test_golden_lines_are_accepted_by_uks_rules():
    for line in GOLDEN_LINES:
        result = contract.parse_uks_frame(_line_no_crlf(line))
        assert not isinstance(result, contract.UksRejection), (
            f"golden AKS satiri UKS kurallarinca reddedildi: {line!r} -> "
            f"{result}"
        )
        assert result["ver"] == contract.VER
        assert result["sys_state"] in range(1, 5)


def test_build_tel_line_round_trips_golden_vector():
    """contract.build_tel_line (60 sn kesinti simulasyonunda sentetik paket
    uretmek icin kullanilir) golden AKS fixture'ini birebir uretmeli —
    boylece simulasyonun kullandigi uretici, gercek AKS ciktisiyla
    dogrulanmis olur."""
    parsed = contract.parse_uks_frame(_line_no_crlf(AKS_GOLDEN_DISTINCT))
    assert contract.build_tel_line(parsed) == AKS_GOLDEN_DISTINCT


def test_golden_distinct_line_fields_match_source_test_vector():
    """makeDistinctData() ile birebir alan degerleri (kaynak dogrulugu)."""
    parsed = contract.parse_uks_frame(_line_no_crlf(AKS_GOLDEN_DISTINCT))
    assert parsed == {
        "ver": 2, "seq": 0, "rpm": 1500, "torque": 240, "motor_err": 5,
        "motor_valid": 1, "motor_timeout": 0, "cell_vmax": 37734,
        "cell_vmin": 37422, "temp_h": 32, "temp_l": 31, "sys_state": 2,
        "pack_v": 780, "current": -181610, "soc": 6283, "bms_valid": 1,
        "ts_ms": 12345, "spd_x10": 1413,
    }


def test_zero_frame_needs_sanitize_before_uks_accepts():
    """sysState=0 (sanitize edilmemis ham AKS value-init) UKS tarafindan
    REDDEDILIR — bu yuzden AKS, sendStatus'tan ONCE sanitizeForUplink
    uygulamak ZORUNDADIR (TelemetrySanitize.h S4 sirasi)."""
    raw_result = contract.parse_uks_frame(_line_no_crlf(AKS_GOLDEN_ZERO_RAW))
    assert isinstance(raw_result, contract.UksRejection)
    assert raw_result.reason == "parse_fail"  # sys_state=0, aralik 1..4 disi

    sanitized_result = contract.parse_uks_frame(
        _line_no_crlf(AKS_GOLDEN_SANITIZED_FAULT)
    )
    assert not isinstance(sanitized_result, contract.UksRejection)
    assert sanitized_result["sys_state"] == 4  # sanitizeSystemState(0) == 4


# ===========================================================================
# 1b) Bilincli bozuk set — REDDI dogrulanir.
# ===========================================================================

BROKEN_CASES = {
    "17_field_v1_line": (
        "TEL,1,0,1500,-250,5,1,0,80,-120,32,360,3700,0,1",
        "parse_fail",
    ),
    "bad_version": (
        "TEL,1,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,123456,425",
        "bad_version",
    ),
    "sys_state_out_of_range": (
        "TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,5,780,-181610,6283,1,123456,425",
        "parse_fail",
    ),
    "temp_h_out_of_range": (
        "TEL,2,0,1500,-250,5,1,0,37734,37422,128,31,2,780,-181610,6283,1,123456,425",
        "parse_fail",
    ),
    "spd_x10_out_of_range": (
        "TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,123456,3001",
        "parse_fail",
    ),
    "rpm_over_sanity_max": (
        "TEL,2,0,30000,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,123456,425",
        "range_fail",
    ),
    "ts_ms_overflow": (
        "TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,4294967296,425",
        "parse_fail",
    ),
    "ts_ms_negative": (
        "TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,-1,425",
        "parse_fail",
    ),
    "current_equals_int32_min": (
        "TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-2147483648,6283,1,123456,425",
        "parse_fail",
    ),
    # Torque alani semantik uyumsuzlugu (bkz. Documents/TORQUE_ALAN_KARAR_NOTU.md):
    # AKS bu alana ham (sanitize edilmemis) TEL_motorVoltageDeciV=40000
    # yazarsa, sozlesmenin int16 sinirini (32767) asar ve UKS TUM frame'i
    # reddeder.
    "torque_field_unsanitized_motor_volt_overflow": (
        "TEL,2,0,1500,40000,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,123456,425",
        "parse_fail",
    ),
}


def test_sanitized_motor_volt_torque_field_is_accepted_by_uks_rules():
    """AKS TelemetrySanitize::sanitizeMotorVoltForTorqueField (Python
    eslenigi: contract.sanitize_motor_volt_for_torque_field) 40000 deciV'yi
    32767'ye kirpar; kirpilmis deger UKS Parse_Int(-32768..32767) tarafindan
    KABUL edilmelidir — boylece frame reddi imkansiz hale gelir. Kirpilmamis
    ham deger (40000) ise BROKEN_CASES'teki
    torque_field_unsanitized_motor_volt_overflow ile REDDEDILDIGI dogrulanir."""
    raw_motor_volt = 40000
    sanitized = contract.sanitize_motor_volt_for_torque_field(raw_motor_volt)
    assert sanitized == 32767

    line = (
        "TEL,2,0,1500,%d,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,123456,425"
        % sanitized
    )
    result = contract.parse_uks_frame(line)
    assert not isinstance(result, contract.UksRejection), (
        f"sanitize edilmis torque alani UKS kurallarinca reddedildi: {result}"
    )
    assert result["torque"] == 32767


def test_broken_frames_are_rejected():
    for name, (line, expected_reason) in BROKEN_CASES.items():
        result = contract.parse_uks_frame(line)
        assert isinstance(result, contract.UksRejection), (
            f"{name}: bozuk satir yanlislikla KABUL edildi: {line!r}"
        )
        assert result.reason == expected_reason, (
            f"{name}: beklenen red nedeni {expected_reason!r}, "
            f"gelen {result.reason!r}"
        )


def test_ts_ms_uint32_max_is_accepted_not_a_broken_case():
    """Sarma siniri (UINT32_MAX) bilerek GECERLI — bkz. UKS
    test_ts_ms_uint32_max_accepted (1918430 duzeltmesi)."""
    line = (
        "TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,"
        "4294967295,425"
    )
    result = contract.parse_uks_frame(line)
    assert not isinstance(result, contract.UksRejection)
    assert result["ts_ms"] == contract.U32_MAX


# ===========================================================================
# 1c) Kabul edilen her frame icin UKS forward satiri uret, Monitor'un
#     GERCEK csv_logger fonksiyonlarina besle.
# ===========================================================================


def test_accepted_frames_forward_through_real_monitor_csv_logger(csv_logger_module):
    for line in GOLDEN_LINES:
        parsed = contract.parse_uks_frame(_line_no_crlf(line))
        assert not isinstance(parsed, contract.UksRejection)

        forward_line = contract.build_forward_line(parsed)
        assert forward_line.startswith("CSV,")
        assert forward_line.endswith("\r\n")

        # Monitor'un GERCEK parse_csv_line'i: 5-alan/';'/baslik uyumunu
        # kanitlamak icin monitor.py'nin kendisi degil, csv_logger.py
        # (bagimliliksiz, saf fonksiyon modulu) kullanilir.
        monitor_parsed = csv_logger_module.parse_csv_line(forward_line)
        assert monitor_parsed is not None, (
            f"Monitor gercek parse_csv_line() forward satirini reddetti: "
            f"{forward_line!r}"
        )
        assert monitor_parsed["timestamp_ms"] == parsed["ts_ms"]
        assert monitor_parsed["speed_kmh_x10"] == parsed["spd_x10"]
        assert monitor_parsed["temp_c"] == parsed["temp_h"]
        assert monitor_parsed["pack_voltage_deciv"] == parsed["pack_v"]
        assert monitor_parsed["soc_hundredths"] == parsed["soc"]
        assert monitor_parsed["seq"] == parsed["seq"]

        record = csv_logger_module.format_record(
            monitor_parsed, battery_capacity_wh=1000.0
        )
        # 5-alan / ';' uyumu: gercek yonetmelik formati.
        record_fields = record.split(";")
        assert len(record_fields) == 5, (
            f"Monitor kaydi 5 alanli olmali, {len(record_fields)} geldi: "
            f"{record!r}"
        )

        header_fields = csv_logger_module.HEADER.split(";")
        assert len(header_fields) == 5
        assert csv_logger_module.HEADER == contract.MONITOR_HEADER


def test_broken_frames_never_reach_monitor_forward_stage():
    """Reddedilen frame'ler icin forward satiri hic URETILMEMELI — bu,
    dogrulayicinin kendi akisinda (reddedilen frame -> forward cagirma)
    test edilir; Monitor'a hicbir zaman gecersiz veri gitmeyecegini
    kanitlar."""
    for name, (line, _reason) in BROKEN_CASES.items():
        result = contract.parse_uks_frame(line)
        assert isinstance(result, contract.UksRejection), name
        # contract.build_forward_line yalnizca basarili parse sozlugu
        # bekler; reddedilen frame icin cagrilmasi zaten akis disi kalir.
