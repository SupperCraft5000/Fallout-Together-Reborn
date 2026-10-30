# Umgebungsvariablen

## Für Skripte und Werkzeuge
| Variable | Pflicht für | Bedeutung |
|---|---|---|
| `FTR_WORK` | alle Skripte | Kurzer Arbeitsordner, zum Beispiel `D:\ftr`. Enthält den xmake-Cache (`xg`), Testkopien (`Fallout4-A`, `Fallout4-B`), Testprofil (`profile`), Ghidra-Daten (`ghidra`) und Arbeitskopien der exe (`exe-1.10.163`, `exe-1.11.240`) |
| `FTR_XMAKE` | `scripts/xm.ps1`, optional | Pfad zur `xmake.exe`, Standard ist xmake aus dem `PATH` |
| `FTR_GAME_DIR` | `scripts/make-testcopy-ng.ps1` | Fallout-4-Ordner, aus dem die Testkopie entsteht. Wird nur gelesen |
| `FTR_GHIDRA` | `Tools/port/ghidra/run-*.ps1` | Ghidra-Ordner mit `support\analyzeHeadless.bat` |
| `FTR_JAVA_HOME` | `Tools/port/ghidra/run-*.ps1` | JDK für Ghidra |

## Für den Client
Der Starter setzt diese Variablen. Von Hand braucht man sie nur für Tests.

| Variable | Bedeutung |
|---|---|
| `TP_AUTOCONNECT` | `host:port`, verbindet, sobald der Spieler in einer Zelle ist. Kennzeichnet auch Starter-Sitzungen: Nur dann erscheinen Ablehnungs- und Trennungsmeldungen, und das Spiel schließt sich danach |
| `TP_PASSWORD` | Serverpasswort |
| `TP_BUILD` | Versionskennung für Client und Server, ersetzt `BUILD_COMMIT`. Muss auf beiden Seiten gleich sein |
| `TP_AUTOLOAD` | Spielstand ohne `.fos`, wird als `sStartingConsoleCommand` ausgeführt |
| `TP_FIGURE_FILE` | Pfad zur `figure.bin` |
| `TP_FIGURE_MODE` | `save` schreibt die Figur alle 60 s, `capture` schreibt sie einmal und schließt das Spiel, `apply` setzt sie ein |
| `TP_ALLOW_MISSING_OFFSETS` | `1` startet trotz fehlender Adressen, nur für Tests |
| `TP_NO_OVERLAY_RENDER` | Diagnose: Overlay und ImGui nicht zeichnen |
