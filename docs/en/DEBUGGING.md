# Troubleshooting

## Logs
| File | Content |
|---|---|
| `%LOCALAPPDATA%\FalloutTogetherReborn\<build>\logs\tp_client.log` | client |
| `...\<build>\logs\cef_debug.log` | overlay |
| `...\<build>\server-run\logs\` | server |
| `...\logs-previous\` | latest logs of a deleted version |

The build folder from source writes to `build\windows\x64\releasedbg\logs`.

## Important log lines
| Line | Meaning |
|---|---|
| `Loading save on start` | autostart took effect |
| `Finished loading` | save loaded |
| `Connected to server as player N` | connection is up |
| `Server refused the connection, type N` | refusal: 1 version, 2 mods, 3 client mods, 4 password, 5 full |
| `Applied figure` | character applied |
| `Player is down` | respawn system is running |
| `Player died outside of the respawn system` | real death, the protection did not take effect |
| `Window close command` | window was closed. Without this line before `Game is shutting down`, the end came from the game itself |
| `Workshop event` | this PC placed, moved or scrapped something |
| `Workshop sync: placed` / `moved` / `removed` | a settlement edit from another player was applied |
| `Workshop sync: ... is already here as ...` | a replayed object already exists, nothing was built twice |
| `Workshop event wire` / `Workshop sync: wired` | wire sent / built on the receiving side |
| `Workshop sync: ... powered true/false` | power state of a wire's end, 3 s after connecting |

`Game is shutting down` appears at every end and does not prove an intentional exit.

Known, harmless messages are listed in [STATUS.md](STATUS.md).

## Evaluating crashes
Addresses from `0x1B0000000` belong to the launcher and thus the client, offset = address minus `0x140000000`.

```powershell
python -I Tools\port\addr2sym.py build\windows\x64\releasedbg\FalloutTogether.exe 70B31146
python -I Tools\port\dumpinfo.py crash_UTC_....dmp build\windows\x64\releasedbg\FalloutTogether.exe 40
```
`addr2sym.py` needs the PDB of the exact same build. `dumpinfo.py` reads exception, registers and stack candidates. The dumps contain only data segments and no heap, so stack hits are candidates, not a call chain.

Game addresses are translated with the dumps from [PORTING.md](PORTING.md).

## Approach to game bugs
1. Prove the cause in the decompiled code, do not conclude it from the symptom alone.
2. Compare both versions if the code also affects 1.10.163.
3. Never write game state directly if there is a setter. Example: [FINDINGS.md](FINDINGS.md), section 20.
4. Enter new addresses as 1.10.163 IDs and add them to `data/ids-1.10.163.csv` and `data/final-*.csv`. If one is missing, the client does not start.
