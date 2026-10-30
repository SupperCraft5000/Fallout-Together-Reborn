# Bauen (Windows x64)

## Voraussetzungen
- Visual Studio Build Tools 2022 (17.14, MSVC 14.44), Workload VCTools, Windows SDK 10.0.19041 und 10.0.22621
- xmake **2.9.5** (portables ZIP von GitHub reicht), Git, Python 3 (für Portierungswerkzeuge)

## Code holen
```powershell
git clone --recurse-submodules <Repo-Adresse> Fallout-Together-Reborn
```

Alle Umgebungsvariablen: [ENVIRONMENT.md](ENVIRONMENT.md).

## Wrapper `scripts/xm.ps1`
Alle xmake-Aufrufe darüber. `FTR_WORK` muss auf einen kurzen Arbeitsordner zeigen, `FTR_XMAKE` optional auf die `xmake.exe`.
Der Wrapper:
1. setzt `XMAKE_GLOBALDIR` auf einen **kurzen** Pfad (GameNetworkingSockets hat sehr lange Pfade → „Filename too long“),
2. aktiviert `core.longpaths` für Git nur für den Prozess,
3. pinnt das xmake-Paket-Repo auf `978015f8b308b0bc1561ab3df06de576c4a1fd5f` (2024-11-03). Mit neueren
   Paketlisten holt tiltedcore mimalloc 3.x → Linkerfehler `mi_malloc_size`,
4. entfernt `NoDefaultCurrentDirectoryInExePath` (sonst findet DirectXTK `CompileShaders.cmd` nicht, MSB8066),
5. wendet `patches/TiltedHooks-dinput.patch` auf das Submodul an.

```powershell
scripts\xm.ps1 config --arch=x64 --mode=releasedbg --yes --vs_sdkver=10.0.19041.0
scripts\xm.ps1 build -y FalloutImmersiveLauncher FalloutServerRunner FalloutBot
```
Ausgabe: `build\windows\x64\releasedbg\` → `FalloutTogether.exe`, `EarlyLoad.dll`, `FalloutTogetherServer.exe`,
`FTServer.dll`, `FalloutBot.exe`.

## Nötige Fixes außerhalb des Repos
| Problem | Fix |
|---|---|
| `MSB8020` Toolset `v144` nicht gefunden | xmake 2.9.5 `modules\private\utils\toolchain.lua`: `verinfo:lt("14.43")` → `lt("14.50")` (MSVC 14.44 → v143). Alternative: MSVC 14.38 installieren |
| LNK2019 `MaybeComputeUnknownFieldsSize`, `DescriptorMethods` (GNS) | im gepinnten xmake-repo `packages/g/gamenetworkingsockets/port/xmake.lua`: `add_requires("protobuf-cpp")` → `add_requires("protobuf-cpp 26.1", ...)`, dann GNS-Paket im Cache löschen |
| abseil `lts_20250127`-Symbole fehlen | `set_policy("package.precompiled", false)` (im Code enthalten) |

## Ziele
| xmake-Ziel | Datei | Inhalt |
|---|---|---|
| FalloutImmersiveLauncher | FalloutTogether.exe | Launcher + Client (eingebettet) |
| FalloutServerRunner | FalloutTogetherServer.exe | Server-Konsole, lädt FTServer.dll |
| FalloutTogetherServer | FTServer.dll | Spiellogik Server |
| FalloutBot | FalloutBot.exe | kopfloser Test-Client (`Code/bot`) |

## Laufzeitdateien
Der Build-Ordner allein startet nicht (`libcef.dll` fehlt). Nach dem Bauen:
```powershell
scripts\xm.ps1 build -y FalloutImmersiveLauncher TPProcess
scripts\deploy-bin.ps1
```
Das Skript kopiert CEF, Crashpad und die Mauszeiger neben `FalloutTogether.exe` (ersetzt `xmake install`).

## Einzel-exe
```powershell
scripts\xm.ps1 build -y FalloutImmersiveLauncher
scripts\xm.ps1 build -y FalloutTogetherServer
scripts\xm.ps1 build -y FalloutServerRunner
scripts\xm.ps1 build -y TPProcess
scripts\deploy-bin.ps1
package\build-package.ps1
```
Ergebnis: `dist\FalloutTogetherReborn.exe`. Die Build-ID daraus prüft auch den Versionsabgleich zwischen Host und Mitspieler.
