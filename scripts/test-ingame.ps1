# Spieltest mit der Testkopie.
# Startet Server, Fallout Together mit der Testkopie und einen Bot. Der Client verbindet sich automatisch
# (TP_AUTOCONNECT), sobald der Spieler in einer Zelle ist. Für den Weg dorthin lädt das Testprofil per
# sStartingConsoleCommand die Testzelle (coc RedRocketExt).
#   .\test-ingame.ps1                 # eine Spielinstanz + Bot
#   .\test-ingame.ps1 -Clients 2      # zwei Spielinstanzen (falls Fallout 4 das zulässt)
param(
    [int]$Clients = 1,
    [int]$Port = 10610,
    [int]$TimeoutMinutes = 5,
    # Nach dem Test weiterlaufen lassen, bis das Spiel beendet wird (zum Anschauen und Mitspielen)
    [switch]$Hold,
    # Diagnose: Overlay und ImGui nicht zeichnen
    [switch]$NoOverlay,
    # Testkopie mit der aktuellen Spielversion statt 1.10.163
    [switch]$NextGen
)
$ErrorActionPreference = 'Stop'

$Bin = Join-Path (Split-Path $PSScriptRoot) 'build\windows\x64\releasedbg'
$Game = if ($NextGen) { "$env:FTR_WORK\Fallout4-B" } else { "$env:FTR_WORK\Fallout4-A" }
$Run = "$env:FTR_WORK\ingame-test"
$Password = 'bottest'

if (-not $NextGen -and (Get-Item "$Game\Fallout4.exe").VersionInfo.FileVersion -notlike '1.10.163*') { throw 'Testkopie ist noch nicht auf 1.10.163 (phase2-downgrade.ps1).' }
if (-not (Test-Path "$env:FTR_WORK\profile\ACTIVE")) { throw 'Testprofil nicht aktiv: erst .\test-profile.ps1 -Enter' }
New-Item -ItemType Directory -Force $Run | Out-Null

$serverLog = Join-Path $Run 'server.out.log'
$server = Start-Process -FilePath (Join-Path $Bin 'FalloutTogetherServer.exe') -WorkingDirectory $Run -PassThru -WindowStyle Hidden `
    -ArgumentList "--GameServer:uPort=$Port", "--GameServer:sPassword=$Password", '--bConsole=false' `
    -RedirectStandardOutput $serverLog -RedirectStandardError (Join-Path $Run 'server.err.log')
$games = @()
try {
    Start-Sleep -Seconds 3
    $env:TP_AUTOCONNECT = "127.0.0.1:$Port"
    $env:TP_PASSWORD = $Password
    if ($NoOverlay) { $env:TP_NO_OVERLAY_RENDER = '1' }
    foreach ($i in 1..$Clients) {
        # --exePath ist Pflicht, sonst nimmt der Launcher die Original-Installation aus der Registry
        $games += Start-Process -FilePath (Join-Path $Bin 'FalloutTogether.exe') -WorkingDirectory $Bin -PassThru `
            -ArgumentList '--exePath', "$Game\Fallout4.exe"
        $null = $games[-1].Handle  # sonst liefert ExitCode später nichts
        Start-Sleep -Seconds 20
    }
    Remove-Item Env:TP_AUTOCONNECT, Env:TP_PASSWORD
    Remove-Item Env:TP_NO_OVERLAY_RENDER -ErrorAction SilentlyContinue

    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    $joined = 0
    while ((Get-Date) -lt $deadline) {
        $joined = @(Select-String -Path $serverLog -Pattern "New player '.*' \[[0-9a-f]+\] connected" -ErrorAction SilentlyContinue).Count
        if ($joined -ge $Clients) { break }
        if (@($games | Where-Object { -not $_.HasExited }).Count -eq 0) { break }
        Start-Sleep -Seconds 5
    }
    Write-Host "Spielclients verbunden: $joined von $Clients"

    # Bot dazu: muss die Spieler als Mitspieler sehen
    $bot = & (Join-Path $Bin 'FalloutBot.exe') "127.0.0.1:$Port" 'Beobachter' 15 $Password
    $bot | Write-Host
    $seen = @($bot -match 'Mitspieler beigetreten').Count

    if ($Hold) {
        Write-Host 'Spiel läuft weiter, Server bleibt an. Zum Beenden das Spiel schließen.'
        $games | ForEach-Object { $_.WaitForExit() }
    }

    # Mit -Hold ist ein sauberes Beenden durch den Spieler kein Fehler
    $crashed = @($games | Where-Object { $_.HasExited -and -not ($Hold -and $_.ExitCode -eq 0) })
    Write-Host "Beendete Spielinstanzen: $($crashed.Count)"
    foreach ($g in $crashed) { Write-Host ("  Exit-Code: 0x{0:X8}, Laufzeit {1:N0} s" -f $g.ExitCode, ($g.ExitTime - $g.StartTime).TotalSeconds) }
    $clientLog = Join-Path $Bin 'logs\tp_client.log'
    if (Test-Path $clientLog) { Write-Host '--- tp_client.log (Ende) ---'; Get-Content $clientLog -Tail 25 | Write-Host }
    if ($joined -ge $Clients -and $seen -ge $Clients -and $crashed.Count -eq 0) {
        Write-Host 'Spieltest bestanden.' -ForegroundColor Green
        $result = 0
    } else {
        Write-Host "Spieltest FEHLGESCHLAGEN. Logs: $Run, $Bin\logs" -ForegroundColor Red
        $result = 1
    }
}
finally {
    $games | Where-Object { -not $_.HasExited } | ForEach-Object { Stop-Process -Id $_.Id -Force }
    Get-Process Fallout4 -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Bin*" -or $_.Path -like "$Game*" } | Stop-Process -Force
    if (-not $server.HasExited) { Stop-Process -Id $server.Id -Force }
}
exit $result
