# Baut die Einzel-exe für Mitspieler: dist\FalloutTogetherReborn.exe
# Vorher bauen: scripts\xm.ps1 build -y FalloutImmersiveLauncher (ebenso FalloutServerRunner, FalloutTogetherServer, TPProcess)
# und einmal scripts\deploy-bin.ps1. Enthält keine Spieldateien.
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot
$Bin  = Join-Path $Root 'build\windows\x64\releasedbg'
$Icon = Join-Path $Root 'Code\immersive_launcher\resources\ft.ico'
$Dist = Join-Path $Root 'dist'
$Work = Join-Path $Dist 'work'
$Csc  = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"

Remove-Item $Work -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $Work | Out-Null

$files = 'FalloutTogether.exe', 'EarlyLoad.dll', 'TPProcess.exe', 'FalloutTogetherServer.exe', 'FTServer.dll', 'crashpad_handler.exe',
    'libcef.dll', 'chrome_elf.dll', 'd3dcompiler_47.dll', 'libEGL.dll', 'libGLESv2.dll', 'vk_swiftshader.dll', 'vulkan-1.dll',
    'vk_swiftshader_icd.json', 'chrome_100_percent.pak', 'chrome_200_percent.pak', 'resources.pak', 'icudtl.dat',
    'snapshot_blob.bin', 'v8_context_snapshot.bin', 'locales', 'assets', 'swiftshader'
$list = Join-Path $Work 'files.txt'
$files | Where-Object { Test-Path (Join-Path $Bin $_) } | Set-Content -Encoding utf8 $list

# Adressdateien aller unterstützten Versionen
$offsets = Get-ChildItem (Join-Path $Root 'data\final-*.csv')
foreach ($csv in $offsets) {
    $version = $csv.BaseName -replace '^final-', ''
    python -I (Join-Path $Root 'Tools\port\make_offsets.py') --ids (Join-Path $Root 'data\ids-1.10.163.csv') --manual $csv.FullName --out (Join-Path $Work "offsets-$version.bin")
    if ($LASTEXITCODE) { throw 'make_offsets fehlgeschlagen' }
}

$zip = Join-Path $Work 'payload.zip'
python -I (Join-Path $PSScriptRoot 'make-payload.py') $Bin $list $Work $zip
if ($LASTEXITCODE) { throw 'make-payload fehlgeschlagen' }

$build = (git -C $Root rev-parse --short HEAD) + '-' + (Get-FileHash $zip -Algorithm SHA256).Hash.Substring(0, 8)
$buildFile = Join-Path $Work 'build.txt'
Set-Content -Encoding ascii $buildFile $build

$exe = Join-Path $Dist 'FalloutTogetherReborn.exe'
& $Csc /nologo /target:winexe /optimize+ "/out:$exe" "/win32icon:$Icon" `
    /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll `
    "/resource:$zip,payload.zip" "/resource:$buildFile,build.txt" (Join-Path $PSScriptRoot 'Starter.cs')
if ($LASTEXITCODE) { throw 'csc fehlgeschlagen' }

Remove-Item $Work -Recurse -Force
Write-Host ("{0}: {1:N0} MB, Build {2}" -f $exe, ((Get-Item $exe).Length / 1MB), $build)
