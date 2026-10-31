# AGENTS.md: starting point for AI agents and new contributors

Read [STATUS.md](STATUS.md) first (what works, what is open), then, depending on the task,
[ARCHITECTURE.md](ARCHITECTURE.md), [DEBUGGING.md](DEBUGGING.md), [PORTING.md](PORTING.md) or [BUILD.md](BUILD.md).

## Project in brief
- Fallout Together Reborn: co-op mod for Fallout 4 based on Fallout Together (TiltedEvolution, C++20, xmake, Windows x64).
- Architecture: `FalloutTogether.exe` (Immersive Launcher) loads `Fallout4.exe` into its own process,
  installs hooks and starts the game. The client code (`Code/client`) is embedded through `/WHOLEARCHIVE`.
  Server: `FalloutTogetherServer.exe` + `FTServer.dll`, networking through
  GameNetworkingSockets (TiltedConnect), messages in `Code/encoding`.
- Game addresses: every engine function or variable in use has an **Old-Gen Address Library ID** (1.10.163)
  in the code (`POINTER_FALLOUT4(Type, name, ID)` or `VersionDbPtr<T> x(ID)`). For other versions
  `VersionDb` resolves the IDs through `Data\F4SE\Plugins\FalloutTogether\offsets-<version>.bin`.

## Hard rules
1. **Never change the original game installation.** Tests run in a test copy (hard links for large data).
   **Never write to** hard links, only delete and replace them (otherwise the original changes too).
2. Do not commit game files, exe files, files derived from them, Address Library `.bin` files or third-party binaries.
3. IDs in the code are **real** Old-Gen IDs (since patch 0002). Always enter new addresses as 1.10.163 IDs;
   the port generates the offsets per version.
4. Check every new address matching heuristic with `Tools/port/validate.py` (cross-validation).
   Wrong addresses are worse than missing ones: discard rather than guess.
5. Skyrim targets and the Skyrim client are removed. The `#if TP_SKYRIM` blocks in shared code are not compiled and are legacy. `Code/encoding/Structs/Skyrim` only contains four headers with animation identifiers that the Fallout code includes.

## Pitfalls (they cost hours otherwise)
- Use xmake **2.9.5** (like the project's CI) and pin the package repository to `978015f8b`; newer packages break
  the link (mimalloc 3.x, abseil/protobuf). Details in [BUILD.md](BUILD.md).
- `set_policy("package.precompiled", false)` is required, otherwise incompatible precompiled packages arrive.
- xmake 2.9.5 detects MSVC 14.44 as toolset `v144`, which needs a patch in `toolchain.lua` ([BUILD.md](BUILD.md)).
- The Old-Gen `Fallout4.exe` (1.10.163) cannot be analysed statically as is, the Next-Gen exe can.
- CommonLibF4-NG IDs for **VTables** are partly wrong (several classes on the same NG ID).
  Use RTTI for VTables, and take CommonLibF4 as truth only for functions.
- Tools in sandboxes or containers: GUI processes (launcher, game) can hang invisibly there.
  Start game tests from a normal user shell.

## Typical tasks
| Task | Where to start |
|---|---|
| Support a new Fallout version | [PORTING.md](PORTING.md), then `Tools/port/pipeline.py` |
| Find open addresses | `data/open-1-11-240-0.csv`, [PORTING.md](PORTING.md) "Open addresses" |
| Network or server bug | `Code/server`, `Code/encoding`, test with `scripts/test-server-bots.ps1` |
| Game-side bug | `Code/client/Games/Fallout4`, `Code/client/Services` |
| New message in the protocol | `Code/encoding/Messages`, opcodes in `Code/encoding/Opcodes.h`, extend the bot (`Code/bot`) |

## Conventions
- Code comments in TiltedEvolution code in English (as upstream), documentation and scripts in German. `docs/en` mirrors the German documentation, so update both.
- Development on `dev`, `main` is the tested state. Gitea and GitHub stay identical, see [REPOSITORIES.md](REPOSITORIES.md).
- Commits with a meaningful message. `patches/` only contains the patch for the TiltedHooks submodule.
