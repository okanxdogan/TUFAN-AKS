"""Contract-drift bekcileri icin ortak, salt-okur yardimci fonksiyonlar.

Bu modul hicbir kaynak dosyayi YAZMAZ — yalnizca okur ve regex ile
sabit/sembol arar. Amac: firmware/Monitor kaynak dosyalarindaki sozlesme
sabitleri gelecekte kirilirsa (yanlislikla degistirilirse) bu bekcilerin
FAIL vermesidir.
"""

from __future__ import annotations

import re
from pathlib import Path


def read(path: Path) -> str:
    if not path.is_file():
        raise AssertionError(f"beklenen kaynak dosya yok: {path}")
    return path.read_text(encoding="utf-8", errors="strict")


def extract_define_raw(text: str, name: str, *, source: str) -> str:
    m = re.search(rf'#define\s+{re.escape(name)}\s+([^\r\n]+)', text)
    if not m:
        raise AssertionError(f"{source}: '#define {name}' bulunamadi")
    value = m.group(1)
    # Satir-sonu // yorumunu ve satir-ici /* ... */ yorumunu deger
    # kismindan at.
    value = value.split("//")[0]
    value = re.sub(r"/\*.*?\*/", " ", value)
    return value.strip()


def extract_define_int(text: str, name: str, *, source: str) -> int:
    raw = extract_define_raw(text, name, source=source)
    raw = re.sub(r"[UuLl]+$", "", raw.strip())
    try:
        return int(raw, 0)
    except ValueError as exc:
        raise AssertionError(
            f"{source}: '#define {name}' sayisal degil ayristirilamadi: {raw!r}"
        ) from exc


def strip_c_comments(text: str) -> str:
    """// ve /* */ yorumlarini kaba (string-literal farkindaligi olmayan,
    ama bu kaynak dosyalarda yeterli) sekilde temizler."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    text = re.sub(r"//[^\r\n]*", "", text)
    return text


def strip_py_comments(text: str) -> str:
    return re.sub(r"#[^\r\n]*", "", text)


def iter_files(root: Path, subdirs: list[str], extensions: tuple[str, ...]) -> list[Path]:
    files: list[Path] = []
    for sub in subdirs:
        base = root / sub
        if not base.is_dir():
            continue
        for ext in extensions:
            files.extend(sorted(base.rglob(f"*{ext}")))
    return files


def count_active_code_hits(files: list[Path], pattern: re.Pattern) -> dict[Path, list[str]]:
    """Her dosya icin (yorumlar temizlenmis) eslesen tum satirlari dondurur."""
    hits: dict[Path, list[str]] = {}
    for f in files:
        cleaned = strip_c_comments(read(f))
        matches = pattern.findall(cleaned)
        if matches:
            hits[f] = matches
    return hits
