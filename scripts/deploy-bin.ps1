# Legt die Laufzeitdateien neben FalloutTogether.exe (macht sonst `xmake install`):
# CEF (libcef.dll, Ressourcen, locales), Crashpad und die Mauszeiger der UI.
# Wiederholen ist gefahrlos. Vorher bauen: .\xm.ps1 build -y FalloutImmersiveLauncher TPProcess
$ErrorActionPreference = 'Stop'

$Repo = Split-Path $PSScriptRoot
$Bin  = Join-Path $Repo 'build\windows\x64\releasedbg'
$Pkg  = "$env:FTR_WORK\xg\.xmake\packages"

$cef = Get-ChildItem "$Pkg\c\cef\100.0.24" -Directory | Select-Object -First 1
if (-not $cef) { throw 'CEF-Paket nicht gefunden, erst bauen.' }
Copy-Item "$($cef.FullName)\bin\*" $Bin -Recurse -Force

$sentry = Get-ChildItem "$Pkg\s\sentry-native\*\*\bin" -Directory | Select-Object -First 1
if ($sentry) { Copy-Item "$($sentry.FullName)\*" $Bin -Recurse -Force }

$img = Join-Path $Bin 'assets\images'
New-Item -ItemType Directory -Force $img | Out-Null
Copy-Item "$Repo\Code\client\assets\cursor.dds", "$Repo\Code\client\assets\cursor.png" $img -Force

foreach ($f in 'FalloutTogether.exe', 'EarlyLoad.dll', 'TPProcess.exe', 'libcef.dll', 'd3dcompiler_47.dll', 'resources.pak', 'locales') {
    if (-not (Test-Path (Join-Path $Bin $f))) { throw "Fehlt in ${Bin}: $f" }
}
Write-Host 'Laufzeitdateien sind vollständig.'
