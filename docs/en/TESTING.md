# Testing

## 1. Server test without the game (automatic, passing)
```powershell
scripts\test-server-bots.ps1            # 2 bots, port 10600
scripts\test-server-bots.ps1 -Bots 3 -Port 10601
```
Starts the server with a password and checks:
- wrong password gives `wrong_password`, exit 2,
- N bots get `accepted`, each sees the others (`NotifyPlayerJoined`),
- settlement edits are forwarded live and replayed to a late bot as a merged state.
Logs are in `$env:FTR_WORK\server-test\` (path in the script).

`FalloutBot <host:port> <name> <seconds> [password] [bauen|spieltest]`: exit 0 accepted, 2 refused, 3 no connection.
It sends the version (`BUILD_COMMIT`), name, password and the official masters. New checks go in `Code/bot/Main.cpp`.

## 2. Test copy of the game
Never touch the original. A test copy is a folder with **hard links** for large, unchanging data (`*.ba2`, `*.esm`,
`*.esl`, videos) and real copies of everything else. Never write to hard links (only delete and replace them).

### Separate game profile (`scripts/test-profile.ps1 -Enter / -Leave`)
Fallout 4 always uses `Documents\My Games\Fallout4` and `AppData\Local\Fallout4`. The script renames the real
folders to `*.echt` and puts a test profile in place (window 1280x720, official masters only,
`sStartingConsoleCommand=coc RedRocketExt`). Always run `-Leave` after the test.
With `-Enter -MainMenu` the game starts in the main menu instead. Saves are in `$env:FTR_WORK\profile\Documents\Saves`.

## 3. Game test (`scripts/test-ingame.ps1`, passed on 1.10.163)
Server + `FalloutTogether.exe --exePath <test copy>\Fallout4.exe` with `TP_AUTOCONNECT=127.0.0.1:<port>` and
`TP_PASSWORD`. The client connects once the player is in a cell (log: "Connected to server as
player N"). A bot then joins as an observer and must see the players.

Important: always pass `--exePath`, otherwise the launcher takes the installation from the Bethesda registry.
Start from a normal user shell (no windows appear in sandboxes).

## 4. Validating the address port
```powershell
python -I Tools\port\validate.py --clib <CommonLibF4> --og-bin version-1-10-163-0.bin --ng-bin version-1-11-191-0.bin `
    --old Fallout4-1.10.163.analysis.exe --new Fallout4-1.11.191.exe
```
See [PORTING.md](PORTING.md) for cross-validation and results.

## 5. Connecting over the Steam network (`Tools/steamtest`)
Checks without the game whether two PCs can find each other over Steam P2P and Valve's relay network. The tunnel in the starter is based on this.
```powershell
Tools\steamtest\build.ps1
dist\SteamTest.exe
```
Both players start the exe at the same time and enter the other side's Steam ID. It uses `steam_api64.dll` from the Fallout folder under app ID 377160, and Steam shows "playing Fallout 4" meanwhile. Other app ID: `--appid <id>`. Direct connections (ICE) are off, because `steamwebrtc64.dll` crashed with them, `--ice` turns them back on for comparison. The result is in `steam-test-ergebnis.txt` next to the exe. Messages such as `Assertion Failed` in the console window come from the Steam library and are not an error by themselves.

## 6. Settlement sync in the game (`scripts/test-workshop-ingame.ps1`)
Starts the test copy in Red Rocket, a bot sends real parts for the Red Rocket workbench, and the script evaluates the client log. It enters and leaves the test profile by itself.
```powershell
scripts\test-workshop-ingame.ps1         # player is present, changes arrive live
scripts\test-workshop-ingame.ps1 -Late   # edits come first, the player joins afterwards
```
Start it from a normal user shell, and do not start Fallout 4 while it runs. It only covers the receiving side, the sending side needs a real player.
