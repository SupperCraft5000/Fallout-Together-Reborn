# Fallout Together Reborn

**English** | [Deutsch](README.md)

Unofficial co-op for Fallout 4. Playable by two players on **Fallout 4 1.11.240**.

> This is a fan project. It is not affiliated with Bethesda Softworks, ZeniMax, Microsoft or Tilted Phoques.
> Fallout is a trademark of its respective owners. You need your own purchased copy of the game. This repository contains no game files.

## Quick start
**Both players need:** Fallout 4 from Steam in version 1.11.240, Steam running, and the same `FalloutTogetherReborn.exe` from the [Releases](https://github.com/SupperCraft5000/Fallout-Together-Reborn/releases). There is nothing to install and your game folder stays unchanged. Windows warns on the first start because the file is not signed: choose "More info", then "Run anyway". The starter's interface is in German, so the translation of each label is in brackets.

**Host (starts first):**
1. Start `FalloutTogetherReborn.exe`.
2. Tick "Server auf diesem PC starten" (start server on this PC), "Über Steam erreichbar" (reachable over Steam) and "Meinen Spielstand freigeben" (share my save).
3. Enter a password and click **Spielen** (Play).
4. Give the other player the password and your Steam ID. The ID is shown in the status and copied to the clipboard.

**Other player (starts once the host's game is running):**
1. Start `FalloutTogetherReborn.exe`.
2. Enter the host's Steam ID and the password.
3. Tick "Spielstand vom Host übernehmen" (take over the host's save). "Eigene Figur mitnehmen" (bring your own character) brings your character into the host's world.
4. Click **Spielen** (Play). The game may start twice, which is normal.

It is best to use a separate save for co-op and to turn other mods off. More, including connecting by IP: [docs/en/PLAYING.md](docs/en/PLAYING.md).

## Origin
Fallout Together Reborn is a fork of [TiltedEvolution / Tilted Online](https://github.com/tiltedphoques/TiltedEvolution) by Tilted Phoques and builds on their last state with Fallout support (`d6ce567b`). The upstream history is preserved, and the original README is in [README-UPSTREAM.md](README-UPSTREAM.md). Questions about this project belong here, not with Tilted Phoques.

## What works
- Two players see each other, along with characters, quests, enemies and equipment
- Single exe with a built-in server, terminal-style interface and status display
- Connection by address or over the Steam network, without port forwarding
- Taking over the host's save while keeping your own character with appearance, inventory and SPECIAL
- Loading the save automatically, version check between host and player
- Death and respawn with real saves
- Settlement building: placing, moving and scrapping appear live and are replayed to players who join later

## Known limits
- Tested only with two players on 1.11.240
- 1.10.163 has not been tested for many changes
- Longer sessions are not tested
- Level and perks of your own character are not carried over
- Power wires are transferred, but power does not flow for the other player yet. Settler jobs and switches are not synced
- After a Fallout update it only runs again once the addresses are ported, see [docs/en/PORTING.md](docs/en/PORTING.md)

## Documentation
| File | Content |
|---|---|
| [docs/en/PLAYING.md](docs/en/PLAYING.md) | Playing |
| [docs/en/BUILD.md](docs/en/BUILD.md) | Building on Windows |
| [docs/en/STATUS.md](docs/en/STATUS.md) | Status, open tasks, bugs |
| [docs/en/TESTING.md](docs/en/TESTING.md) | Tests |
| [docs/en/PORTING.md](docs/en/PORTING.md) | New Fallout versions |
| [docs/en/FINDINGS.md](docs/en/FINDINGS.md) | Findings about the engine |
| [docs/en/ARCHITECTURE.md](docs/en/ARCHITECTURE.md) | Starter, protocol, session flow |
| [docs/en/DEBUGGING.md](docs/en/DEBUGGING.md) | Logs, crashes, approach to bugs |
| [docs/en/ENVIRONMENT.md](docs/en/ENVIRONMENT.md) | Environment variables |
| [docs/en/HISTORY.md](docs/en/HISTORY.md) | Old patch numbers and commits |
| [docs/en/CONTRIBUTING.md](docs/en/CONTRIBUTING.md) | Reporting bugs, contributing |
| [docs/en/REPOSITORIES.md](docs/en/REPOSITORIES.md) | Gitea, GitHub, branches and syncing |
| [docs/en/AGENTS.md](docs/en/AGENTS.md) | Starting point for contributors and AI agents |

## License
GPL-3.0-or-later, see [LICENSE](LICENSE) and [COPYING](COPYING). Third-party components: [docs/en/THIRD-PARTY.md](docs/en/THIRD-PARTY.md). Credits: [docs/en/CREDITS.md](docs/en/CREDITS.md). Changes from upstream are in the commit history from `d6ce567b` onward.
