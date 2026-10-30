# Stand (2026-10-30)

Spielbar im LAN über zwei PCs auf Fallout 4 1.11.240 mit echten Spielständen. Die Patchnummern unten beziehen sich auf die Commit-Historie, Commits tragen dieselbe Reihenfolge.

## Funktioniert (verifiziert)
| Was | Nachweis |
|---|---|
| Build aller Fallout-Ziele (Launcher, Client, Server, Runner, Bot) | xmake releasedbg, MSVC 14.44 |
| Server startet, lauscht (Standardport 10578) | Rauchtest |
| `FalloutBot`: kopfloser Client meldet sich an | `scripts/test-server-bots.ps1`: falsches Passwort abgewiesen, 2 und 3 Bots angenommen und sehen einander |
| Echte Address-Library-IDs, gleiches Verhalten auf 1.10.163 | alle 159 Verwendungen nachgerechnet: identische Adressen |
| Adress-Portierung 1.10.163 → 1.11.240 | 138 von 139 Code-IDs (Ghidra, siehe `data/ghidra-report-1-11-240-0.md`), `ctor` im Client nachgebaut, RTTI vollständig bis auf 2 entfernte Typen und 3.233 von 3.235 RTTI-IDs; zwei unabhängige Wege stimmen in allen 82 gemeinsamen überein |
| Präzision der Portierung | Kreuzvalidierung gegen 1.11.191 (Address Library + CommonLibF4): Funktionen 0 Fehler bei Lückengröße 4 |

## Meilenstein 2026-10-30: erster Spieltest bestanden (1.10.163)
`scripts/test-ingame.ps1`: Client startet im echten Spiel mit F4SE 0.6.23, verbindet sich per Auto-Connect, der
Server legt die Objekte der Zelle an, der Bot sieht den Spieler als Mitspieler. Das Skript beendet das Spiel danach.
Offen aus diesem Lauf: leerer Spielername, Log-Meldungen `activated object has no parent` und
`Form id not found for magic add target`, Spiel mit zwei echten Clients, längere Sitzung (`-Hold`).

## Meilenstein 2026-10-30: erster Spieltest auf 1.11.240 (ein Client)
Der Client startet, kommt in Red Rocket, verbindet sich mit dem Server und sendet Quests und Actors. Das Beenden lief sauber (Log „skipping engine teardown“), ohne Absturz und ohne Dump.
Im Log gab es keine auf 1.11.240 begrenzten Fehler außer sieben Warnungen „neither a local or remote component“ (Actor noch ohne Server-ID, wenige Millisekunden später kam sie an; bewertet als Zeitfenster, nicht geprüft).

## Meilenstein 2026-10-30: zwei Clients auf 1.11.240
Zwei Instanzen auf einem PC, beide verbunden. Laut Spieltest läuft es, im Log kommen entfernte Figuren und Quest-Updates an, kein Absturz. Jede Instanz lädt mit zwei Clients rund 80 Sekunden, das Fenster wirkt in der Zeit eingefroren.
Neu im Log: `MagicService::OnNotifyAddTarget: Failed to retrieve effect by id` (2-mal), nicht untersucht. Die Testkopie enthält Creation-Club-`.esl`, die Clients melden 19 statt 8 Mods.
Nicht geprüft: Kampf, Spielstand, zwei PCs über Netzwerk, längere Sitzung.

## Stand 2026-10-30 abends: Einzel-exe und echter Spielstand
`FalloutTogetherReborn.exe` lief mit eingebautem Server und einem echten Spielstand (Stufe 57): laden, verbinden, kein Absturz. Nur ein Client, auf der Testkopie.
**Offen:** Das Spiel beendete sich um 17:04:09 laut Spieler ohne dessen Zutun, 52 s nach dem Verbinden und 6 s nach einem Standortwechsel (`HC_ChangeLocationRainingHandler`). Exit-Code 0, kein Dump, keine Trennung, kein `Exitsave`, also ein Verlassen der Hauptschleife und kein Absturz. `Game is shutting down` erscheint bei jedem Ende und belegt kein gewolltes Beenden. Seit Patch 0020 steht im Log, ob das Fenster geschlossen wurde (`Window message`, `Window close command`). Fehlt die Zeile, kam das Ende aus dem Spiel selbst.
Offene Log-Meldungen aus diesem Lauf:
- `OnEquipmentChangeEvent: failed to find server id` (rund 130-mal) und `neither a local or remote component`, gehäuft beim Zellwechsel. Solche Ausrüstungs-Events gehen verloren, bis der Actor seine Server-ID hat. Ob die Ausrüstung danach stimmt, ist ungeprüft.
- `Object not found for form id 1` (11-mal) direkt nach dem Verbinden, Ursache offen.
- `Never found requested cookie` (8-mal): Actors wurden entfernt, bevor die Antwort kam, vermutlich harmlos.

## Meilenstein 2026-10-30: zwei PCs im LAN (Build ec824cad)
Host mit eingebautem Server, Mitspieler ab 19:00:56 im Log des Hosts sichtbar (`New entity remotely managed`, Ausrüstung wird angelegt). Bis 19:03 kein Absturz. Ausgewertet ist nur das Log des Hosts, der Lauf war beim Auswerten noch aktiv.
- `Form id not found for magic add target, form id: 0` (1365-mal, davon fast alle 19:01:51–19:02:08, rund 70 pro Sekunde): lokales Event ohne Ziel, wird unter Schlüssel 0 eingereiht und überschrieben, also Log-Flut ohne Speicherwachstum. Gehört gedrosselt oder bei Ziel 0 ohne Eintrag verworfen.
- `Failed to retrieve object to activate` (2-mal, 19:01:17): Aktivierung des anderen Spielers fand ihr Objekt nicht. Sichtbare Folge ungeprüft.
- `OnNotifyAddTarget: Failed to retrieve effect by id 4B24F` (1-mal).
- `Health change event form id component not found` (16-mal), neu gegenüber dem Lauf mit einem Client.

## Stand 2026-10-30: Spielstand vom Host und eigene Figur (ungetestet im Spiel)
- **Stufe 1:** Der Starter des Hosts gibt einen Spielstand über TCP 10578 frei (`SaveShare` in `Starter.cs`), der Starter des Mitspielers legt ihn unter dem Dateinamen des Hosts mit aktueller Zeit ab und löscht ihn beim Spielende. Der erste Test mit dem Namen `FTRHost_<zeit>.fos` endete mit getrennten Spawnpunkten und zwei Klicks auf „Weiter“, vermutlich weil das Spiel das Namensschema auswertet (Client-Log fehlt, Ursache unbelegt). Geladen wird über „Weiter“ im Hauptmenü, also ohne Spieladressen. Übertragung lokal getestet (identische Datei, falsches Passwort abgelehnt, keine Reste). Nicht getestet: zwei PCs, Firewall, ob „Weiter“ wirklich diese Datei nimmt, Steam Cloud (die Datei kann während des Spiels hochgeladen werden).
- **Stufe 2 (Teil):** `TP_FIGURE_MODE=save` schreibt beim Spielen mit eigenem Spielstand alle 60 s Inventar und SPECIAL nach `figure.bin`, `apply` legt sie 10 s nach dem Verbinden in der Host-Welt an (`UpdateFigure` in `DebugService.cpp`, Patch 0021). Kompiliert, nie ausgeführt. Die Datei enthält Server-Mod-IDs und passt daher nur bei gleicher Mod-Liste. `SetActorInventory` entfernt das Inventar der Host-Kopie. Fehlt noch: Aussehen (`TESNPC`), Stufe, Perks, Quest-Gegenstände der Kopie.
- **Automatisches Laden:** `BGSSaveLoadGame::LoadGame` ist OG `0xCDE9F0`, NG `0xBEE760` (gleiche Strings). Der NG-Weg läuft über `0xBF98D0` (Manager, Name, -1, 0, 1, 0). `BFD8E0` lädt aus Manager `+0x60`/`+0x68`, wenn Flag `0x4` in `+0x40` gesetzt ist. Der Setter ist noch nicht gefunden und in 1.10.163 ist der Aufbau anders, deshalb nicht verwendet.

## Stand 2026-10-30 abends: Autostart, Aussehen, Host-Ende (Patch 0022, im Spiel ungetestet)
- LAN-Test mit Host-Spielstand: beide am selben Ort (`New entity remotely managed` 19:59:02), kein Dump.
- **Autostart:** IAT-Hook auf `GetPrivateProfileStringA` (zwei Aufrufer in 1.11.240, `0x17F0905`, `0x17F097B`) liefert für `sStartingConsoleCommand` den Befehl `LoadGame "<name>"`, wenn `TP_AUTOLOAD` gesetzt ist. `LoadGame`/`load` steht im Befehlstabellen-Eintrag bei Datei-Offset `0x2EFA1C0`. Log: `Loading save on start`. Schlägt es fehl, bleibt das Spiel im Hauptmenü. Ob der Befehl zu diesem Zeitpunkt lädt, ist offen.
- **Aussehen und Name:** `figure.bin` enthält zusätzlich `TESNPC::Serialize` (mit `MarkChanged(0x2000800)`) und den Namen. `ApplyFigure` lädt das auf die Basisform des Spielers und ruft `QueueUpdate`. Danach `Close` und erneutes Verbinden, weil das Aussehen nur mit der Spawn-Anfrage übertragen wird. Alte Figuren ohne Aussehen bleiben lesbar, müssen aber neu geschrieben werden. Risiko: `Load` ändert die Basisform des laufenden Spielers.
- **Host-Ende:** Bei Verbindungsverlust in Starter-Sitzungen (`TP_AUTOCONNECT`) zeigt der Client eine Meldung und schließt das Fenster (`UpdateHostLost`). Der Verlust wird erst nach dem Timeout der Verbindung erkannt.

## Stand 2026-10-30 abends: Figur automatisch aus dem eigenen Spielstand (Patch 0023, im Spiel ungetestet)
Mit Host-Spielstand und „Eigene Figur mitnehmen“ startet der Starter das Spiel zweimal. Erster Start: `TP_FIGURE_MODE=capture`, `TP_AUTOLOAD` auf den eigenen Spielstand, der Client verbindet sich, schreibt `figure.bin` und schließt das Fenster. `figure.bin.source` merkt sich den Spielstand, ist die Figur neuer als er, entfällt dieser Start. Zweiter Start: Host-Spielstand mit `apply`. Zwei Starts statt zweimal Laden in einem Lauf, weil das Laden zur Laufzeit nur über ungetestete Engine-Adressen ginge. Figuren-IDs übernommener Host-Spielstände stehen in `hosts.txt`, `SaveShare.Newest` überspringt sie. Bleibt der erste Start im Menü hängen, weil das automatische Laden fehlschlägt, muss der Spieler dort selbst laden.

## Stand 2026-10-30 abends: Versionsabgleich und neuer Starter (Patch 0024)
LAN-Test bestätigt: Host-Spielstand, eigene Figur und Autostart laufen laut Spieler.
- **Versionsabgleich:** `BUILD_COMMIT` ändert sich bei unseren Builds nicht. Der Starter setzt deshalb `TP_BUILD` (Build-ID des Pakets) für Client und Server, `AuthenticationRequest.Version` und die Prüfung in `GameServer` nutzen ihn. Vor dem Spielstart fragt `SaveShare.CheckHost` über TCP Build und Spielversion ab (`FTR2 HELLO`). Abweichungen und nicht erreichbarer Host brechen ab. Lokal getestet: gleich, andere Build-ID, andere Spielversion, kein Host. Die Ablehnung durch den Server und `ShowRefusal` (Klartext für Version, Mods, Passwort, voll) sind nur kompiliert.
- **Starter:** Terminal-Oberfläche (`Pip`, `PipCheck`, `PipButton`, `StatusView`) mit Schrittliste, Spinner und Balken. Der Fortschritt im Spiel kommt aus `logs\tp_client.log` (`ReadLog`). Bewusst ohne Marken und Figuren von Bethesda.

## Bug 2026-10-30: Durch die Welt gefallen beim Respawn (Patch 0027, ungetestet)
`RespawnPlayer` teleportierte in die Zellenmitte per `GetCOCPlacementInfo`, ein Rest aus Skyrim. Jetzt steht der Spieler am Ort des Sturzes auf. Ganz ohne `MoveTo` blieb er schräg stehen und konnte nicht springen, weil Animationsgraph und Character Controller erst durch den Umzug zurückgesetzt werden. Deshalb `MoveTo` auf die eigene Position, 16 Einheiten höher (Patch 0028, Animation bestätigt). Danach fehlten noch Springen sowie AP- und Munitionsanzeige, weil nur das Bit des Lebenszustands gelöscht wurde. Patch 0029 rief dafür `Actor::Resurrect` (vtable-Slot 0xCC, zwei `bool`-Parameter, die Deklaration hatte nur einen), das ersetzt aber beim Spieler `currentProcess` und machte Waffe und Schießen kaputt. Richtig ist `PlayerCharacter::SetLifeState(0)` (Slot 0x105, OG `0xE9C0C0`, NG `0xD57980`, Patch 0030, im LAN-Test mit Build `6e3f0df8-7C197844` laut Nutzer bestätigt). Die 10 Sekunden Unverwundbarkeit aus `RunPostDeathUpdates` bleiben.

## Bug 2026-10-30: Client stirbt und landet im Hauptmenü (Patch 0025 Wächter, Patch 0026 Ursache)
Spieltest mit Patch 0026 bestanden: zweimal `Player is down`, Respawn laut Nutzer in Ordnung.
**Ursache (Patch 0026, Spieltest steht aus):** Das Spiel berechnet Bit 18 von `Actor+0x43C` neu (OG `0xD71E60`, ID 1085880, NG `0xC5B2B0`). Vorrang hat ein an der Referenz gespeicherter Wert (`ExtraData`, Abruf `0x292940`), dann das Flag der Basisform (`+0x70`, Bit 1), dann Aliase (Extra `0x88`). Echte Spielstände tragen nach dem Prolog vermutlich „nicht essential“, das schlägt unser Flag. Frische Testcharaktere haben den Wert nicht, deshalb lief der Respawn dort. `HookUpdateEssential` setzt das Bit nach der Berechnung neu, solange `g_keepPlayerEssential` gilt. Dass der Spieler den Wert wirklich trägt, ist aus dem Code abgeleitet, nicht im Speicher gemessen. Die erste Prüfung von Patch 0025 fragte zusätzlich die Basisform ab und meldete deshalb dauerhaft „Respawn mode was lost“, das `essential: true` im Log war wertlos, weil es nach dem Setzen gelesen wurde.

Client-Log des LAN-Tests: Figur eingesetzt und neu verbunden, dann Neuladen um 20:34:26 ohne Trennung und ohne Respawn-Eintrag. Die Figur starb also vermutlich regulär, statt über `RunRespawnUpdates` auszubluten. Der Schutz (`SetPlayerRespawnMode`) wird nur mit den Servereinstellungen beim Verbinden gesetzt, Neuladen und `TESNPC::Load` können ihn zurücksetzen. `RunRespawnUpdates` prüft ihn jetzt jede Sekunde und setzt ihn neu. Logzeilen: `Respawn mode was lost`, `Player is down`, `Player died outside of the respawn system`. Warum das Spiel ins Hauptmenü statt zum letzten Spielstand ging, ist offen.

## Stand 2026-10-30 nachts: Bereinigung auf `dev` (Spieltest steht aus)
Entfernt: Skyrim-Ziele und Skyrim-Client, `Code/skyrim_ui`, `Code/one_ui`, `Code/admin`, semantic-release, Docker, Upstream-Issue-Vorlagen und die Sentry-Upload-Aufgabe. Die Mauszeiger liegen jetzt in `Code/client/assets`.
Offen: `#if TP_SKYRIM` in rund 50 Dateien, Discord-Anbindung mit der Anwendungs-ID von Upstream, Konventionen und Namen im Code wie `TP_`, `Tilted`.

## Nicht getestet
- Regressionstest 1.10.163 mit den Next-Gen-Änderungen und dem Shutdown-Hook.

## Offene Aufgaben (nach Priorität)
1. **42 offene Adressen für 1.11.240** (`data/open-1-11-240-0.csv`): siehe Datei; außerdem 2 RTTI-Namen ohne Treffer (`data/rtti-1-11-240-0.missing.csv`).
   Ideen: Ähnlichkeitsvergleich (Aufrufer/Aufgerufene/Strings/Konstanten), Querverweise aus bekannten Funktionen,
   F4SE-NG-Quellen (benannte Adressen), manuell im Disassembler. Siehe docs/PORTING.md.
2. **Struktur-Offsets und VTable-Indizes** im Client für Next-Gen prüfen (Klassengrößen/Felder haben sich teils
   geändert; Referenz: CommonLibF4-NG). Erst im Spieltest sichtbar.
3. **Echter Zwei-Spieler-Test auf 1.10.163** (`-Clients 2` oder zwei PCs), längere Sitzung mit `-Hold`.
4. **F4SE für 1.11.240** (0.7.9+) im Spielordner; der Client lädt F4SE selbst, wenn vorhanden.
5. Upstream-Hinweis: Off-by-one-Fehler (docs/FINDINGS.md) betrifft nur Fallout; Skyrim nutzt den anderen Loader.

## Bekannte Probleme / Bugs
- Server schreibt bei jedem Start `mi_free: pointer does not point to a valid heap space` nach stderr, vor jedem Client. Ursache offen, bisher ohne Folgen.
- Begleiter werden nicht synchronisiert: In jedem Client muss der Begleiter einzeln angesprochen werden (Dogmeat getestet). Niedrige Priorität, Vorrang hat der Port auf die neueste Version.
- Spiel friert beim Beenden ein (einmal bei Red Rocket beobachtet, Exit-Code `0xCFFFFFFF`, Windows-Ereignis 1002). Im Spiel selbst nicht aufgetreten, Ursache offen. Ein früherer Lauf endete sauber mit Exit-Code 0.
- Launcher auf 1.11.240 ohne vollständige Offsets: bricht absichtlich mit Meldung ab und schreibt
  `logs\missing-offsets-<version>.txt`. Mit `TP_ALLOW_MISSING_OFFSETS=1` geht es weiter (Absturz erwartet).
- Server: beim harten Beenden meldet mimalloc `mi_free: pointer does not point to a valid heap space` (nur beim Kill).
- Server: Sentry-Init und Serverlisten-Ping schlagen fehl (egal für LAN).
- Launcher findet die Spielinstallation sonst über die Bethesda-Registry → für Testkopien immer `--exePath` angeben.
- `Code/client/main.cpp` öffnet bei fehlender Address Library eine Nexus-Seite (Upstream-Verhalten).

## Versionen
| | |
|---|---|
| Upstream-Basis | TiltedEvolution `d6ce567b` (2024-11-03) |
| Spiel Ziel | Fallout 4 1.11.240 (Steam Build 24564252), SHA-256 `fdcef37a…72f8` |
| Spiel Old-Gen | 1.10.163, Depot 377162 Manifest `5847529232406005096` |
| Zwischenversion für Validierung | 1.11.191, Depot 377162 Manifest `5433405173062582852` |
