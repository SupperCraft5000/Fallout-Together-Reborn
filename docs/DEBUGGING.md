# Fehlersuche

## Logs
| Datei | Inhalt |
|---|---|
| `%LOCALAPPDATA%\FalloutTogetherReborn\<build>\logs\tp_client.log` | Client |
| `...\<build>\logs\cef_debug.log` | Overlay |
| `...\<build>\server-run\logs\` | Server |
| `...\logs-previous\` | letzte Logs einer gelöschten Version |

Der Build-Ordner aus dem Quellcode schreibt nach `build\windows\x64\releasedbg\logs`.

## Wichtige Logzeilen
| Zeile | Bedeutung |
|---|---|
| `Loading save on start` | Autostart hat gegriffen |
| `Finished loading` | Spielstand geladen |
| `Connected to server as player N` | Verbindung steht |
| `Server refused the connection, type N` | Ablehnung: 1 Version, 2 Mods, 3 Client-Mods, 4 Passwort, 5 voll |
| `Applied figure` | Figur eingesetzt |
| `Player is down` | Respawn-System läuft |
| `Player died outside of the respawn system` | echter Tod, der Schutz hat nicht gegriffen |
| `Window close command` | Fenster wurde geschlossen. Ohne diese Zeile vor `Game is shutting down` kam das Ende aus dem Spiel selbst |
| `Workshop event` | Dieser PC hat etwas gebaut, verschoben oder verschrottet |
| `Workshop sync: placed` / `moved` / `removed` | Änderung eines anderen Spielers wurde übernommen |
| `Workshop sync: ... is already here as ...` | Nachgeholtes Objekt ist schon da, nichts wurde doppelt gebaut |

`Game is shutting down` erscheint bei jedem Ende und ist kein Beleg für ein gewolltes Beenden.

Bekannte, harmlose Meldungen stehen in [STATUS.md](STATUS.md).

## Abstürze auswerten
Adressen ab `0x1B0000000` gehören zum Launcher und damit zum Client, Offset = Adresse minus `0x140000000`.

```powershell
python -I Tools\port\addr2sym.py build\windows\x64\releasedbg\FalloutTogether.exe 70B31146
python -I Tools\port\dumpinfo.py crash_UTC_....dmp build\windows\x64\releasedbg\FalloutTogether.exe 40
```
`addr2sym.py` braucht die PDB des exakt gleichen Builds. `dumpinfo.py` liest Ausnahme, Register und Stack-Kandidaten. Die Dumps enthalten nur Daten-Segmente, keinen Heap, Stack-Treffer sind deshalb Kandidaten und keine Aufrufkette.

Spieladressen übersetzt man mit den Dumps aus [PORTING.md](PORTING.md).

## Vorgehen bei Spiel-Bugs
1. Ursache im Dekompilat belegen, nicht nur aus dem Symptom schließen.
2. Beide Versionen vergleichen, wenn der Code 1.10.163 mitbetrifft.
3. Spielzustände nie direkt schreiben, wenn es einen Setter gibt. Beispiel: [FINDINGS.md](FINDINGS.md), Abschnitt 20.
4. Neue Adressen als 1.10.163-ID eintragen und in `data/ids-1.10.163.csv` sowie `data/final-*.csv` ergänzen. Fehlt eine, startet der Client nicht.
