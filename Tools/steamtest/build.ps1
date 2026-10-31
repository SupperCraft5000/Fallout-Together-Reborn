# Baut dist\SteamTest.exe, braucht nur das .NET Framework 4
$ErrorActionPreference = 'Stop'
$Root = Split-Path (Split-Path $PSScriptRoot)
$Dist = Join-Path $Root 'dist'
New-Item -ItemType Directory -Force $Dist | Out-Null
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /platform:x64 /optimize+ "/out:$Dist\SteamTest.exe" (Join-Path $PSScriptRoot 'SteamTest.cs')
if ($LASTEXITCODE) { throw 'csc fehlgeschlagen' }
Write-Host "$Dist\SteamTest.exe"
