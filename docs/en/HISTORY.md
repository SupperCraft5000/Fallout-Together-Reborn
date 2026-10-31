# Patch numbers and commits

Development first ran as a patch series on `d6ce567b`. That is why [FINDINGS.md](FINDINGS.md) and [STATUS.md](STATUS.md) contain references like `patches/0014` or `Patch 0026`. Those files no longer exist here, each patch is a commit on `main`:

| Patch | Commit | Content |
|---|---|---|
| `0001` | `3e0e9774` | Add headless FalloutBot, client auto-connect and build fixes |
| `0002` | `775903d9` | Fallout 4: use real Address Library IDs and per-version offset tables |
| `0003` | submodule TiltedHooks | [patches/TiltedHooks-dinput.patch](../../patches/TiltedHooks-dinput.patch) |
| `0004` | `2e184e52` | Fallout 4: fix remaining off-by-one IDs, flush client log |
| `0005` | `a6f5a805` | Fallout 4: use real Address Library IDs for RTTI descriptors |
| `0006` | `5ff42ddd` | Client: add TP_NO_OVERLAY_RENDER diagnostic switch |
| `0007` | `38dccfa7` | Server: create a party for the first player |
| `0008` | `3665efbf` | Quests: sync miscellaneous quests with objectives, log quest forwarding |
| `0009` | `7759d9d3` | Quests: start stopped quests before applying a remote stage |
| `0010` | `2375adfa` | Fallout 4: support next-gen code offsets (1.11.240) |
| `0011` | `5fce1fce` | Launcher: honor SteamStub NoEncryption flag, crash handler: fix recursion |
| `0012` | `66ad94ca` | Launcher: fix SteamStub header layout, add next-gen startup hook |
| `0013` | `d01cf168` | Launcher: pick the startup hook by the game's imports |
| `0014` | `ee5976b3` | Client: fix next-gen VM flag, skip engine teardown on exit |
| `0015` | `535a5a91` | Client: fix respawn loop after player death on Fallout 4 |
| `0016` | `2c99169b` | Crash handler: don't send reports to the upstream Sentry project |
| `0017` | `960330b2` | VersionDb: prefer offset table next to the launcher |
| `0018` | `3826ee89` | Memory: hook realloc as well |
| `0019` | `5413c174` | Memory: send foreign blocks back to the CRT |
| `0020` | `656709ef` | Fallout4: log why the game window closes |
| `0021` | `3fc4259b` | Fallout4: save and apply the player's figure |
| `0022` | `91c28b93` | Fallout4: load a save on start, carry the look, handle a lost host |
| `0023` | `552ed02e` | Fallout4: capture the figure and close for the launcher |
| `0024` | `faae34d6` | Check the package build on connect and explain refusals |
| `0025` | `410a2e28` | PlayerService: keep the respawn mode alive and log deaths |
| `0026` | `4772a534` | Fallout4: keep the player essential across the game's own update |
| `0027` | `ed824812` | Fallout4: respawn the player in place |
| `0028` | `952cd318` | Fallout4: move the respawned player onto itself to reset the pose |
| `0029` | `627bb0d7` | Fallout4: use the game's Resurrect for respawning, fix its signature |
| `0030` | `e77b0406` | Fallout4: leave the downed state through the game's SetLifeState |

View one with `git show <commit>`. All changes from upstream: `git diff d6ce567b..main`.
