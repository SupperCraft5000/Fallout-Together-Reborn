# Starter and session flow

Overview of the mod itself: [AGENTS.md](AGENTS.md). This page covers what happens between starter, client and server.

## Components
- **Starter** `package/Starter.cs` (C# 5, WinForms): single exe, contains client and server as `payload.zip`.
- **Client** `FalloutTogether.exe`: loads `Fallout4.exe` into its own process.
- **Server** `FalloutTogetherServer.exe` + `FTServer.dll`: runs invisibly on the host, UDP 10578.
- **Starter service** on the host: TCP 10578, class `SaveShare`.

## Data on the PC
Everything is in `%LOCALAPPDATA%\FalloutTogetherReborn`:

| Path | Content |
|---|---|
| `<build>\` | unpacked version, `.complete` marks it as complete |
| `<build>\logs\tp_client.log` | client log, the starter follows it for the status display |
| `<build>\crash_*.dmp` | crash dump |
| `settings.txt` | address and game folder |
| `figure.bin` | your own character |
| `figure.bin.source` | save the character came from |
| `hosts.txt` | character IDs of taken-over host saves, the automatic choice skips them |
| `logs-previous\` | latest logs of the most recently deleted version |

The starter deletes older version folders on start (`Payload.RemoveOldBuilds`), as long as no game or server is running.

## Starter service protocol
Line-based over TCP, one request per connection:

| Request | Response |
|---|---|
| `FTR2 HELLO` | `<build> <game version>` |
| `FTR2 SAVE <password>` | `OK <size> <file name>`, then the file, or `NO` |

## Start on the host
1. `Payload.Prepare` unpacks, old versions are removed.
2. Server and starter service start, both with `TP_BUILD`.
3. The game loads the chosen save and connects to `127.0.0.1`.

## Start on a player's PC
1. `CheckHost`: build and game version must match, otherwise it stops before the game starts.
2. `Fetch`: the host's save is downloaded, named as on the host but with the current time. The game evaluates the file name.
3. With **Eigene Figur mitnehmen** (bring your own character) there are two starts:
   - First start: `capture` with your own save. The client connects, after 10 s `figure.bin` is written and the window closes. Skipped if the character is newer than the save.
   - Second start: `apply` with the host's save. After 10 s `ApplyFigure` applies appearance, name, inventory and SPECIAL, then the client reconnects, because the appearance is only transferred with the spawn request.
4. When the game ends, the starter deletes the copy of the host's save.

Why two starts: loading at runtime would only work through engine addresses that are not ported. Why the 10 s wait: the character contains server mod IDs that are only valid after connecting. It therefore only fits with the same mod list.

## Steam tunnel
`package/Tunnel.cs`. The starter launches itself with `--tunnel` as a separate process, because the tunnel is needed before the game runs. Game and server keep talking UDP to a port on 127.0.0.1, the starter service talks TCP.

| Channel | Content |
|---|---|
| 0, unreliable | UDP datagrams with a 2-byte tag, the game's source port |
| 1, reliable | TCP streams as frames: type (open, data, end), stream ID, data |

On the host each tag has its own UDP socket. With a shared socket, the server would take a new game for the old session if that session's end never arrived, for example after the double start with your own character.

Only the relay is used (`P2P_Transport_ICE_Enable=0`), because `steamwebrtc64.dll` crashed in testing. The host accepts every session, the password is the protection. In the game only `SteamLink` is used, in tests `ITunnelLink` replaces Steam with an artificial line.

## Autostart
The hook on `GetPrivateProfileStringA` in `Code/client/FalloutVM.cpp` returns the command `LoadGame "<name>"` for `sStartingConsoleCommand`. If that fails, the game stays in the main menu.

## Death and respawn
1. `SetPlayerRespawnMode` sets essential. `HookUpdateEssential` keeps the bit, because the game otherwise recalculates it.
2. The game sets life state 7/8, `RunRespawnUpdates` notices that and waits 5 s.
3. `RespawnPlayer`: `SetLifeState(0)` removes the input lock and camera state, `MoveTo` onto the player's own position resets animation and physics.
4. After that 10 s of invulnerability.

Background: [FINDINGS.md](FINDINGS.md), section 20.

## Version check
Two stages: `CheckHost` before the game starts and `GameServer` on connecting, both using the build ID of the package. `BUILD_COMMIT` alone is not enough, because it can stay the same across our builds.
