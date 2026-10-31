# Automatischer Servertest ohne Spiel: Server starten, Bots anmelden lassen, Ergebnis prüfen.
#   .\test-server-bots.ps1                 # 2 Bots, Port 10600
#   .\test-server-bots.ps1 -Bots 3 -Port 10601
param(
    [int]$Bots = 2,
    [int]$Port = 10600,
    [int]$Seconds = 8
)
$ErrorActionPreference = 'Stop'

$Bin = Join-Path (Split-Path $PSScriptRoot) 'build\windows\x64\releasedbg'
$Run = "$env:FTR_WORK\server-test"
$Password = 'bottest'
New-Item -ItemType Directory -Force $Run | Out-Null

$serverOut = Join-Path $Run 'server.out.log'
$server = Start-Process -FilePath (Join-Path $Bin 'FalloutTogetherServer.exe') -WorkingDirectory $Run -PassThru -WindowStyle Hidden `
    -ArgumentList "--GameServer:uPort=$Port", "--GameServer:sPassword=$Password", '--bConsole=false' `
    -RedirectStandardOutput $serverOut -RedirectStandardError (Join-Path $Run 'server.err.log')
$failed = @()
try {
    Start-Sleep -Seconds 3
    if ($server.HasExited) { throw "Server beendet sich sofort (Exit $($server.ExitCode)), siehe $serverOut" }

    # Falsches Passwort muss abgewiesen werden
    $out = & (Join-Path $Bin 'FalloutBot.exe') "127.0.0.1:$Port" 'Eindringling' 5 'falsch'
    $out | Write-Host
    if ($LASTEXITCODE -ne 2 -or -not ($out -match 'wrong_password')) { $failed += 'falsches Passwort wurde nicht abgewiesen' }

    # Mehrere Bots gleichzeitig, jeder muss angenommen werden und die anderen sehen
    $jobs = 1..$Bots | ForEach-Object {
        $name = "Bot$_"
        Start-Sleep -Milliseconds 500
        Start-Process -FilePath (Join-Path $Bin 'FalloutBot.exe') -PassThru -NoNewWindow `
            -ArgumentList "127.0.0.1:$Port", $name, $Seconds, $Password `
            -RedirectStandardOutput (Join-Path $Run "$name.log")
    }
    $jobs | Wait-Process -Timeout ($Seconds + 10)
    foreach ($i in 1..$Bots) {
        $log = Get-Content (Join-Path $Run "Bot$i.log")
        $log | Write-Host
        if (-not ($log -match 'Anmeldung: accepted')) { $failed += "Bot$i nicht angenommen" }
        # Jeder Bot außer dem letzten muss mindestens die später gekommenen Bots sehen
        if ($i -lt $Bots -and -not ($log -match 'Mitspieler beigetreten')) { $failed += "Bot$i sieht keine Mitspieler" }
    }

    # Bau-Sync: Beobachter sieht alles live, der Nachzügler nur den zusammengefassten Stand
    $bot = Join-Path $Bin 'FalloutBot.exe'
    $watcher = Start-Process -FilePath $bot -PassThru -NoNewWindow -ArgumentList "127.0.0.1:$Port", 'Beobachter', 5, $Password `
        -RedirectStandardOutput (Join-Path $Run 'Beobachter.log')
    Start-Sleep -Milliseconds 500
    $builder = Start-Process -FilePath $bot -PassThru -NoNewWindow -ArgumentList "127.0.0.1:$Port", 'Baumeister', 2, $Password, 'bauen' `
        -RedirectStandardOutput (Join-Path $Run 'Baumeister.log')
    $builder, $watcher | Wait-Process -Timeout 15
    $late = & $bot "127.0.0.1:$Port" 'Nachzuegler' 2 $Password
    $late | Write-Host
    $live = Get-Content (Join-Path $Run 'Beobachter.log') | Where-Object { $_ -match 'Bauteil .* Replay 0' }
    if (@($live).Count -ne 5) { $failed += "Beobachter sah $(@($live).Count) statt 5 Bau-Nachrichten" }
    $replay = $late | Where-Object { $_ -match 'Bauteil ' }
    if (@($replay).Count -ne 3) { $failed += "Nachzügler bekam $(@($replay).Count) statt 3 Einträge" }
    if (-not ($replay -match 'Bauteil FF000001 Art 0 Replay 1 x 2 Ursprung 1')) { $failed += 'Verschobenes Objekt falsch nachgeholt' }
    if (-not ($replay -match 'Bauteil FF000002 Art 2 Replay 1 x 3 Ursprung 3')) { $failed += 'Verschrottetes Objekt falsch nachgeholt' }
    if (-not ($replay -match 'Bauteil 0 Art 1 Replay 1 x 4')) { $failed += 'Plugin-Objekt falsch nachgeholt' }
}
finally {
    if (-not $server.HasExited) { Stop-Process -Id $server.Id -Force }
}

if ($failed) {
    Write-Host "FEHLGESCHLAGEN:`n - $($failed -join "`n - ")" -ForegroundColor Red
    Write-Host "Serverlog: $serverOut"
    exit 1
}
Write-Host "Servertest bestanden ($Bots Bots)." -ForegroundColor Green
exit 0
