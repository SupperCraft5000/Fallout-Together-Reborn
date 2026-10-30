# Wrapper: xmake 2.9.5 mit kurzem Cache-Pfad und Paketliste vom 2024-11-03, siehe docs/BUILD.md
# FTR_WORK: kurzer Arbeitsordner, FTR_XMAKE: Pfad zur xmake.exe (Standard: xmake aus dem PATH)
if (-not $env:FTR_WORK) { throw 'Bitte FTR_WORK auf einen kurzen Arbeitsordner setzen, zum Beispiel D:\ftr' }
$Root = Split-Path $PSScriptRoot
$env:XMAKE_GLOBALDIR = "$env:FTR_WORK\xg"
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'core.longpaths'
$env:GIT_CONFIG_VALUE_0 = 'true'
# DirectXTK ruft "CompileShaders.cmd" ohne Pfad auf; das braucht die Suche im aktuellen Ordner
Remove-Item Env:NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue
$xmake = if ($env:FTR_XMAKE) { $env:FTR_XMAKE } else { 'xmake' }
$repo = "$env:FTR_WORK\xg\.xmake\repositories\xmake-repo"
$pin = '978015f8b308b0bc1561ab3df06de576c4a1fd5f'

if (-not (Test-Path $repo)) { & $xmake repo -u | Out-Null }
if ((git -C $repo rev-parse HEAD) -ne $pin) {
    git -C $repo fetch -q --unshallow 2>$null
    git -C $repo -c advice.detachedHead=false checkout -q $pin
}

# Das Submodul TiltedHooks bleibt auf Upstream, unsere Korrektur liegt als Patch daneben
$hooks = Join-Path $Root 'Libraries\TiltedHooks'
if (-not (Select-String -Quiet -SimpleMatch 'does not import dinput8' (Join-Path $hooks 'Code\hooks\src\DInputHook.cpp'))) {
    git -C $hooks apply (Join-Path $Root 'patches\TiltedHooks-dinput.patch')
    if ($LASTEXITCODE) { throw 'Patch für TiltedHooks fehlgeschlagen' }
}

Set-Location $Root
& $xmake @args
exit $LASTEXITCODE
