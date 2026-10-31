# Spieltest Bau-Sync: Testkopie startet in Red Rocket, ein Bot schickt echte Bau-Änderungen.
# Geprüft wird die Empfängerseite im Spiel: bauen, verschieben, entfernen, Nachholen ohne Doppelung.
# Das Testprofil wird vorher aktiviert und danach immer wieder zurückgetauscht.
#   .\test-workshop-ingame.ps1         # Spieler ist da, Änderungen kommen live
#   .\test-workshop-ingame.ps1 -Late   # erst bauen, dann tritt der Spieler bei und bekommt sie vom Server
param(
    [int]$Port = 10620,
    [switch]$Late,
    [int]$TimeoutMinutes = 6
)
$ErrorActionPreference = 'Stop'
if (-not $env:FTR_WORK) { $env:FTR_WORK = 'E:\FO4Test' }

$Root = Split-Path $PSScriptRoot
$Bin = Join-Path $Root 'build\windows\x64\releasedbg'
$Game = "$env:FTR_WORK\Fallout4-B"
$Run = "$env:FTR_WORK\ingame-test"
$Password = 'bottest'
$ClientLog = Join-Path $Bin 'logs\tp_client.log'
$failed = @()

if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { throw 'Fallout 4 läuft, bitte erst beenden.' }
New-Item -ItemType Directory -Force $Run | Out-Null

# Eigene Datei, kein Hardlink: vorher löschen, nie beschreiben
$offsets = "$Game\Data\F4SE\Plugins\FalloutTogether"
New-Item -ItemType Directory -Force $offsets | Out-Null
Remove-Item "$offsets\offsets-1-11-240-0.bin" -ErrorAction SilentlyContinue
python -I (Join-Path $Root 'Tools\port\make_offsets.py') --ids (Join-Path $Root 'data\ids-1.10.163.csv') --manual (Join-Path $Root 'data\final-1-11-240-0.csv') --out "$offsets\offsets-1-11-240-0.bin"
if ($LASTEXITCODE) { throw 'make_offsets fehlgeschlagen' }

Remove-Item $ClientLog -ErrorAction SilentlyContinue
& (Join-Path $PSScriptRoot 'test-profile.ps1') -Enter

$server = $null
$gameProcess = $null
try {
    $serverLog = Join-Path $Run 'server.out.log'
    $server = Start-Process -FilePath (Join-Path $Bin 'FalloutTogetherServer.exe') -WorkingDirectory $Run -PassThru -WindowStyle Hidden `
        -ArgumentList "--GameServer:uPort=$Port", "--GameServer:sPassword=$Password", '--bConsole=false' `
        -RedirectStandardOutput $serverLog -RedirectStandardError (Join-Path $Run 'server.err.log')
    Start-Sleep -Seconds 3

    if ($Late) { & (Join-Path $Bin 'FalloutBot.exe') "127.0.0.1:$Port" 'Baumeister' 3 $Password 'spieltest' | Write-Host }

    $env:TP_AUTOCONNECT = "127.0.0.1:$Port"
    $env:TP_PASSWORD = $Password
    $gameProcess = Start-Process -FilePath (Join-Path $Bin 'FalloutTogether.exe') -WorkingDirectory $Bin -PassThru -ArgumentList '--exePath', "$Game\Fallout4.exe"
    $null = $gameProcess.Handle
    Remove-Item Env:TP_AUTOCONNECT, Env:TP_PASSWORD

    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    $joined = $false
    while ((Get-Date) -lt $deadline -and -not $gameProcess.HasExited) {
        $players = @(Select-String -Path $serverLog -Pattern "New player '.*' \[[0-9a-f]+\] connected" -ErrorAction SilentlyContinue).Count
        if ($players -ge $(if ($Late) { 2 } else { 1 })) { $joined = $true; break }
        Start-Sleep -Seconds 3
    }
    if (-not $joined) { throw 'Spiel hat sich nicht verbunden' }

    # Zellen und Werkbank laden lassen
    Start-Sleep -Seconds 25
    if (-not $Late) { & (Join-Path $Bin 'FalloutBot.exe') "127.0.0.1:$Port" 'Baumeister' 5 $Password 'spieltest' | Write-Host }
    Start-Sleep -Seconds 20

    if ($gameProcess.HasExited) { $failed += "Spiel beendet (Exit $($gameProcess.ExitCode))" }
}
finally {
    if ($gameProcess -and -not $gameProcess.HasExited) { $gameProcess.CloseMainWindow() | Out-Null; Start-Sleep -Seconds 8 }
    Get-Process Fallout4, FalloutTogether -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    if ($server -and -not $server.HasExited) { Stop-Process -Id $server.Id -Force }
    Start-Sleep -Seconds 3
    & (Join-Path $PSScriptRoot 'test-profile.ps1') -Leave
}

$log = @(Get-Content $ClientLog -ErrorAction SilentlyContinue)
$log | Where-Object { $_ -match 'Workshop' } | Write-Host
# Nachgeholt wird der zusammengefasste Stand: das verschrottete Teil entsteht gar nicht erst
$placed = if ($Late) { 3 } else { 4 }
if (@($log -match 'Workshop sync: placed').Count -ne $placed) { $failed += "Es hätten $placed Objekte entstehen müssen" }
if (-not $Late -and -not ($log -match 'Workshop sync: moved')) { $failed += 'Verschieben fehlt' }
if (-not $Late -and -not ($log -match 'Workshop sync: removed')) { $failed += 'Entfernen fehlt' }
if (-not ($log -match 'Workshop sync: .* is already here')) { $failed += 'Vorhandenes Objekt nicht erkannt' }
if ($log -match 'Workshop sync: .*(missing|not found|could not|is not here)') { $failed += 'Fehlerzeile im Log' }
if (-not ($log -match 'Workshop sync: wired ')) { $failed += 'Kabel fehlt' }
if (@($log -match 'Workshop sync: [0-9A-F]+ powered ').Count -ne 2) { $failed += 'Stromzustand fehlt im Log' }

if ($failed) {
    Write-Host "FEHLGESCHLAGEN:`n - $($failed -join "`n - ")" -ForegroundColor Red
    exit 1
}
Write-Host 'Bau-Sync im Spiel bestanden.' -ForegroundColor Green
# Red Rocket gehört im Testprofil niemandem, Strom ist hier also kein Pflichtergebnis
if (-not ($log -match 'Workshop sync: [0-9A-F]+ powered true')) { Write-Host 'Warnung: Kabel steht, aber kein Objekt meldet Strom.' -ForegroundColor Yellow }
exit 0
