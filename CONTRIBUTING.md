# Mitmachen

Einstieg: [AGENTS.md](AGENTS.md), [docs/BUILD.md](docs/BUILD.md), [docs/STATUS.md](docs/STATUS.md).

## Bug melden
Bitte angeben:
- Build-ID, sie steht oben im Starter
- Fallout-4-Version, Host oder Mitspieler, gewählte Optionen
- Was passiert ist, was passieren sollte, Uhrzeit
- `tp_client.log` von dem PC, auf dem es passiert ist, möglichst auch vom anderen PC, siehe [docs/DEBUGGING.md](docs/DEBUGGING.md)
- Bei Abstürzen die Adresse aus der Meldung. Dumps sind rund 2 GB groß und können Speicherinhalte enthalten, also nicht öffentlich anhängen

Prüfe die Logs vor dem Hochladen. Spielstandnamen enthalten den Namen deiner Figur.

## Code beitragen
- Pull Requests bitte nach `dev`, nicht nach `main`, siehe [docs/REPOSITORIES.md](docs/REPOSITORIES.md)
- Regeln und Fallstricke stehen in [AGENTS.md](AGENTS.md)
- Code-Kommentare auf Englisch, Doku und Skripte auf Deutsch. Die englische Doku in `docs/en` spiegelt die deutsche, also beide pflegen
- Keine Spieldateien, exe, davon abgeleitete Dateien oder Address-Library-Dateien committen
- Bei Änderungen an Spieladressen den Bericht aus [docs/PORTING.md](docs/PORTING.md) beachten
