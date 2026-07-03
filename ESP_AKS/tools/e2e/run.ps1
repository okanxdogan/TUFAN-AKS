# tools/e2e/run.ps1 - tek komutla AKS/UKS/Monitor sozlesme dogrulayicisini
# kosar (donanimsiz, gercek bekleme yok) ve ozet basar.
#
# UKS/Monitor repo yollari varsayilan olarak kesfedilir (bkz. conftest.py);
# override etmek icin:
#   $env:TUFAN_UKS_REPO = "C:\path\to\TUFAN-UKS-TELEMETRY"
#   $env:TUFAN_MONITOR_REPO = "C:\path\to\TUFAN-Monitor"

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$PythonCmd = "python"
try {
    & $PythonCmd --version *> $null
} catch {
    $PythonCmd = "python3"
}

Write-Host "=== tools/e2e: bagimliliklar kontrol ediliyor ==="
& $PythonCmd -c "import pytest" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "pytest bulunamadi; kuruluyor (requirements.txt)..."
    & $PythonCmd -m pip install -r requirements.txt
}

Write-Host "=== tools/e2e: suite calistiriliyor ==="
& $PythonCmd -m pytest -v @args
$Status = $LASTEXITCODE

Write-Host ""
if ($Status -eq 0) {
    Write-Host "=== SONUC: PASS ==="
} else {
    Write-Host "=== SONUC: FAIL (exit=$Status) ==="
}
exit $Status
