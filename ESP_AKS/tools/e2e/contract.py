"""Tek dogruluk kaynagi: AKS<->UKS<->Monitor telemetri sozlesmesinin Python
eslenigi.

Bu dosya HICBIR firmware/Monitor kaynagini import ETMEZ ve degistirmez.
Buradaki sabitler/kurallar, asagidaki gercek kaynaklardan EL ILE transkribe
edilmistir (kaynak dosya + satir aralik referanslari yorumlarda verilmistir).
Bir sozlesme sabiti gercek kaynakta degisirse, bu dosyadaki deger de elle
guncellenmelidir; contract-drift bekcileri (test_contract_drift.py) bu iki
tarafin senkron kalip kalmadigini otomatik denetler.

Kaynaklar:
  UKS  Core/Inc/telemetry.h, Core/Src/telemetry.c (Decode_Line)
  AKS  lib/Telemetry/Telemetry.h, Telemetry.cpp (sendStatus)
  AKS  lib/Telemetry/TelemetrySanitize.h
  AKS  include/SystemConfig.h (P6 offline buffer sabitleri)
  UKS  Core/Src/main.c (CSV forward satiri, satir ~278-288)
  UKS  Core/Inc/e22_regs.h ; AKS include/E22Regs.h (E22 config sozlesmesi)
"""

from __future__ import annotations

from dataclasses import dataclass

# ===========================================================================
# Protokol sabitleri
# ===========================================================================

# UKS telemetry.h: TEL_FIELD_COUNT (TEL tag + 18 veri alani = 19 toplam).
# Tokenize() bu sayidan farkli alan sayisinda TUM satiri reddeder
# (telemetry.c Decode_Line, nf != TEL_FIELD_COUNT -> parse_fail).
#
# SAYIM KONVANSIYONU NOTU: "19" tokenize edilen VIRGULLE-AYRILMIS alan
# sayisidir (f[0]="TEL" dahil). "18" ise SAYISAL/mantiksal veri alani
# sayisidir (ver..spd_x10, TEL tag'i haric). Bu dosyada ve testlerde
# TOKEN_COUNT=19 kullanilir (telemetry.h dogrudan bu adı kullanır); "18"
# yalnizca konusma dilinde "veri alani sayisi" anlaminda gecer, ayrı bir
# sabit DEGILDIR.
TOKEN_COUNT = 19
VER = 2

# ===========================================================================
# Alan aralik sozlesmesi (UKS Core/Src/telemetry.c Decode_Line, satir 236-256)
# ===========================================================================
# Alan sirasi (TEL haric, f[] index'i): 1=ver 2=seq 3=rpm 4=torque 5=motorErr
# 6=motorValid 7=motorTimeout 8=cellVMax 9=cellVMin 10=tempH 11=tempL
# 12=sysState 13=packV 14=current 15=soc 16=bmsValid 17=ts_ms 18=spd_x10

U32_MAX = 4294967295

FIELD_ORDER = [
    "ver", "seq", "rpm", "torque", "motor_err", "motor_valid",
    "motor_timeout", "cell_vmax", "cell_vmin", "temp_h", "temp_l",
    "sys_state", "pack_v", "current", "soc", "bms_valid", "ts_ms", "spd_x10",
]

# (min, max) kapsayici araliklar; ts_ms/seq ayrica uint32 (Parse_U32) ile
# parse edilir, digerleri isaretli long (Parse_Int) ile.
FIELD_RANGES = {
    "ver": (0, 255),
    "seq": (0, U32_MAX),
    "rpm": (0, 65535),            # tip siniri; TEL_RPM_MAX=20000 ayrica sanity-check
    "torque": (-32768, 32767),
    "motor_err": (0, 255),
    "motor_valid": (0, 1),
    "motor_timeout": (0, 1),
    "cell_vmax": (0, 65535),
    "cell_vmin": (0, 65535),
    "temp_h": (-128, 127),
    "temp_l": (-128, 127),
    "sys_state": (1, 4),
    "pack_v": (0, 65535),
    # INT32_MIN (-2147483648) bilincli olarak HARIC (UKS Parse_Int min
    # -2147483647 kullanir) — AKS TelemetrySanitize.sanitizeCurrent bu
    # degeri asla uretmez.
    "current": (-2147483647, 2147483647),
    "soc": (0, 10000),
    "bms_valid": (0, 1),
    "ts_ms": (0, U32_MAX),
    "spd_x10": (0, 3000),
}

# UKS telemetry.c: TEL_RPM_MAX (sanity, tip sinirinin OTESINDE ek kontrol)
TEL_RPM_MAX = 20000

# ===========================================================================
# AKS sanitize garantileri (lib/Telemetry/TelemetrySanitize.h)
# ===========================================================================

INT32_MIN = -2147483648


def sanitize_system_state(raw: int) -> int:
    """TelemetrySanitize::sanitizeSystemState — 1..4 disinda FAULT(4)."""
    return raw if 1 <= raw <= 4 else 4


def sanitize_soc(raw: int) -> int:
    """TelemetrySanitize::sanitizeSoc — 10000 ustunu kirpar."""
    return 10000 if raw > 10000 else raw


def sanitize_current(raw: int) -> int:
    """TelemetrySanitize::sanitizeCurrent — INT32_MIN'i INT32_MIN+1 yapar."""
    return INT32_MIN + 1 if raw == INT32_MIN else raw


# AKS lib/Telemetry/Telemetry.h: TEL_SPD_X10_MAX — rpmToSpeedKmhX10Impl
# icinde spd_x10 bu degere clamp'lenir (sanitizeForUplink'in DISINDA, hiz
# hesaplama zincirinde).
TEL_SPD_X10_MAX = 3000


def clamp_spd_x10(spd_x10: int) -> int:
    return TEL_SPD_X10_MAX if spd_x10 >= TEL_SPD_X10_MAX else spd_x10


# ===========================================================================
# E22-400T30D-V2 config sozlesmesi (UKS e22_regs.h / AKS E22Regs.h)
# ===========================================================================

E22_REG0 = 0x64
E22_REG1 = 0x00
E22_REG2 = 0x17
E22_REG3 = 0x00
E22_ADDH = 0x00
E22_ADDL = 0x00
E22_NETID = 0x00
# G7: heartbeat-injection kapatma icin sifir-disi ortak CRYPT anahtari
# (E22_CRYPT_KEY=0x5A3C). AKS E22Regs.h (E22_CFG_CRYPT_H/L) ve UKS e22_regs.h
# (E22_VAL_CRYPT_H/L) ile AYNI COMMIT'te senkron (bkz. E22_CRYPT_SENKRON.md).
E22_CRYPT_H = 0x5A
E22_CRYPT_L = 0x3C

# Config modu pin seviyeleri (M0=0, M1=1 — E32'den farkli)
E22_MODE_CONFIG_M0 = 0
E22_MODE_CONFIG_M1 = 1

# Bench dump / boot-log formati (UKS lora.c, AKS main.cpp + e22_diagnostic.cpp)
E22_DUMP_LINE_FORMAT = "E22REG,0x%02X,0x%02X"

# ===========================================================================
# Tek yonluluk (9.2.a)
# ===========================================================================

LORA_HEARTBEAT_BYTE = 0xB0  # UKS->AKS tek mesru byte

# Kaldirilan eski komut sabitleri — bunlarin aktif kodda SIFIR gecmesi
# beklenir (bkz. test_contract_drift.py one-directionality guard).
REMOVED_COMMAND_BYTES = [0xA1, 0xA2, 0xA3, 0xA4]

# ===========================================================================
# P6 offline buffer / replay sabitleri (AKS include/SystemConfig.h)
# ===========================================================================

LORA_TX_PERIOD_MS = 500          # 2 Hz telemetry uplink tick periyodu (link flapping duzeltmesi — bkz. SystemConfig.h)
OFFLINE_SAMPLE_PERIOD_MS = 1000  # kesinti sirasi buffer ornekleme periyodu (1 Hz)
OB_CAPACITY = 75                 # offline buffer kapasitesi (60 sn x 1 Hz + %25 marj)
REPLAY_BURST_PER_TICK = 1        # link-up sonrasi tik basina en fazla replay
BOOT_LINK_GRACE_MS = 5000        # boot'tan itibaren ilk heartbeat icin tanina sure
LINK_TIMEOUT_MS = 9000           # yarim-dubleks kanal tikanikligi nedeniyle ~5-6 sn'lik fiili heartbeat araligina marj (bkz. SystemConfig.h)


def build_tel_line(f: dict) -> str:
    """AKS Telemetry.cpp::sendStatus format string'inin Python eslenigi.

    Girdi: FIELD_ORDER anahtarlarina sahip bir dict (parse_uks_frame()
    ciktisiyla ayni sekil). Cikti, gercek sendStatus() ciktisiyla birebir
    aynı format sirasindadir (bkz. test_frame_contract.py
    test_build_tel_line_round_trips_golden_vector — golden fixture'la
    round-trip dogrulanir).
    """
    return "TEL,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n" % (
        f["ver"], f["seq"], f["rpm"], f["torque"], f["motor_err"],
        f["motor_valid"], f["motor_timeout"], f["cell_vmax"], f["cell_vmin"],
        f["temp_h"], f["temp_l"], f["sys_state"], f["pack_v"], f["current"],
        f["soc"], f["bms_valid"], f["ts_ms"], f["spd_x10"],
    )


# ===========================================================================
# Monitor sozlesmesi (TUFAN-Monitor csv_logger.py / config.py)
# ===========================================================================

MONITOR_HEADER = "zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh"

# UKS -> PC forward format (UKS Core/Src/main.c, satir ~278-288):
#   printf("CSV,%lu,%u,%d,%u,%u,%lu\r\n",
#          ts_ms, spd_x10, tempH, packV, soc, seq)


def build_forward_line(frame: dict) -> str:
    """UKS main.c'nin PC-forward CSV satirinin Python eslenigi.

    frame: parse_uks_frame() ciktisi (veya esdeger alanlara sahip dict).
    Donus: sonunda \\r\\n olan "CSV,..." satiri (gercek UKS ciktisiyla
    birebir bicimde).
    """
    return "CSV,%u,%u,%d,%u,%u,%u\r\n" % (
        frame["ts_ms"],
        frame["spd_x10"],
        frame["temp_h"],
        frame["pack_v"],
        frame["soc"],
        frame["seq"],
    )


# ===========================================================================
# UKS kabul kurallarinin Python eslenigi (Decode_Line'in saf-fonksiyon
# yeniden yazimi — telemetry.c'yi COGALTMAZ, yalnizca AYNI kurallari farkli
# bir dilde uygular; ikisinin sozlesmesi test_contract_drift.py ile
# denetlenir).
# ===========================================================================


@dataclass
class UksRejection:
    reason: str


def parse_uks_frame(line: str) -> dict | UksRejection:
    """UKS Decode_Line'in Python eslenigi.

    Basarili durumda FIELD_ORDER anahtarlariyla bir dict doner. Reddedilen
    her durumda UksRejection(reason=...) doner (telemetry.c'deki stats
    sayaclarina karsilik gelen kisa neden etiketleriyle).
    """
    raw = line.rstrip("\r\n")
    fields = raw.split(",")

    if len(fields) != TOKEN_COUNT:
        return UksRejection("parse_fail")

    if fields[0] != "TEL":
        return UksRejection("bad_tag")

    values = {}
    for name, tok in zip(FIELD_ORDER, fields[1:]):
        lo, hi = FIELD_RANGES[name]
        if not _is_strict_int_token(tok, allow_sign=(lo < 0)):
            return UksRejection("parse_fail")
        v = int(tok)
        if v < lo or v > hi:
            return UksRejection("parse_fail")
        values[name] = v

    if values["ver"] != VER:
        return UksRejection("bad_version")

    if values["rpm"] > TEL_RPM_MAX:
        return UksRejection("range_fail")

    return values


def _is_strict_int_token(tok: str, allow_sign: bool) -> bool:
    """UKS Parse_Int/Parse_U32'nin token-gecerlilik esdegeri.

    Parse_Int (isaretli alanlar): bastaki tek '+' ya da '-' kabul eder,
    ardindan yalnizca rakam. Parse_U32 (ts_ms/seq): hicbir isaret kabul
    etmez, yalnizca rakam. Bos token her iki durumda da reddedilir."""
    if tok == "":
        return False
    body = tok
    if allow_sign and tok[0] in ("-", "+"):
        body = tok[1:]
    if body == "":
        return False
    return body.isdigit()
