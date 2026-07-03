#!/usr/bin/env bash
# tools/e2e/run.sh — tek komutla AKS/UKS/Monitor sozlesme dogrulayicisini
# kosar (donanimsiz, gercek bekleme yok) ve ozet basar.
#
# UKS/Monitor repo yollari varsayilan olarak kesfedilir (bkz. conftest.py);
# override etmek icin:
#   TUFAN_UKS_REPO=/path/to/TUFAN-UKS-TELEMETRY
#   TUFAN_MONITOR_REPO=/path/to/TUFAN-Monitor
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PYTHON="${PYTHON:-python}"

if ! command -v "$PYTHON" >/dev/null 2>&1; then
  PYTHON=python3
fi

echo "=== tools/e2e: bagimliliklar kontrol ediliyor ==="
"$PYTHON" -c "import pytest" 2>/dev/null || {
  echo "pytest bulunamadi; kuruluyor (requirements.txt)..."
  "$PYTHON" -m pip install -r requirements.txt
}

echo "=== tools/e2e: suite calistiriliyor ==="
set +e
"$PYTHON" -m pytest -v "$@"
STATUS=$?
set -e

echo ""
if [ "$STATUS" -eq 0 ]; then
  echo "=== SONUC: PASS ==="
else
  echo "=== SONUC: FAIL (exit=$STATUS) ==="
fi
exit "$STATUS"
