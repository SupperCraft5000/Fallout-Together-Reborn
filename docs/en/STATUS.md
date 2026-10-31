# Status (2026-10-31)

Playable over two PCs on Fallout 4 1.11.240 with real saves. Patch numbers below refer to the commit history, commits carry the same order, see [HISTORY.md](HISTORY.md).

## Works (verified)
| What | Evidence |
|---|---|
| Build of all Fallout targets (launcher, client, server, runner, bot) | xmake releasedbg, MSVC 14.44 |
| Server starts, listens (default port 10578) | smoke test |
| `FalloutBot`: headless client signs in | `scripts/test-server-bots.ps1`: wrong password refused, 2 and 3 bots accepted and see each other |
| Real Address Library IDs, same behaviour on 1.10.163 | all 159 uses recalculated: identical addresses |
| Address port 1.10.163 → 1.11.240 | 138 of 139 code IDs (Ghidra, see `data/ghidra-report-1-11-240-0.md`), `ctor` rebuilt in the client, RTTI complete except 2 removed types and 3,233 of 3,235 RTTI IDs; two independent paths agree on all 82 shared ones |
| Precision of the port | cross-validation against 1.11.191 (Address Library + CommonLibF4): functions 0 errors at gap size 4 |

## Milestone 2026-10-30: first game test passed (1.10.163)
`scripts/test-ingame.ps1`: the client starts in the real game with F4SE 0.6.23, connects by auto-connect, the
server creates the cell's objects, the bot sees the player as another player. The script then ends the game.
Open from this run: empty player name, log messages `activated object has no parent` and
`Form id not found for magic add target`, game with two real clients, longer session (`-Hold`).

## Milestone 2026-10-30: first game test on 1.11.240 (one client)
The client starts, reaches Red Rocket, connects to the server and sends quests and actors. Exiting ran cleanly (log "skipping engine teardown"), without a crash and without a dump.
The log had no errors limited to 1.11.240 apart from seven warnings "neither a local or remote component" (actor without a server ID yet, it arrived a few milliseconds later; rated as a timing window, not checked).

## Milestone 2026-10-30: two clients on 1.11.240
Two instances on one PC, both connected. According to the game test it runs, remote characters and quest updates arrive in the log, no crash. With two clients each instance loads for about 80 seconds, and the window appears frozen during that time.
New in the log: `MagicService::OnNotifyAddTarget: Failed to retrieve effect by id` (twice), not investigated. The test copy contains Creation Club `.esl` files, the clients report 19 instead of 8 mods.
Not checked: combat, saves, two PCs over a network, longer session.

## Status 2026-10-30 evening: single exe and real save
`FalloutTogetherReborn.exe` ran with the built-in server and a real save (level 57): loading, connecting, no crash. Only one client, on the test copy.
**Open:** The game ended at 17:04:09 without the player doing anything according to the player, 52 s after connecting and 6 s after a location change (`HC_ChangeLocationRainingHandler`). Exit code 0, no dump, no disconnect, no `Exitsave`, so a departure from the main loop and not a crash. `Game is shutting down` appears at every end and does not prove an intentional exit. Since patch 0020 the log shows whether the window was closed (`Window message`, `Window close command`). If the line is missing, the end came from the game itself.
Open log messages from this run:
- `OnEquipmentChangeEvent: failed to find server id` (about 130 times) and `neither a local or remote component`, clustered at cell changes. Such equipment events are lost until the actor has its server ID. Whether the equipment is right afterwards is unchecked.
- `Object not found for form id 1` (11 times) right after connecting, cause open.
- `Never found requested cookie` (8 times): actors were removed before the response came, probably harmless.

## Milestone 2026-10-30: two PCs on LAN (build ec824cad)
Host with built-in server, the other player visible in the host's log from 19:00:56 (`New entity remotely managed`, equipment is created). No crash until 19:03. Only the host's log is evaluated, the run was still active during evaluation.
- `Form id not found for magic add target, form id: 0` (1365 times, almost all 19:01:51–19:02:08, about 70 per second): local event without a target, queued under key 0 and overwritten, so log flood without memory growth. Should be throttled, or dropped without an entry for target 0.
- `Failed to retrieve object to activate` (twice, 19:01:17): the other player's activation did not find its object. Visible consequence unchecked.
- `OnNotifyAddTarget: Failed to retrieve effect by id 4B24F` (once).
- `Health change event form id component not found` (16 times), new compared to the one-client run.

## Status 2026-10-30: host save and own character (not tested in the game)
- **Stage 1:** The host's starter shares a save over TCP 10578 (`SaveShare` in `Starter.cs`), the player's starter stores it under the host's file name with the current time and deletes it when the game ends. The first test with the name `FTRHost_<time>.fos` ended with separate spawn points and two clicks on "Continue", probably because the game evaluates the naming scheme (client log missing, cause unproven). Loading goes through "Continue" in the main menu, so without game addresses. Transfer tested locally (identical file, wrong password refused, no leftovers). Not tested: two PCs, firewall, whether "Continue" really takes this file, Steam Cloud (the file can be uploaded during play).
- **Stage 2 (part):** `TP_FIGURE_MODE=save` writes inventory and SPECIAL to `figure.bin` every 60 s when playing with your own save, `apply` applies them in the host's world 10 s after connecting (`UpdateFigure` in `DebugService.cpp`, patch 0021). Compiled, never run. The file contains server mod IDs and therefore only fits with the same mod list. `SetActorInventory` removes the inventory of the host copy. Still missing: appearance (`TESNPC`), level, perks, quest items of the copy.
- **Automatic loading:** `BGSSaveLoadGame::LoadGame` is OG `0xCDE9F0`, NG `0xBEE760` (same strings). The NG path goes through `0xBF98D0` (manager, name, -1, 0, 1, 0). `BFD8E0` loads from manager `+0x60`/`+0x68` when flag `0x4` is set in `+0x40`. The setter has not been found yet and the structure differs in 1.10.163, so this is not used.

## Status 2026-10-30 evening: autostart, appearance, host exit (patch 0022, not tested in the game)
- LAN test with host save: both at the same place (`New entity remotely managed` 19:59:02), no dump.
- **Autostart:** IAT hook on `GetPrivateProfileStringA` (two callers in 1.11.240, `0x17F0905`, `0x17F097B`) returns the command `LoadGame "<name>"` for `sStartingConsoleCommand` when `TP_AUTOLOAD` is set. `LoadGame`/`load` is in the command table entry at file offset `0x2EFA1C0`. Log: `Loading save on start`. If it fails, the game stays in the main menu. Whether the command loads at this point is open.
- **Appearance and name:** `figure.bin` also contains `TESNPC::Serialize` (with `MarkChanged(0x2000800)`) and the name. `ApplyFigure` loads this onto the player's base form and calls `QueueUpdate`. Then `Close` and a reconnect, because the appearance is only transferred with the spawn request. Old characters without appearance stay readable but must be rewritten. Risk: `Load` changes the base form of the running player.
- **Host exit:** On connection loss in starter sessions (`TP_AUTOCONNECT`) the client shows a message and closes the window (`UpdateHostLost`). The loss is only detected after the connection timeout.

## Status 2026-10-30 evening: character automatically from your own save (patch 0023, not tested in the game)
With the host save and "bring your own character", the starter starts the game twice. First start: `TP_FIGURE_MODE=capture`, `TP_AUTOLOAD` on your own save, the client connects, writes `figure.bin` and closes the window. `figure.bin.source` remembers the save, and if the character is newer than it, this start is skipped. Second start: host save with `apply`. Two starts instead of loading twice in one run, because loading at runtime would only work through untested engine addresses. Character IDs of taken-over host saves are in `hosts.txt`, `SaveShare.Newest` skips them. If the first start hangs in the menu because automatic loading fails, the player must load there manually.

## Status 2026-10-30 evening: version check and new starter (patch 0024)
LAN test confirms: host save, own character and autostart work according to the player.
- **Version check:** `BUILD_COMMIT` does not change across our builds. So the starter sets `TP_BUILD` (the package's build ID) for client and server, and `AuthenticationRequest.Version` and the check in `GameServer` use it. Before the game starts, `SaveShare.CheckHost` queries build and game version over TCP (`FTR2 HELLO`). Differences and an unreachable host stop the start. Tested locally: same, different build ID, different game version, no host. The refusal by the server and `ShowRefusal` (plain text for version, mods, password, full) are only compiled.
- **Starter:** terminal-style interface (`Pip`, `PipCheck`, `PipButton`, `StatusView`) with step list, spinner and bar. In-game progress comes from `logs\tp_client.log` (`ReadLog`). Deliberately without Bethesda's marks and characters.

## Bug 2026-10-30: fell through the world on respawn (patch 0027, untested)
`RespawnPlayer` teleported to the cell centre through `GetCOCPlacementInfo`, a leftover from Skyrim. Now the player gets up where they fell. Entirely without `MoveTo` they stayed tilted and could not jump, because the animation graph and character controller are only reset by the move. So `MoveTo` onto their own position, 16 units higher (patch 0028, animation confirmed). After that jumping and the AP and ammo display were still missing, because only the life state bit was cleared. Patch 0029 called `Actor::Resurrect` for this (vtable slot 0xCC, two `bool` parameters, the declaration had only one), but on the player that replaces `currentProcess` and broke the weapon and shooting. The right call is `PlayerCharacter::SetLifeState(0)` (slot 0x105, OG `0xE9C0C0`, NG `0xD57980`, patch 0030, confirmed by the user in the LAN test with build `6e3f0df8-7C197844`). The 10 seconds of invulnerability from `RunPostDeathUpdates` remain.

## Bug 2026-10-30: client dies and lands in the main menu (patch 0025 guard, patch 0026 cause)
Game test with patch 0026 passed: `Player is down` twice, respawn fine according to the user.
**Cause (patch 0026, game test pending):** The game recalculates bit 18 of `Actor+0x43C` (OG `0xD71E60`, ID 1085880, NG `0xC5B2B0`). A value stored on the reference takes precedence (`ExtraData`, lookup `0x292940`), then the base form flag (`+0x70`, bit 1), then aliases (extra `0x88`). Real saves probably carry "not essential" after the prologue, and that beats our flag. Fresh test characters do not have the value, which is why the respawn worked there. `HookUpdateEssential` sets the bit again after the calculation, as long as `g_keepPlayerEssential` applies. That the player really carries the value is derived from the code, not measured in memory. The first check in patch 0025 also queried the base form and therefore constantly reported "Respawn mode was lost"; the `essential: true` in the log was worthless, because it was read after setting.

Client log of the LAN test: character applied and reconnected, then a reload at 20:34:26 without a disconnect and without a respawn entry. So the character probably died normally instead of bleeding out through `RunRespawnUpdates`. The protection (`SetPlayerRespawnMode`) is only set with the server settings on connecting, and reloading and `TESNPC::Load` can reset it. `RunRespawnUpdates` now checks it every second and sets it again. Log lines: `Respawn mode was lost`, `Player is down`, `Player died outside of the respawn system`. Why the game went to the main menu instead of the last save is open.

## Status 2026-10-30 night: cleanup (build `cd748d95-2DAD111D` runs according to the user)
Removed: Skyrim targets and Skyrim client, `Code/skyrim_ui`, `Code/one_ui`, `Code/admin`, semantic-release, Docker, upstream issue templates and the Sentry upload task. The mouse cursors are now in `Code/client/assets`.
Open: `#if TP_SKYRIM` in about 50 files, Discord integration with the upstream application ID, conventions and names in the code such as `TP_`, `Tilted`.

## Status 2026-10-31: Steam network tested (`Tools/steamtest`)
Two PCs connected under app ID 377160 through `P2P SDR`, relay `fra`. Setup about 2 s, median 30 to 33 ms, no packet loss while both ran. That was 2 packets per second, not game traffic. Direct connections (ICE) are off, because `steamwebrtc64.dll` crashed in the test. Open: whether Valve allows this use.

The tunnel in the starter (`package/Tunnel.cs`) is included. Tested over an artificial line with 30 ms and 3 % loss: bots with a real server, two players, hard abort with immediate restart, wrong password, version check, 20 MB save. According to the user the connection then also worked over the real Steam network between two PCs in a game session.

## Status 2026-10-31: settlement sync stage 1
Placing, moving and scrapping run through `WorkshopService` and the messages `RequestWorkshopChange`/`NotifyWorkshopChange`. Changes wait on the receiving side until the workbench is loaded. Confirmed by the user on two PCs. Log lines: `Workshop event` on the builder's side, `Workshop sync: placed` on the receiver's.

## Status 2026-10-31: settlement sync stage 2 (receiving side tested in the game)
The server remembers the latest state per object and sends it to everyone joining, with `Replay`. This lives only in memory and ends with the server, buildings persist through the host's save. When replaying, the client first looks for an equal object at the current and original spot (`FindClosestReferenceOfType`), so nothing is built twice if the save already holds it or a client reconnects. Log line: `Workshop sync: ... is already here as ...`. `scripts/test-server-bots.ps1` checks forwarding and merging.

`scripts/test-workshop-ingame.ps1` starts the test copy in Red Rocket and has a bot send real parts, with `-Late` the player only joins afterwards. Both runs passed on 2026-10-31. Client log of a two-PC session confirms replay, live moving and recognition on reconnect. Not checked: recognising buildings in the host's save with two real players.

## Status 2026-10-31: settlement sync stage 3, wires and power
Power wires are transferred and built on the receiving side, also for late joiners. They are detected through `QueueRebuildBendableSpline` and removed through `DisconnectSpline`. Before a copy is deleted, its wires are disconnected. `scripts/test-workshop-ingame.ps1` and `-Late` pass on 1.11.240, the wire is created without a crash.

The receiving side also adds the wire to the workbench's power grid the way the game does and creates missing power data itself, see [FINDINGS.md](FINDINGS.md), section 23. In the test, generator and pylon report `powered true`. Removing wire and pylon works.

Not checked: detection on the builder's side with a real player, wires in a settlement from a real save, whether visible consumers such as lamps turn on. Not included: switches on built objects, settler jobs. Turrets are actors and are not transferred as parts.

## Not tested
- Regression test on 1.10.163 with the Next-Gen changes and the shutdown hook.

## Open tasks (by priority)
1. **42 open addresses for 1.11.240** (`data/open-1-11-240-0.csv`): see the file; also 2 RTTI names without a hit (`data/rtti-1-11-240-0.missing.csv`).
   Ideas: similarity comparison (callers/callees/strings/constants), cross references from known functions,
   F4SE-NG sources (named addresses), manually in the disassembler. See [PORTING.md](PORTING.md).
2. **Check structure offsets and VTable indices** in the client for Next-Gen (class sizes and fields changed in
   places; reference: CommonLibF4-NG). Only visible in a game test.
3. **Real two-player test on 1.10.163** (`-Clients 2` or two PCs), longer session with `-Hold`.
4. **F4SE for 1.11.240** (0.7.9+) in the game folder; the client loads F4SE itself if present.
5. Upstream note: the off-by-one bug ([FINDINGS.md](FINDINGS.md)) only affects Fallout; Skyrim uses the other loader.

## Known problems and bugs
- On every start the server writes `mi_free: pointer does not point to a valid heap space` to stderr, before any client. Cause open, no consequences so far.
- Companions are not synced: the companion must be talked to separately in each client (tested with Dogmeat). Low priority, porting to the newest version comes first.
- The game freezes on exit (observed once at Red Rocket, exit code `0xCFFFFFFF`, Windows event 1002). It did not occur in the game itself, cause open. An earlier run ended cleanly with exit code 0.
- Launcher on 1.11.240 without complete offsets: stops on purpose with a message and writes
  `logs\missing-offsets-<version>.txt`. With `TP_ALLOW_MISSING_OFFSETS=1` it continues (crash expected).
- Server: on hard termination mimalloc reports `mi_free: pointer does not point to a valid heap space` (only on kill).
- Server: Sentry init and server list ping fail (irrelevant for LAN).
- The launcher otherwise finds the game installation through the Bethesda registry, so always pass `--exePath` for test copies.
- `Code/client/main.cpp` opens a Nexus page if the Address Library is missing (upstream behaviour).

## Versions
| | |
|---|---|
| Upstream base | TiltedEvolution `d6ce567b` (2024-11-03) |
| Game target | Fallout 4 1.11.240 (Steam build 24564252), SHA-256 `fdcef37a…72f8` |
| Game Old-Gen | 1.10.163, depot 377162 manifest `5847529232406005096` |
| Intermediate version for validation | 1.11.191, depot 377162 manifest `5433405173062582852` |
