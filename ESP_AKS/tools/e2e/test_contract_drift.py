"""Gorev 3: Contract-drift bekcileri.

Bu dosyadaki testler yalnizca OKUR — hicbir firmware/Monitor kaynagini
DEGISTIRMEZ. Amac: sozlesme sabitlerinden biri (TEL_FIELD_COUNT, E22
register hedefleri, one-directionality vb.) gelecekte kaza ile
degistirilirse CI'da ANINDA bir FAIL uretmek.
"""

from __future__ import annotations

import re

import pytest

import contract
from drift_helpers import (
    count_active_code_hits,
    extract_define_int,
    extract_define_raw,
    iter_files,
    read,
    strip_c_comments,
    strip_py_comments,
)


# ===========================================================================
# 3.1 — UKS telemetry.h / telemetry.c
# ===========================================================================


def test_uks_tel_field_count_and_version(uks_telemetry_dir):
    header = read(uks_telemetry_dir / "Core/Inc/telemetry.h")
    assert extract_define_int(header, "TEL_FIELD_COUNT", source="UKS telemetry.h") == contract.TOKEN_COUNT
    assert extract_define_int(header, "TEL_PROTOCOL_VERSION", source="UKS telemetry.h") == contract.VER


def test_uks_telemetry_c_has_parse_u32(uks_telemetry_dir):
    src = read(uks_telemetry_dir / "Core/Src/telemetry.c")
    assert re.search(r"\bstatic\s+int\s+Parse_U32\s*\(", src), (
        "UKS telemetry.c icinde Parse_U32 bulunamadi — ts_ms/seq'in uint32 "
        "(sarma-guvenli) parse edilmesi 1918430 duzeltmesiyle Parse_U32'ye "
        "bagliydi; bu fonksiyon kaldirilir/yeniden adlandirilirsa 24.8 gunluk "
        "sarma hatasi geri gelir."
    )
    # ts_ms (f[17]) ve seq (f[2]) fiilen Parse_U32 ile parse ediliyor mu?
    assert re.search(r"Parse_U32\(f\[2\]", src), "seq (f[2]) Parse_U32 ile parse edilmiyor"
    assert re.search(r"Parse_U32\(f\[17\]", src), "ts_ms (f[17]) Parse_U32 ile parse edilmiyor"


# ===========================================================================
# 3.2 — ESP_AKS SystemConfig.h / TelemetrySanitize.h / Telemetry.h
# ===========================================================================


def test_aks_lora_protocol_version(aks_root):
    text = read(aks_root / "include/SystemConfig.h")
    assert extract_define_int(text, "LORA_PROTOCOL_VERSION", source="AKS SystemConfig.h") == contract.VER


def test_aks_tel_spd_x10_max(aks_root):
    text = read(aks_root / "lib/Telemetry/Telemetry.h")
    assert extract_define_int(text, "TEL_SPD_X10_MAX", source="AKS Telemetry.h") == contract.TEL_SPD_X10_MAX


def test_aks_telemetry_sanitize_exists_with_system_state_rule(aks_root):
    path = aks_root / "lib/Telemetry/TelemetrySanitize.h"
    assert path.is_file(), "AKS TelemetrySanitize.h eksik"
    text = read(path)
    assert "sanitizeSystemState" in text
    assert "sanitizeSoc" in text
    assert "sanitizeCurrent" in text
    # sysState disi -> 4 (FAULT) kurali kaynakta hala mevcut mu?
    m = re.search(
        r"sanitizeSystemState[^{]*\{[^}]*\}", strip_c_comments(text), re.DOTALL
    )
    assert m, "sanitizeSystemState govdesi bulunamadi"
    assert re.search(r">=\s*1U?\s*&&.*<=\s*4U?", m.group(0)) or re.search(
        r"1U?\s*<=.*<=\s*4U?", m.group(0)
    ), "sanitizeSystemState govdesinde 1..4 aralik kontrolu bulunamadi"
    assert re.search(r":\s*4U?\s*;", m.group(0)), (
        "sanitizeSystemState govdesinde FAULT(4) donus degeri bulunamadi"
    )


def test_aks_p6_offline_buffer_constants(aks_root):
    sys_config = read(aks_root / "include/SystemConfig.h")
    assert extract_define_int(sys_config, "OFFLINE_SAMPLE_PERIOD_MS", source="AKS SystemConfig.h") == contract.OFFLINE_SAMPLE_PERIOD_MS
    assert extract_define_int(sys_config, "REPLAY_BURST_PER_TICK", source="AKS SystemConfig.h") == contract.REPLAY_BURST_PER_TICK
    assert extract_define_int(sys_config, "BOOT_LINK_GRACE_MS", source="AKS SystemConfig.h") == contract.BOOT_LINK_GRACE_MS

    ob_header = read(aks_root / "lib/OfflineBuffer/OfflineBuffer.h")
    assert extract_define_int(ob_header, "OB_CAPACITY", source="AKS OfflineBuffer.h") == contract.OB_CAPACITY


# ===========================================================================
# 3.3 — E22 register sozlesmesi (UKS e22_regs.h <-> AKS E22Regs.h)
# ===========================================================================

_E22_FIELDS = ["ADDH", "ADDL", "NETID", "REG0", "REG1", "REG2", "REG3", "CRYPT_H", "CRYPT_L"]


def test_e22_register_targets_match_across_repos(uks_telemetry_dir, aks_root):
    uks_text = read(uks_telemetry_dir / "Core/Inc/e22_regs.h")
    aks_text = read(aks_root / "include/E22Regs.h")

    mismatches = []
    for field in _E22_FIELDS:
        uks_val = extract_define_int(uks_text, f"E22_VAL_{field}", source="UKS e22_regs.h")
        aks_val = extract_define_int(aks_text, f"E22_CFG_{field}", source="AKS E22Regs.h")
        if uks_val != aks_val:
            mismatches.append((field, uks_val, aks_val))

    assert not mismatches, (
        "E22 register hedef degerleri UKS/AKS arasinda UYUSMUYOR (cift "
        f"commit senkronizasyonu bozulmus olabilir): {mismatches}"
    )

    # Sozlesme sabitleriyle (contract.py) de birebir tutarli olmali.
    expected = {
        "ADDH": contract.E22_ADDH, "ADDL": contract.E22_ADDL,
        "NETID": contract.E22_NETID, "REG0": contract.E22_REG0,
        "REG1": contract.E22_REG1, "REG2": contract.E22_REG2,
        "REG3": contract.E22_REG3, "CRYPT_H": contract.E22_CRYPT_H,
        "CRYPT_L": contract.E22_CRYPT_L,
    }
    for field in _E22_FIELDS:
        uks_val = extract_define_int(uks_text, f"E22_VAL_{field}", source="UKS e22_regs.h")
        assert uks_val == expected[field], (
            f"contract.py E22_{field} degeri kaynaktan sapmis: "
            f"contract={expected[field]} kaynak={uks_val}"
        )


def test_e22_config_mode_pin_levels_match(uks_telemetry_dir, aks_root):
    uks_text = read(uks_telemetry_dir / "Core/Inc/e22_regs.h")
    aks_text = read(aks_root / "include/SystemConfig.h")

    uks_m0 = extract_define_int(uks_text, "E22_MODE_CONFIG_M0", source="UKS e22_regs.h")
    uks_m1 = extract_define_int(uks_text, "E22_MODE_CONFIG_M1", source="UKS e22_regs.h")
    aks_m0 = extract_define_int(aks_text, "LORA_MODE_CONFIG_M0_LEVEL", source="AKS SystemConfig.h")
    aks_m1 = extract_define_int(aks_text, "LORA_MODE_CONFIG_M1_LEVEL", source="AKS SystemConfig.h")

    assert (uks_m0, uks_m1) == (aks_m0, aks_m1) == (
        contract.E22_MODE_CONFIG_M0, contract.E22_MODE_CONFIG_M1
    )


def test_e22_dump_format_string_matches_across_repos(uks_telemetry_dir, aks_root):
    uks_lora_c = read(uks_telemetry_dir / "Core/Src/lora.c")
    aks_main_cpp = read(aks_root / "src/main.cpp")

    needle = contract.E22_DUMP_LINE_FORMAT
    assert needle in uks_lora_c, f"UKS lora.c icinde dump formati bulunamadi: {needle!r}"
    assert needle in aks_main_cpp, f"AKS main.cpp icinde dump formati bulunamadi: {needle!r}"


def test_e32_leftover_constants_are_gone(uks_telemetry_dir, aks_root):
    """E32->E22 gecisinden (1803e1f) sonra hicbir E32_CFG_* sabiti kalmamali."""
    uks_files = iter_files(uks_telemetry_dir, ["Core", "test"], (".c", ".h"))
    aks_files = iter_files(aks_root, ["include", "lib", "src", "test"], (".c", ".h", ".cpp"))

    pattern = re.compile(r"\bE32_CFG_\w+")
    for label, files in (("UKS", uks_files), ("AKS", aks_files)):
        hits = count_active_code_hits(files, pattern)
        assert not hits, f"{label} reposunda E32_CFG_* leftover bulundu: {hits}"


# ===========================================================================
# 3.4 — Tek yonluluk (9.2.a) bekcisi
# ===========================================================================


def test_no_removed_command_bytes_in_active_production_code(uks_telemetry_dir, aks_root):
    """0xA1-0xA4 (eski komut byte'lari) PRODUCTION kaynaklarda (yorumlar
    HARIC) SIFIR gecmeli. Test dosyalari kasitli olarak KAPSAM DISI
    birakilir — cunku AKS'in kendi test/test_native_lora_rx_handler
    testi bu byte'lari, TAM DA UNKNOWN olarak siniflandigini KANITLAMAK
    icin literal kullanir (yani varligi kaldirilmayi degil, kaldirilmis
    OLDUGUNU dogrular)."""
    pattern = re.compile(r"\b0[xX][Aa][1-4]\b")

    uks_files = iter_files(uks_telemetry_dir, ["Core"], (".c", ".h"))
    aks_files = iter_files(aks_root, ["include", "lib", "src"], (".c", ".h", ".cpp"))

    for label, files in (("UKS", uks_files), ("AKS", aks_files)):
        hits = count_active_code_hits(files, pattern)
        assert not hits, (
            f"{label} PRODUCTION kodunda kaldirilmis komut byte'i (0xA1-0xA4) "
            f"bulundu — 9.2.a tek-yonluluk ihlali olabilir: {hits}"
        )


def test_uks_active_code_has_no_button_estop_symbols(uks_telemetry_dir):
    """UKS'te RF komut kanali + E-STOP butonu kaldirildi (d8b321f). UKS
    PRODUCTION kodunda buton/estop sembolu SIFIR olmali. (AKS tarafinda
    fiziksel kontaktor/HMI E-STOP MANTIGI KASITLI OLARAK KALIR — bu bekci
    yalnizca UKS icindir, spesifikasyonla birebir.)"""
    pattern = re.compile(r"(?i)estop|e_stop|emergency|\bbutton\b|\bbtn\b")
    uks_files = iter_files(uks_telemetry_dir, ["Core"], (".c", ".h"))
    hits = count_active_code_hits(uks_files, pattern)
    assert not hits, f"UKS production kodunda buton/estop sembolu bulundu: {hits}"


def test_uks_sends_heartbeat_0xb0(uks_telemetry_dir):
    lora_h = read(uks_telemetry_dir / "Core/Inc/lora.h")
    main_c = read(uks_telemetry_dir / "Core/Src/main.c")

    hb_value = extract_define_int(lora_h, "LORA_HEARTBEAT_BYTE", source="UKS lora.h")
    assert hb_value == contract.LORA_HEARTBEAT_BYTE

    # main.c fiilen Lora_Send(..., &hb, ...) cagirip 0xB0 gonderiyor mu?
    assert re.search(r"LORA_HEARTBEAT_BYTE", main_c), (
        "UKS main.c LORA_HEARTBEAT_BYTE'i kullanmiyor gorunuyor — 0xB0 "
        "gonderimi POZITIF dogrulanamadi"
    )
    assert re.search(r"Lora_Send\s*\(\s*&lora_ctx\s*,\s*&hb", main_c), (
        "UKS main.c icinde heartbeat byte'inin Lora_Send ile TX edildigi "
        "kod bulunamadi"
    )


def test_aks_handles_0xb0_via_lora_rx_handler(aks_root):
    """AKS'te 0xB0 isleme (LoraRxHandler) POZITIF dogrulanir: UKS_HEARTBEAT_BYTE
    == 0xB0 ve lora_classify_rx_byte bu degeri HEARTBEAT olarak siniflandiriyor."""
    sys_config = read(aks_root / "include/SystemConfig.h")
    hb_value = extract_define_int(sys_config, "UKS_HEARTBEAT_BYTE", source="AKS SystemConfig.h")
    assert hb_value == contract.LORA_HEARTBEAT_BYTE

    handler = read(aks_root / "include/LoraRxHandler.h")
    assert "lora_classify_rx_byte" in handler
    m = re.search(
        r"lora_classify_rx_byte\s*\([^)]*\)\s*\{.*?\}", strip_c_comments(handler), re.DOTALL
    )
    assert m, "lora_classify_rx_byte govdesi bulunamadi"
    assert re.search(r"UKS_HEARTBEAT_BYTE", m.group(0)) and re.search(
        r"HEARTBEAT", m.group(0)
    ), "lora_classify_rx_byte, UKS_HEARTBEAT_BYTE'i HEARTBEAT'e eslemiyor"

    main_cpp = read(aks_root / "src/main.cpp")
    assert re.search(r"lora_classify_rx_byte\s*\(", main_cpp), (
        "AKS main.cpp lora_classify_rx_byte'i fiilen CAGIRMIYOR — 0xB0 "
        "isleme yolu POZITIF dogrulanamadi"
    )


# ===========================================================================
# 3.5 — Monitor sozlesmesi
# ===========================================================================


def test_monitor_header_matches_spec_byte_for_byte(csv_logger_module):
    assert csv_logger_module.HEADER == contract.MONITOR_HEADER


def test_aks_vehicle_params_confirmed_flag_exists(aks_root):
    text = read(aks_root / "include/VehicleParams.h")
    assert re.search(r"#define\s+VEHICLE_PARAMS_CONFIRMED\b", text), (
        "AKS VehicleParams.h icinde VEHICLE_PARAMS_CONFIRMED bayragi yok "
        "(9.2.c.i teyitsiz-build korumasi kaldirilmis olabilir)"
    )


def test_monitor_config_confirmed_flag_exists(monitor_root):
    text = read(monitor_root / "config.py")
    text_no_comments = strip_py_comments(text)
    assert re.search(r"^\s*CONFIG_CONFIRMED\s*=", text_no_comments, re.MULTILINE), (
        "Monitor config.py icinde CONFIG_CONFIRMED bayragi yok"
    )


# ===========================================================================
# 3.6 — AÇIK İŞ izleyicisi: Lithium Balance c-BMS alan kapsamı (9.2.c.ii)
#
# 2026-07-03 main-merge'inde BMS vendörü Solion SK -> Lithium Balance
# c-BMS'e gecti (donanim teyitli). Su an SADECE packV (CAN ID 0xE000)
# cozuldu; digger 7 ID (E001-E005, E032, E033) stub — TelemetryData'ya
# hicbir alan yazmiyor (bkz. TEKNIK_KONTROL_PROVASI.md "AÇIK İŞ" maddesi).
#
# Bu test BİLEREK xfail(strict=True): alan kapsamı eksikliğini izler.
# Ekip stub'lardan birine gerçek parse ekleyip TelemetryData'ya yazmaya
# baslarsa bu test XPASS eder ve strict=True nedeniyle SUITE KIRILIR —
# bu, TEKNIK_KONTROL_PROVASI.md'yi ve boot-log uyarisini guncellemeyi
# unutmamak icin bilincli bir hatirlatma mekanizmasidir.
# ===========================================================================

_LB_STUB_IDS = ["E001", "E002", "E003", "E004", "E005", "E032", "E033"]


@pytest.mark.xfail(
    strict=True,
    reason=(
        "9.2.c.ii AÇIK İŞ: Lithium Balance c-BMS'in 7 CAN ID'si (E001-E005,"
        " E032, E033) henuz TelemetryData'ya alan yazmiyor (yalnizca packV/"
        " E000 cozuldu). Biri gercek parse kazanirsa bu test XPASS eder —"
        " TEKNIK_KONTROL_PROVASI.md 'AÇIK İŞ' maddesini ve boot-log"
        " uyarisini guncelleyip bu testi kaldirin/genisletin."
    ),
)
def test_lb_bms_field_coverage_is_tracked(aks_root):
    can_parse_cpp = read(aks_root / "lib/CanParse/CanParse.cpp")
    cleaned = strip_c_comments(can_parse_cpp)

    still_stubbed = []
    for can_id in _LB_STUB_IDS:
        m = re.search(
            rf"bool\s+parseLbBms{can_id}\s*\([^)]*\)\s*\{{(.*?)\n\}}",
            cleaned, re.DOTALL,
        )
        assert m, f"parseLbBms{can_id} fonksiyonu CanParse.cpp'de bulunamadi"
        body = m.group(1)
        # Stub imzasi: (void)out; ve govde icinde out. alan atamasi YOK.
        if "(void)out;" in body and not re.search(r"\bout\.\w+\s*=", body):
            still_stubbed.append(can_id)

    # Hedef durum (henuz ULASILMADI): 7 ID'nin TAMAMI gercek parse kazanmis
    # olmali (still_stubbed bos). Bu satir BUGUN FAIL eder (xfail bunu
    # bekliyor); biri parse eklerse still_stubbed kuculur/bosalir, assert
    # PASS eder, xfail(strict=True) bunu XPASS'e cevirip suite'i kirar.
    assert still_stubbed == [], (
        f"Hala stub olan LB BMS ID'leri: {still_stubbed} — hedef: tumu "
        "gercek parse kazanmis olmali (bos liste)."
    )


# ===========================================================================
# 3.7 — aks_loop_sim.py outage sim post_live_ms bekcisi
# ===========================================================================


def test_aks_loop_sim_post_live_ms_is_derived_from_contract(aks_root):
    """417a665, REPLAY_BURST_PER_TICK'i 3'ten 1'e dusurdukten sonra
    aks_loop_sim.run_outage_simulation'in sabit post_live_ms=6000 varsayilani
    bayatlamis, 60 paketlik buffer'in yarisi replay edilmeden kalmisti (bkz.
    test_outage_simulation.py'nin FAIL etmesi). Duzeltme: post_live_ms=None
    varsayilan olsun, deger contract sabitlerinden turetilsin. Bu bekci,
    fonksiyonun tekrar sabit bir sayiya donmedigini kontrol eder — aksi
    halde REPLAY_BURST_PER_TICK bir daha degisirse ayni sozlesme kaymasi
    sessizce geri gelir."""
    src = strip_py_comments(read(aks_root / "tools/e2e/aks_loop_sim.py"))

    assert re.search(r"post_live_ms\s*:\s*int\s*\|\s*None\s*=\s*None", src), (
        "run_outage_simulation'in post_live_ms parametresi artik "
        "'int | None = None' imzasini tasimiyor gorunuyor (sabit bir sayiya "
        "geri donmus olabilir) — bu, REPLAY_BURST_PER_TICK degistiginde "
        "post_live_ms'in yeniden bayatlayip 417a665'teki gibi buffer'in "
        "yarisinin sessizce replay edilmeden kalmasina karsi korur; eger "
        "parametre bilincli olarak yeniden adlandirildiysa/imzasi degistiyse "
        "bu regex'i de AYNI COMMIT'TE yeni imzaya gore guncelleyin"
    )
    assert re.search(r"post_live_ms\s+is\s+None", src), (
        "post_live_ms icin 'None ise dinamik hesapla' dali bulunamadi — "
        "bu dal olmadan fonksiyon cagirani post_live_ms'i hep sabit/varsayilan "
        "degerle calistirir ve REPLAY_BURST_PER_TICK degistiginde outage "
        "testi sessizce yetersiz sureyle FAIL eder (417a665 kaymasi); eger "
        "hesaplama farkli bir kosul ifadesiyle (orn. 'if not post_live_ms') "
        "yeniden yazildiysa bu regex'i yeni ifadeye gore guncelleyin"
    )
    assert "contract.OFFLINE_SAMPLE_PERIOD_MS" in src and "contract.REPLAY_BURST_PER_TICK" in src, (
        "post_live_ms hesaplamasi artik contract.OFFLINE_SAMPLE_PERIOD_MS / "
        "contract.REPLAY_BURST_PER_TICK sabitlerine dayanmiyor gorunuyor — bu "
        "sabitler yerine hard-code sayilar kullanilirsa REPLAY_BURST_PER_TICK "
        "gelecekte tekrar degistiginde post_live_ms otomatik uyum saglamaz ve "
        "417a665'teki sozlesme kaymasi baska bir sekilde geri gelir; eger "
        "hesaplama mesru bir sekilde contract'tan farkli/ek sabitler "
        "kullanacak sekilde genisletildiyse bu test'i de o sabitleri "
        "kontrol edecek sekilde guncelleyin"
    )
