"""tools/e2e pytest konfigurasyonu: UKS ve Monitor repo yollarini cozer.

Bu suite ESP_AKS reposunun KOKUNE (tools/e2e/) kurulur. Kardes repolar
(TUFAN-UKS-TELEMETRY, TUFAN-Monitor) varsayilan olarak asagidaki aday
konumlarda aranir (ilk bulunan kullanilir); ortam degiskenleriyle
override edilebilir:

    TUFAN_UKS_REPO       UKS repo kok dizini (icinde UKS-Telemetry/ olmali)
    TUFAN_MONITOR_REPO   TUFAN-Monitor repo kok dizini

Hicbiri bulunamazsa anlasilir bir hata mesajiyla durur (bkz.
_resolve_or_die).
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

E2E_DIR = Path(__file__).resolve().parent
AKS_ROOT = E2E_DIR.parent.parent  # tools/e2e -> tools -> ESP_AKS kok

# Aday goreli yollar, ESP_AKS kokune (AKS_ROOT) gore, oncelik sirasiyla.
# 1) Duz kardes dizin varsayimi (dokumantasyonda soylenen "kardes dizinler").
# 2) Bu gelistirme makinesinde gercekte kesfedilen yerlesim: ESP_AKS bir
#    "AKS/" sarmalayici klasorde, UKS ise "uks/" sarmalayicida — ikisi de
#    ortak bir ust dizinin (Desktop) altinda kardes REPO GRUPLARI olarak
#    durur. Bu yuzden ikinci aday iki seviye yukari cikip oradan arar.
UKS_CANDIDATES = [
    "../TUFAN-UKS-TELEMETRY",
    "../../TUFAN-UKS-TELEMETRY",
    "../../uks/TUFAN-UKS-TELEMETRY",
    "../uks/TUFAN-UKS-TELEMETRY",
]
MONITOR_CANDIDATES = [
    "../TUFAN-Monitor",
    "../../TUFAN-Monitor",
]

# Repo'nun gercekten dogru yer oldugunu dogrulamak icin varligi aranan
# "imza" dosyalari (yanlis bir dizini sessizce kabul etmemek icin).
UKS_MARKER = "UKS-Telemetry/Core/Inc/telemetry.h"
MONITOR_MARKER = "csv_logger.py"


def _resolve_or_die(env_var: str, candidates: list[str], marker: str, label: str) -> Path:
    env_val = os.environ.get(env_var)
    if env_val:
        p = Path(env_val).resolve()
        if (p / marker).is_file():
            return p
        raise RuntimeError(
            f"{env_var}={env_val} ayarli ama '{marker}' orada bulunamadi. "
            f"{label} repo kok dizinini dogru gosterdiginden emin olun."
        )

    tried = []
    for cand in candidates:
        p = (AKS_ROOT / cand).resolve()
        tried.append(str(p))
        if (p / marker).is_file():
            return p

    raise RuntimeError(
        f"{label} repo bulunamadi. Denenen konumlar:\n  "
        + "\n  ".join(tried)
        + f"\nCozum: {env_var} ortam degiskenini {label} repo kok dizinine "
        "isaret edecek sekilde ayarlayin, orn.\n"
        f"  (bash)  export {env_var}=/path/to/{label}\n"
        f"  (pwsh)  $env:{env_var} = 'C:\\path\\to\\{label}'"
    )


@pytest.fixture(scope="session")
def aks_root() -> Path:
    return AKS_ROOT


@pytest.fixture(scope="session")
def uks_root() -> Path:
    return _resolve_or_die("TUFAN_UKS_REPO", UKS_CANDIDATES, UKS_MARKER,
                            "TUFAN-UKS-TELEMETRY")


@pytest.fixture(scope="session")
def monitor_root() -> Path:
    return _resolve_or_die("TUFAN_MONITOR_REPO", MONITOR_CANDIDATES,
                            MONITOR_MARKER, "TUFAN-Monitor")


@pytest.fixture(scope="session")
def uks_telemetry_dir(uks_root: Path) -> Path:
    return uks_root / "UKS-Telemetry"


@pytest.fixture(scope="session")
def csv_logger_module(monitor_root: Path):
    """TUFAN-Monitor'un GERCEK csv_logger modulunu import eder.

    monitor.py DEGIL, yalnizca csv_logger.py import edilir — bu modulun
    tek bagimliligi stdlib'dir (os, datetime), boylece pyserial/matplotlib
    kurulu olmasa da bu testler calisabilir.
    """
    monitor_root_str = str(monitor_root)
    inserted = monitor_root_str not in sys.path
    if inserted:
        sys.path.insert(0, monitor_root_str)
    try:
        import csv_logger
        return csv_logger
    finally:
        if inserted:
            sys.path.remove(monitor_root_str)
