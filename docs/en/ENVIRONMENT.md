# Environment variables

## For scripts and tools
| Variable | Required for | Meaning |
|---|---|---|
| `FTR_WORK` | all scripts | Short working folder, for example `D:\ftr`. Contains the xmake cache (`xg`), test copies (`Fallout4-A`, `Fallout4-B`), test profile (`profile`), Ghidra data (`ghidra`) and working copies of the exe (`exe-1.10.163`, `exe-1.11.240`) |
| `FTR_XMAKE` | `scripts/xm.ps1`, optional | Path to `xmake.exe`, default is xmake from the `PATH` |
| `FTR_GAME_DIR` | `scripts/make-testcopy-ng.ps1` | Fallout 4 folder the test copy is made from. Only read |
| `FTR_GHIDRA` | `Tools/port/ghidra/run-*.ps1` | Ghidra folder with `support\analyzeHeadless.bat` |
| `FTR_JAVA_HOME` | `Tools/port/ghidra/run-*.ps1` | JDK for Ghidra |

## For the client
The starter sets these variables. You only need them by hand for tests.

| Variable | Meaning |
|---|---|
| `TP_AUTOCONNECT` | `host:port`, connects once the player is in a cell. Also marks starter sessions: only then do refusal and disconnect messages appear, and the game closes afterwards |
| `TP_PASSWORD` | server password |
| `TP_BUILD` | version tag for client and server, replaces `BUILD_COMMIT`. Must be the same on both sides |
| `TP_AUTOLOAD` | save without `.fos`, run as `sStartingConsoleCommand` |
| `TP_FIGURE_FILE` | path to `figure.bin` |
| `TP_FIGURE_MODE` | `save` writes the character every 60 s, `capture` writes it once and closes the game, `apply` applies it |
| `TP_ALLOW_MISSING_OFFSETS` | `1` starts despite missing addresses, for tests only |
| `TP_NO_OVERLAY_RENDER` | diagnostics: do not draw overlay and ImGui |
