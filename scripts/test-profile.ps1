# Eigenes Spielprofil für die Testkopie.
# Fallout 4 liest INIs und Spielstände immer aus "Dokumente\My Games\Fallout4" und die Plugin-Liste aus
# "AppData\Local\Fallout4". Beides teilen sich Original und Testkopie. Dieses Skript tauscht die Ordner
# per Umbenennen aus, deine echten Spielstände bleiben dabei unberührt liegen.
#
#   .\test-profile.ps1 -Enter    # echtes Profil beiseite, Testprofil aktiv
#   .\test-profile.ps1 -Enter -MainMenu    # ebenso, aber Start im Hauptmenü statt direkt in Red Rocket
#   .\test-profile.ps1 -Leave    # Testprofil zurück nach $env:FTR_WORK\profile, echtes Profil wieder aktiv
#   .\test-profile.ps1           # zeigt den Zustand
[CmdletBinding(DefaultParameterSetName = 'Status')]
param(
    [Parameter(ParameterSetName = 'Enter')][switch]$Enter,
    [Parameter(ParameterSetName = 'Enter')][switch]$MainMenu,
    [Parameter(ParameterSetName = 'Leave')][switch]$Leave
)
$ErrorActionPreference = 'Stop'

$Docs    = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Fallout4'
$Local   = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Fallout4'
$Store   = "$env:FTR_WORK\profile"
$Marker  = Join-Path $Store 'ACTIVE'
$Suffix  = '.echt'

function Assert-GameStopped {
    if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { throw 'Fallout 4 läuft, bitte erst beenden.' }
}

function New-TestProfile {
    New-Item -ItemType Directory -Force "$Store\Documents\Saves", "$Store\Local" | Out-Null
    # Fenstermodus, damit zwei Instanzen nebeneinander laufen können
    $prefs = "$Store\Documents\Fallout4Prefs.ini"
    if (-not (Test-Path $prefs)) {
        @'
[Display]
bFull Screen=0
bBorderless=0
iSize W=1280
iSize H=720
sAntiAliasing=FXAA
bMBEnable=0
bTopMostWindow=0
bMaximizeWindow=0
[General]
bAlwaysActive=1
[Launcher]
bEnableFileSelection=1
'@ | Set-Content -Encoding ascii $prefs
    }
    $custom = "$Store\Documents\Fallout4Custom.ini"
    if (-not (Test-Path $custom)) {
        @'
[General]
SIntroSequence=
bSkipSplash=1
sStartingConsoleCommand=coc RedRocketExt
[Archive]
bInvalidateOlderFiles=1
sResourceDataDirsFinal=
'@ | Set-Content -Encoding ascii $custom
    }
    # Nur offizielle Inhalte, keine Mods
    $plugins = "$Store\Local\plugins.txt"
    if (-not (Test-Path $plugins)) {
        @('# Testprofil Fallout Together', '*DLCRobot.esm', '*DLCworkshop01.esm', '*DLCCoast.esm',
          '*DLCworkshop02.esm', '*DLCworkshop03.esm', '*DLCNukaWorld.esm', '*DLCUltraHighResolution.esm') |
            Set-Content -Encoding ascii $plugins
    }
}

# Move-Item kann Ordner nicht zwischen Laufwerken verschieben (E: <-> C:)
function Move-Dir([string]$From, [string]$To) {
    Copy-Item -LiteralPath $From -Destination $To -Recurse
    Remove-Item -LiteralPath $From -Recurse -Force
}

function Swap-In([string]$Live, [string]$Test) {
    if (Test-Path -LiteralPath $Live) {
        if (Test-Path -LiteralPath "$Live$Suffix") { throw "$Live$Suffix existiert schon, bitte prüfen." }
        Rename-Item -LiteralPath $Live -NewName ((Split-Path $Live -Leaf) + $Suffix)
    }
    Move-Dir $Test $Live
}

function Swap-Out([string]$Live, [string]$Test) {
    if (Test-Path -LiteralPath $Live) { Move-Dir $Live $Test }
    if (Test-Path -LiteralPath "$Live$Suffix") { Rename-Item -LiteralPath "$Live$Suffix" -NewName (Split-Path $Live -Leaf) }
}

if ($Enter) {
    Assert-GameStopped
    if (Test-Path $Marker) { Write-Host 'Testprofil ist schon aktiv.'; return }
    New-TestProfile
    $customIni = "$Store\Documents\Fallout4Custom.ini"
    $start = if ($MainMenu) { '' } else { 'coc RedRocketExt' }
    (Get-Content $customIni) -replace '^sStartingConsoleCommand=.*', "sStartingConsoleCommand=$start" | Set-Content -Encoding ascii $customIni
    Swap-In $Docs  "$Store\Documents"
    Swap-In $Local "$Store\Local"
    Set-Content $Marker (Get-Date -Format o)
    Write-Host 'Testprofil aktiv. Danach unbedingt: .\test-profile.ps1 -Leave'
}
elseif ($Leave) {
    Assert-GameStopped
    if (-not (Test-Path $Marker)) { Write-Host 'Testprofil ist nicht aktiv.'; return }
    Swap-Out $Docs  "$Store\Documents"
    Swap-Out $Local "$Store\Local"
    Remove-Item $Marker
    Write-Host 'Echtes Profil wieder aktiv.'
}
else {
    if (Test-Path $Marker) { Write-Host "Testprofil AKTIV seit $(Get-Content $Marker)" } else { Write-Host 'Echtes Profil aktiv.' }
    foreach ($p in $Docs, "$Docs$Suffix", $Local, "$Local$Suffix") { Write-Host ("{0,-6} {1}" -f $(if (Test-Path $p) { 'da' } else { '-' }), $p) }
}
