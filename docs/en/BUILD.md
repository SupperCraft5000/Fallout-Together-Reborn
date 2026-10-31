# Building (Windows x64)

## Requirements
- Visual Studio Build Tools 2022 (17.14, MSVC 14.44), workload VCTools, Windows SDK 10.0.19041 and 10.0.22621
- xmake **2.9.5** (the portable ZIP from GitHub is enough), Git, Python 3 (for the porting tools)

## Getting the code
```powershell
git clone --recurse-submodules <repository address> Fallout-Together-Reborn
```

All environment variables: [ENVIRONMENT.md](ENVIRONMENT.md).

## Wrapper `scripts/xm.ps1`
Run all xmake calls through it. `FTR_WORK` must point to a short working folder, `FTR_XMAKE` optionally to `xmake.exe`.
The wrapper:
1. sets `XMAKE_GLOBALDIR` to a **short** path (GameNetworkingSockets has very long paths, which gives "Filename too long"),
2. enables `core.longpaths` for Git for this process only,
3. pins the xmake package repository to `978015f8b308b0bc1561ab3df06de576c4a1fd5f` (2024-11-03). With newer
   package lists tiltedcore pulls in mimalloc 3.x, which gives the linker error `mi_malloc_size`,
4. removes `NoDefaultCurrentDirectoryInExePath` (otherwise DirectXTK does not find `CompileShaders.cmd`, MSB8066),
5. applies `patches/TiltedHooks-dinput.patch` to the submodule.

```powershell
scripts\xm.ps1 config --arch=x64 --mode=releasedbg --yes --vs_sdkver=10.0.19041.0
scripts\xm.ps1 build -y FalloutImmersiveLauncher FalloutServerRunner FalloutBot
```
Output in `build\windows\x64\releasedbg\`: `FalloutTogether.exe`, `EarlyLoad.dll`, `FalloutTogetherServer.exe`,
`FTServer.dll`, `FalloutBot.exe`.

## Required fixes outside the repository
| Problem | Fix |
|---|---|
| `MSB8020` toolset `v144` not found | xmake 2.9.5 `modules\private\utils\toolchain.lua`: `verinfo:lt("14.43")` → `lt("14.50")` (MSVC 14.44 → v143). Alternative: install MSVC 14.38 |
| LNK2019 `MaybeComputeUnknownFieldsSize`, `DescriptorMethods` (GNS) | in the pinned xmake repo `packages/g/gamenetworkingsockets/port/xmake.lua`: `add_requires("protobuf-cpp")` → `add_requires("protobuf-cpp 26.1", ...)`, then delete the GNS package from the cache |
| abseil `lts_20250127` symbols missing | `set_policy("package.precompiled", false)` (included in the code) |

## Targets
| xmake target | File | Content |
|---|---|---|
| FalloutImmersiveLauncher | FalloutTogether.exe | launcher + client (embedded) |
| FalloutServerRunner | FalloutTogetherServer.exe | server console, loads FTServer.dll |
| FalloutTogetherServer | FTServer.dll | server game logic |
| FalloutBot | FalloutBot.exe | headless test client (`Code/bot`) |

## Runtime files
The build folder alone does not start (`libcef.dll` is missing). After building:
```powershell
scripts\xm.ps1 build -y FalloutImmersiveLauncher TPProcess
scripts\deploy-bin.ps1
```
The script copies CEF, Crashpad and the mouse cursors next to `FalloutTogether.exe` (replaces `xmake install`).

## Single exe
```powershell
scripts\xm.ps1 build -y FalloutImmersiveLauncher
scripts\xm.ps1 build -y FalloutTogetherServer
scripts\xm.ps1 build -y FalloutServerRunner
scripts\xm.ps1 build -y TPProcess
scripts\deploy-bin.ps1
package\build-package.ps1
```
Result: `dist\FalloutTogetherReborn.exe`. Its build ID is also what the version check between host and player compares.
