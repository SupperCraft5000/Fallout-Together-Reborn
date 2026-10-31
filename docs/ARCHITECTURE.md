# Starter und Sitzungsablauf

Überblick über den Mod selbst: [AGENTS.md](../AGENTS.md). Hier steht, was zwischen Starter, Client und Server passiert.

## Bestandteile
- **Starter** `package/Starter.cs` (C# 5, WinForms): Einzel-exe, enthält Client und Server als `payload.zip`.
- **Client** `FalloutTogether.exe`: lädt `Fallout4.exe` in den eigenen Prozess.
- **Server** `FalloutTogetherServer.exe` + `FTServer.dll`: läuft beim Host unsichtbar, UDP 10578.
- **Starter-Dienst** beim Host: TCP 10578, Klasse `SaveShare`.

## Daten auf dem PC
Alles unter `%LOCALAPPDATA%\FalloutTogetherReborn`:

| Pfad | Inhalt |
|---|---|
| `<build>\` | entpackte Version, `.complete` markiert sie als vollständig |
| `<build>\logs\tp_client.log` | Client-Log, der Starter liest es für die Statusanzeige mit |
| `<build>\crash_*.dmp` | Absturz-Dump |
| `settings.txt` | Adresse und Spielordner |
| `figure.bin` | eigene Figur |
| `figure.bin.source` | Spielstand, aus dem die Figur stammt |
| `hosts.txt` | Figuren-IDs übernommener Host-Spielstände, die automatische Wahl überspringt sie |
| `logs-previous\` | letzte Logs der zuletzt gelöschten Version |

Ältere Versionsordner löscht der Starter beim Start (`Payload.RemoveOldBuilds`), solange kein Spiel oder Server läuft.

## Protokoll des Starter-Dienstes
Zeilenbasiert über TCP, eine Anfrage pro Verbindung:

| Anfrage | Antwort |
|---|---|
| `FTR2 HELLO` | `<build> <spielversion>` |
| `FTR2 SAVE <passwort>` | `OK <größe> <dateiname>`, dann die Datei, oder `NO` |

## Start beim Host
1. `Payload.Prepare` entpackt, alte Versionen werden entfernt.
2. Server und Starter-Dienst starten, beide mit `TP_BUILD`.
3. Das Spiel lädt den gewählten Spielstand und verbindet sich mit `127.0.0.1`.

## Start beim Mitspieler
1. `CheckHost`: Build und Spielversion müssen gleich sein, sonst Abbruch vor dem Spielstart.
2. `Fetch`: Der Host-Spielstand wird geladen, Name wie beim Host, aber mit aktueller Zeit. Das Spiel wertet den Dateinamen aus.
3. Mit **Eigene Figur mitnehmen** zwei Starts:
   - Erster Start: `capture` mit dem eigenen Spielstand. Client verbindet sich, nach 10 s wird `figure.bin` geschrieben, das Fenster schließt sich. Entfällt, wenn die Figur neuer als der Spielstand ist.
   - Zweiter Start: `apply` mit dem Host-Spielstand. Nach 10 s setzt `ApplyFigure` Aussehen, Name, Inventar und SPECIAL ein, dann verbindet der Client neu, weil das Aussehen nur mit der Spawn-Anfrage übertragen wird.
4. Beim Spielende löscht der Starter die Kopie des Host-Spielstands.

Warum zwei Starts: Das Laden zur Laufzeit ginge nur über nicht portierte Engine-Adressen. Warum 10 s Wartezeit: Die Figur enthält Server-Mod-IDs, die erst nach dem Verbinden gelten. Sie passt deshalb nur bei gleicher Mod-Liste.

## Steam-Tunnel
`package/Tunnel.cs`. Der Starter startet sich selbst mit `--tunnel` als eigenen Prozess, weil der Tunnel schon vor dem Spiel gebraucht wird. Spiel und Server sprechen weiter UDP mit einem Port auf 127.0.0.1, der Starter-Dienst TCP.

| Kanal | Inhalt |
|---|---|
| 0, unzuverlässig | UDP-Datagramme mit 2 Byte Kennung, dem Quellport des Spiels |
| 1, zuverlässig | TCP-Ströme als Rahmen: Typ (Öffnen, Daten, Ende), Strom-ID, Daten |

Beim Host gehört zu jeder Kennung ein eigener UDP-Socket. Wäre es derselbe, hielte der Server ein neues Spiel für die alte Sitzung, wenn deren Ende nicht ankam, etwa nach dem Doppelstart mit eigener Figur.

Nur das Relay wird genutzt (`P2P_Transport_ICE_Enable=0`), weil `steamwebrtc64.dll` im Test abstürzte. Der Host nimmt jede Sitzung an, Schutz ist das Passwort. Spiel läuft nur über `SteamLink`, `ITunnelLink` ersetzt Steam im Test durch eine künstliche Leitung.

## Autostart
Der Hook auf `GetPrivateProfileStringA` in `Code/client/FalloutVM.cpp` liefert für `sStartingConsoleCommand` den Befehl `LoadGame "<name>"`. Schlägt das fehl, bleibt das Spiel im Hauptmenü.

## Tod und Respawn
1. `SetPlayerRespawnMode` setzt Essential. `HookUpdateEssential` hält das Bit, weil das Spiel es sonst neu berechnet.
2. Das Spiel setzt den Lebenszustand 7/8, `RunRespawnUpdates` bemerkt das und wartet 5 s.
3. `RespawnPlayer`: `SetLifeState(0)` entfernt Eingabesperre und Kamerazustand, `MoveTo` auf die eigene Position setzt Animation und Physik zurück.
4. Danach 10 s Unverwundbarkeit.

Hintergrund: [FINDINGS.md](FINDINGS.md), Abschnitt 20.

## Versionsprüfung
Zwei Stufen: `CheckHost` vor dem Spielstart und `GameServer` beim Verbinden, beide über die Build-ID des Pakets. `BUILD_COMMIT` allein reicht nicht, weil es bei unseren Builds gleich bleiben kann.
