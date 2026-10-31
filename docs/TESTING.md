# Testen

## 1. Servertest ohne Spiel (automatisch, grün)
```powershell
scripts\test-server-bots.ps1            # 2 Bots, Port 10600
scripts\test-server-bots.ps1 -Bots 3 -Port 10601
```
Startet den Server mit Passwort, prüft:
- falsches Passwort → `wrong_password`, Exit 2,
- N Bots → `accepted`, jeder sieht die anderen (`NotifyPlayerJoined`).
Logs unter `$env:FTR_WORK\server-test\` (Pfad im Skript).

`FalloutBot <host:port> <name> <sekunden> [passwort]` – Exit 0 angenommen, 2 abgewiesen, 3 keine Verbindung.
Er schickt Version (`BUILD_COMMIT`), Name, Passwort und die offiziellen Masters. Neue Prüfungen: `Code/bot/Main.cpp`.

## 2. Testkopie des Spiels
Original nie anfassen. Testkopie = Ordner mit **Hardlinks** für große, unveränderliche Daten (`*.ba2`, `*.esm`,
`*.esl`, Videos) und echten Kopien für alles andere. Hardlinks nie beschreiben (nur löschen/ersetzen).

### Eigenes Spielprofil (`scripts/test-profile.ps1 -Enter / -Leave`)
Fallout 4 nutzt fest `Dokumente\My Games\Fallout4` und `AppData\Local\Fallout4`. Das Skript benennt die echten
Ordner in `*.echt` um und setzt ein Testprofil ein (Fenster 1280x720, nur offizielle Masters,
`sStartingConsoleCommand=coc RedRocketExt`). Nach dem Test unbedingt `-Leave`.
Mit `-Enter -MainMenu` startet das Spiel stattdessen im Hauptmenü. Spielstände liegen in `$env:FTR_WORK\profile\Documents\Saves`.

## 3. Spieltest (`scripts/test-ingame.ps1`, auf 1.10.163 bestanden)
Server + `FalloutTogether.exe --exePath <Testkopie>\Fallout4.exe` mit `TP_AUTOCONNECT=127.0.0.1:<port>` und
`TP_PASSWORD`. Der Client verbindet sich, sobald der Spieler in einer Zelle ist (Log: „Connected to server as
player N“). Danach meldet sich ein Bot als Beobachter und muss die Spieler sehen.

Wichtig: `--exePath` immer angeben, sonst nimmt der Launcher die Installation aus der Bethesda-Registry.
Aus einer normalen Benutzer-Shell starten (in Sandboxes erscheinen keine Fenster).

## 4. Adress-Portierung validieren
```powershell
python -I Tools\port\validate.py --clib <CommonLibF4> --og-bin version-1-10-163-0.bin --ng-bin version-1-11-191-0.bin `
    --old Fallout4-1.10.163.analysis.exe --new Fallout4-1.11.191.exe
```
Siehe docs/PORTING.md für Kreuzvalidierung und Ergebnisse.

## 5. Verbindung über das Steam-Netz (`Tools/steamtest`)
Prüft ohne Spiel, ob zwei PCs über Steam P2P und das Relay-Netz von Valve zueinander finden. Grundlage für einen möglichen Tunnel statt Portfreigabe, im Mod ist das noch nicht eingebaut.
```powershell
Tools\steamtest\build.ps1
dist\SteamTest.exe
```
Beide starten die exe gleichzeitig und tragen die Steam-ID der Gegenseite ein. Sie nutzt die `steam_api64.dll` aus dem Fallout-Ordner unter App-ID 377160, Steam zeigt dabei „spielt Fallout 4“. Andere App-ID: `--appid <id>`. Direkte Verbindungen (ICE) sind aus, weil `steamwebrtc64.dll` dabei abstürzte, `--ice` schaltet sie zum Vergleich wieder ein. Das Ergebnis steht in `steam-test-ergebnis.txt` neben der exe. Meldungen wie `Assertion Failed` im Konsolenfenster stammen aus der Steam-Bibliothek und sind allein kein Fehler.
