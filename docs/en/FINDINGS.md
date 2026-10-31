# Discoveries and pitfalls

References like `patches/0014` are commits, see [HISTORY.md](HISTORY.md).

## 1. Off-by-one when reading the Address Library (Fallout Together, upstream)
`VersionDb::LoadFallout4` read `version-1-10-163-0.bin` from byte 0 as pairs `(address, ID)` and ignored the
last 8 bytes ("strangely has no ID"). The actual format is `u64 count`, then `(u64 ID, u64 address)`.
Result: every ID got the address of the **previous** entry. The developers chose IDs to match,
so it worked, but all IDs in the code were "real ID + 1" (e.g. 633525 instead of 633524 =
`Main::OnIdle`). That is why no ID matched community mappings (CommonLibF4, VR database).
Fix: patch 0002 (all 159 places, loader corrected). Demonstrably identical addresses for 1.10.163.

## 2. Next-Gen is recompiled
Byte signatures from the function start find only 20 of 153 addresses from 1.10.163 to 1.11.240. Call graph +
string anchors + RTTI work much better (80 of the 133 needed).

## 3. Analysing the Old-Gen exe
The Steam exe of 1.10.163 cannot be read directly for static analysis, the Next-Gen exe (1.11.x) can. How to solve that locally is not part of this project. Files derived from the exe never belong in the repository.

## 4. CommonLibF4-NG VTable IDs partly wrong
Several classes share the same NG ID (e.g. four `AutoRegister…` VTables → `2731340`), and RTTI shows a
different class than expected. For functions the pairs are reliable.

## 5. Build environment
See [BUILD.md](BUILD.md) (xmake 2.9.5, package pin, precompiled off, MSVC 14.44 toolset, long paths, DirectXTK).

## 6. Launcher
- Finds the installation through `HKLM\Software\Wow6432Node\Bethesda Softworks\Fallout4` or an HKCU TiltedPhoques key;
  `--exePath` overrides this and stores it in HKCU.
- Loads `Fallout4.exe` into a reserved area (`exeLoadSz` 0x70000000; 1.11.240 needs 0x425A000).
- Loads F4SE itself (`f4se_<version>.dll` from the game folder) if present.
- In container or sandbox environments (e.g. app containers) it hangs before the first window (wait state LpcReply);
  even a simple MessageBox does not appear there. Start it normally from the user shell.

## 7. Game profile
INIs and saves (`Documents\My Games\Fallout4`) and `plugins.txt` (`AppData\Local\Fallout4`) are shared by all
installations. Swap the profile for tests (`scripts/test-profile.ps1`).

## 8. Downgrade data
Depot manifests for 1.10.163 from zerratar/fallout4-downgrader (`DepotManager.cs`). Next-Gen-only
Creation Club files and NG F4SE plugins must be set aside in the test copy.
Also disable `xSE Plugin Preloader` (`winmm.dll`).

## 9. Fallout 4 does not import dinput8.dll
`DInputHook::Install` (submodule TiltedHooks) copies the start of `DirectInput8Create`. Fallout 4 does not import
the DLL (neither 1.10.163 nor 1.11.240), so the pointer is null: crash when the client starts.
Fix: `patches/0003` (apply in the `Libraries/TiltedHooks` folder: `git apply -p3 <patch>`).
Resolving crash addresses: `Tools/port/addr2sym.py`.
The base offset is `address - 0x140000000`, addresses from `0x1B0000000` belong to the launcher/client,
below that to the game. WER dumps are in `%LOCALAPPDATA%\CrashDumps`.

## 10. Six IDs were missed in the conversion
`inventory.py` did not recognise `DECLARE_DISPATCHER(Name, id)` and `internal::DynamicCast(id)`. These six IDs stayed
"real ID + 1" and pointed to wrong entries with the corrected loader. Result: crash shortly after start with
RIP in `.pdata` (0x146A31CA8 = ID 823571). Fixed in `patches/0004`, `data/ids-1.10.163.csv` is updated.
The offsets for 1.11.240 do not contain these six IDs yet: 139 are now required instead of 133.
`inventory.py` expects the old IDs, so do not run it again on the corrected code.

## 11. 3,235 RTTI IDs were affected too
`registerRtti_*` in `Games/Fallout4/RTTI.cpp` also use Address Library IDs. With the old IDs `Cast<T>` returned
null, crash in `CharacterService::OnActorAdded` (`pActor->GetExtension()`). Fixed in `patches/0005`; all
3,235 entries were checked against the type name in the TypeDescriptor of the 1.10.163 exe.
Consequence for 1.11.240: these IDs are still missing from the offsets. They can be mapped through the type name (`.?AV…@@`),
but must go into the pipeline. The client otherwise reports them as missing.

## 12. Ghosting came from the test cell QASmoke
In `QASmoke` the image smeared heavily, at Red Rocket (`coc RedRocketExt`) it did not. TAA, motion blur and the overlay
were not the cause, tests on them changed nothing. So the test profile starts at Red Rocket.
`iPresentInterval=3` gave a black screen (cause not investigated), leave the value at 1.
Diagnostics: `test-ingame.ps1 -NoOverlay` sets `TP_NO_OVERLAY_RENDER`, `-Hold` keeps the game open.
For screenshots: the window is called "Fallout Together", computer use does not find it as an app.

## 13. Quests only sync in a party, and there was no party
`QuestService` only sends as party leader, the server only distributes to the party. The party interface is missing in the Fallout build
(no `UI` folder), and `bAutoPartyJoin` only applies when a party already exists. So every player stayed alone.
`patches/0007`: the first player automatically gets a party and is leader, others join.
Only the leader's progress is transferred. Confirmed in the bot test (messages 11 and 9), not yet tested in the game.

Two-player test (2026-10-30, -Clients 2): both connected and visible to each other. Both clients write to the same
`tp_client.log`. Still open in the log: `MoveActor: failed to fetch cell` (22x), `Character with remote id 6 is already spawned`,
`Failed to retrieve Actor 60BFE`.

## 14. "Miscellaneous" quests were filtered
`QuestService::IsNonSyncableQuest` blocked all quests of type Miscellaneous. In Fallout 4 these include the entries under
"Miscellaneous" in the Pip-Boy. `patches/0008`: Miscellaneous is now only blocked without objectives. Internal quests such as 11679D
otherwise fire every second. The client log now shows type and objectives, the server log the forwarding. Not tested in the game.

## 15. The launcher damaged the Next-Gen exe
The Immersive Launcher treated the 1.11.240 exe like the old version and destroyed `.text` in the process, crash right behind the original entry point.
Fixed in `patches/0011`. In addition `VectoredExceptionHandler` ran into endless recursion (mutex exception on re-entry) until the
stack was full, which is why there was no dump. Also fixed.

Addendum to 15: the first fix had no effect, because the field `SteamAppId` in the header struct had vanished into a comment. `Flags` therefore read the app ID.
Also, the client never started on Next-Gen: the start hook sat on `_ismbblead` from MSVCR110, which the 1.11.240 exe does not import. Now `GetStartupInfoW` as in Skyrim (`patches/0012`).

Second addendum to 15: `patches/0012` did not work either. The switch checked whether `msvcr110.dll` is loaded, and `GFSDK_GodraysLib.x64.dll` loads it under 1.11.240 too.
Result: no hook applied, `BeginMain` never ran, and the first window callback in `InputService::WndProc` crashed on the null pointer `s_pOverlay`. The switch now goes by the exe's imports (`patches/0013`).

## 16. Structure offset: client did not run on Next-Gen, crash on exit
The flag `VMContext::inactive` is at 0x610 instead of 0x600 in 1.11.240 (decompiled `cVMUpdate` of both versions). With the old offset `HookVMUpdate` never called `Update()`: the game ran, but without a client, and there was no "Auto connect". Evidence in the dump: `bAlwaysActive` was 0 and `bUseFaceGenPreprocessedHeads` 1, although `Update()` sets both every frame.
The crash on exit (`0x4CB273`, null pointer through the global variable `0x30EB310`) is in the engine shutdown `Main::Shutdown`, third call in WinMain (OG ID 234348, 1.11.240 `0xC31700`). The freeze on 1.10.163 (`0xCFFFFFFF`) probably belongs to it, but that is not proven. `patches/0014` now ends the process right before this shutdown. Not tested in the game, on either version.

## 17. Respawn loop after a player's death
`PlayerService::RunRespawnUpdates` called `RespawnPlayer` every 5 seconds, because the player stayed in the essential-down state after health was refilled (`actorState`, bits 17 to 20, value 7 or 8). Each pass teleported the player and showed the loading screen. On the other client this deleted the character and recreated it, hence `Temporary Remote Deleted` and `Actor to remove not found in form ids map` in the log.
`patches/0015` resets the life state and allows only one respawn per death. Not checked: whether the player then gets up from the prone pose, and whether the bug also exists on 1.10.163. The code there was already marked as unchecked (`TODO: ft (verify)`).

Addendum to 17: in the game test on 1.11.240 with two clients the respawn worked according to the user. The log shows three single deletions 26 and 50 seconds apart instead of a loop, no crash and a clean exit of both clients.

## 18. The crash handler sent reports to the Tilted Phoques Sentry project
`InstallCrashHandler` set hard-coded DSNs of the original project. Sentry and Crashpad can upload crash reports including minidumps there. Whether reports from our tests arrived is not checked. `patches/0016` removes the DSNs, reports stay local.

## 19. `realloc` was missing from the heap hooks
Crash when switching to the main menu, on a Steam thread in `ntdll`. The return address on the stack `ucrtbase+0x318B5` is in `_realloc_base`, `rbx` pointed into the mimalloc area (`0x2001403...`). 1.11.240 imports `realloc` from the UCRT, but the client only hooked `malloc`, `calloc`, `free`, `_msize` and the aligned variants. So the Windows heap received a mimalloc pointer. `patches/0018` adds the hook. That the crash is gone is not yet confirmed in the game.

Addendum to 19: hooking only `realloc` was not enough. The game also frees and grows blocks that come from unhooked CRT functions. `mi_realloc` cannot handle that (`mi_free: pointer does not point to a valid heap space`, then a crash in mimalloc). `patches/0019`: `free`, `realloc` and `_msize` check with `mi_is_in_heap_region` and hand foreign blocks back to the CRT.

## 20. Life state and essential on real saves
Bit 18 of `Actor+0x43C` is recalculated by `0xD71E60` (OG, ID 1085880) / `0xC5B2B0` (NG). A value on the reference takes precedence, then the base form, then aliases. `HookUpdateEssential` sets it again for the player afterwards.
The life state (`actorState`, bits 17 to 20) must not be written directly. `Actor::SetLifeState` (OG `0xD98580`, ID 1300465, NG `0xC83990`) adds, when changing to 7/8 for the player, an `ExtraInputEnableLayer` with mask `0x424` and a camera state and runs the bleedout action; the way back removes these. The player override is in slot 0x105 (OG `0xE9C0C0`, NG `0xD57980`). Both versions are the same here in the decompiled code.
`Resurrect` on the player (NG `0xD5E2E0`) deletes `currentProcess` and creates it anew, which makes it unsuitable for respawning.

## 21. The server reported to the upstream server list
`ServerListService` sent name, description, port, player count, tick rate and version to `fallout-reborn-list.skyrim-together.com` every minute and on every join, even with `bAnnounceServer=false`. The recipient sees the public IP this way. A `403` response ended the server (`GameServer::Kill`). The service is removed. Whether the address was reachable is not checked.

## 22. Workshop build events
The sinks that trigger script events such as `OnWorkshopObjectPlaced` are virtual `ProcessEvent` functions without direct callers. Signature `(this, event*)`, the event holds two `TESObjectREFR*`.

| Event | OG ID | OG RVA | NG RVA |
|---|---|---|---|
| Destroyed | 313584 | `0x145D920` | `0x1195F90` |
| Moved | 725213 | `0x145DB70` | `0x1196200` |
| Placed | 697786 | `0x145DDB0` | `0x1196470` |
| Repaired | 670087 | `0x145DFF0` | `0x11966E0` |
| Grabbed | 642544 | `0x145E230` | `0x1196950` |

It is proven that each sink sends the script event to both references. That `[0]` is the workbench and `[1]` the object is an assumption. `WorkshopService` swaps the two if only `[0]` is temporary. `WorkshopReferenceEventSink` is the wrong place to start, the stat handlers do not evaluate the event data.

Papyrus natives on NG: `PlaceAtMe` `0x1159E80` returns `TESObjectREFR*` and releases its own reference, `abDeleteWhenAble` must be `false`, otherwise the object vanishes. `SetPosition` `0x115DED0` and `SetAngle` `0x115CEE0` take `RefrOrInventoryObj` and three floats, `SetAngle` in degrees. `SetLinkedRef` `0x11815D0` takes `(self, target, keyword)` as plain pointers. `WorkshopItemKeyword` is `0x54BA6`.

`Game.FindClosestReferenceOfType` (OG ID 1459241, OG `0x13C0490`, NG `0x1121840`) has the form `(vm, stack, nullptr, form, x, y, z, radius)` and searches the loaded cells. `PapyrusService::Get("Game", ...)` does not find it in the game, hence the direct pointer. The class name probably comes from the string pool in a different spelling, which is not proven.

## 23. Wires and the power grid
A wire is a reference with a BNDS base (form type 35), `ExtraPowerLinks` (type `0xD0`, two ends) and `ExtraBendableSplineParams` (type `0xB4`). Sources were the F4Multiplayer docs, the FathersFallout code and CommonLibF4, the addresses are matched in the listings of both game versions.

| Function | OG ID | OG RVA | NG RVA |
|---|---|---|---|
| `SplineUtils::ConnectSpline` | 59311 | `0x200E50` | `0x38DFE0` |
| `SplineUtils::DisconnectSpline` | 750682 | `0x200F50` | `0x38E0E0` |
| `TaskQueueInterface::QueueRebuildBendableSpline` | 198419 | `0xD5F120` | `0xC4C090` |
| `ExtraDataList::SetBendableSplineInfo` | 894306 | `0x9A800` | `0x2915A0` |
| `Workshop::IsPowered` | 1546240 | `0x1F2F60` | `0x382530` |

`ConnectSpline` has no direct callers in the game and only writes `ExtraPowerLinks` on both ends and the wire. It does not touch the power grid. The grid lives in `Workshop::ExtraData` (type `0xBF`) on the workbench. A workbench nobody worked at does not have this extra, it was missing in the Red Rocket test.

The game's way, as seen at `0x39982C`, confirmed in the test:

| Step | OG ID | OG RVA | NG RVA |
|---|---|---|---|
| Make power data: `0x58` bytes of memory | 898179 | `0x30D0` | `0x1E0320` |
| Make power data: constructor | 1290994 | `0xA9190` | `0x29C7E0` |
| Make power data: add to the extra list | 787248 | `0x42FA0` | `0x29F910` |
| Add connection `(data, a, b, wire)` | 523652 | `0x1F6E20` | `0x3876D0` |
| Update end `(end, data)`, for both | 958345 | `0x201B10` | `0x38ECB0` |
| Recalculate the workbench | 63358 | `0x1F6170` | `0x3867B0` |
| Recalculate the workbench, second part | 730242 | `0x1F5D40` | `0x386240` |
| Remove connection `(data, a, b, wire, true)` | 800489 | `0x1F7440` | `0x387BD0` |
| Find an object's grid `(data, object)` | 1529707 | `0x1F7950` | `0x3881D0` |
| Take an object off the grid `(data, object)` | 1456203 | `0x1F69C0` | `0x3871C0` |

Without this extra `IsPowered` is always false. The OG mappings come from matching call sites and are not tested on 1.10.163. The game reads the task queue from the address of ID 7491, the client calls the same ID `WeatherManager`, that mapping looks wrong.

`PapyrusService` did not find `Game.FindClosestReferenceOfType` and `GetLinkedRef` in the game, while other names worked. The keys now ignore case, and `GetLinkedRef` is found that way. This supports the string pool assumption from section 22 without measuring it directly.
