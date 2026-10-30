# Legt die Next-Gen-Testkopie $env:FTR_WORK\Fallout4-B an. Das Original wird nur gelesen.
# Große Spieldaten sind Hardlinks, darum nie in diese Dateien schreiben. Wiederholen ist gefahrlos.
$ErrorActionPreference = 'Stop'

$Original = "$env:FTR_GAME_DIR"
$Game     = "$env:FTR_WORK\Fallout4-B"
$Version  = '1-11-240-0'
$Port     = Join-Path (Split-Path $PSScriptRoot) 'data'
$Link     = '.ba2', '.esm', '.esl', '.bk2', '.bik'
# Fremde Mods der echten Installation stören den Test
$Skip     = '^(winmm[.]dll|f4se_.*|Data.F4SE.*|Data.*[.]esp)$'

Get-ChildItem $Original -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($Original.Length + 1)
    if ($rel -match $Skip) { return }
    $dst = Join-Path $Game $rel
    if (Test-Path -LiteralPath $dst) { return }
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    if ($Link -contains $_.Extension.ToLower()) {
        New-Item -ItemType HardLink -Path $dst -Target $_.FullName | Out-Null
    } else {
        Copy-Item -LiteralPath $_.FullName -Destination $dst
    }
}

$plugins = Join-Path $Game 'Data\F4SE\Plugins\FalloutTogether'
New-Item -ItemType Directory -Force $plugins | Out-Null
python -I (Join-Path $Port 'make_offsets.py') --ids (Join-Path $Port 'ids-1.10.163.csv') --manual (Join-Path $Port "final-$Version.csv") --out (Join-Path $plugins "offsets-$Version.bin")
Write-Host ('Testkopie: ' + (Get-Item "$Game\Fallout4.exe").VersionInfo.FileVersion)
