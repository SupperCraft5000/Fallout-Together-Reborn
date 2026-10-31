# Fallout Together Reborn

[English](README.en.md) | **Deutsch**

Inoffizieller Co-op für Fallout 4. Spielbar mit zwei Spielern auf **Fallout 4 1.11.240**.

> Dieses Projekt ist ein Fanprojekt. Es steht in keiner Verbindung zu Bethesda Softworks, ZeniMax, Microsoft oder Tilted Phoques.
> Fallout ist eine Marke ihrer jeweiligen Inhaber. Du brauchst eine eigene, gekaufte Kopie des Spiels. Dieses Repo enthält keine Spieldateien.

## Herkunft
Fallout Together Reborn ist ein Fork von [TiltedEvolution / Tilted Online](https://github.com/tiltedphoques/TiltedEvolution) von Tilted Phoques und baut auf deren letztem Stand mit Fallout-Unterstützung auf (`d6ce567b`). Die Upstream-Historie ist erhalten, die ursprüngliche README steht in [README-UPSTREAM.md](README-UPSTREAM.md). Upstream-Fragen gehören nicht zu Tilted Phoques, sondern hierher.

## Was geht
- Zwei Spieler sehen sich, Figuren, Quests, Gegner, Ausrüstung
- Einzel-exe mit eingebautem Server, Terminal-Oberfläche und Statusanzeige
- Verbindung per Adresse oder über das Steam-Netz ohne Portfreigabe
- Spielstand des Hosts übernehmen, dabei die eigene Figur mit Aussehen, Inventar und SPECIAL mitnehmen
- Automatisches Laden des Spielstands, Versionsabgleich zwischen Host und Mitspieler
- Tod und Respawn mit echten Spielständen
- Siedlungsbau: Bauen, Verschieben und Verschrotten erscheinen live und werden später Beitretenden nachgeliefert

## Bekannte Grenzen
- Getestet nur mit zwei Spielern auf 1.11.240
- 1.10.163 ist seit vielen Änderungen nicht mehr getestet
- Längere Sitzungen sind nicht getestet
- Stufe und Perks der eigenen Figur werden nicht übernommen
- Strom, Kabel, Betten, Generatoren und Siedler in Siedlungen werden nicht synchronisiert
- Nach einem Fallout-Update läuft es erst wieder, wenn die Adressen portiert sind, siehe [docs/PORTING.md](docs/PORTING.md)

## Dokumentation
| Datei | Inhalt |
|---|---|
| [docs/MITSPIELER.md](docs/MITSPIELER.md) | Spielen |
| [docs/BUILD.md](docs/BUILD.md) | Bauen unter Windows |
| [docs/STATUS.md](docs/STATUS.md) | Stand, offene Aufgaben, Bugs |
| [docs/TESTING.md](docs/TESTING.md) | Tests |
| [docs/PORTING.md](docs/PORTING.md) | Neue Fallout-Versionen |
| [docs/FINDINGS.md](docs/FINDINGS.md) | Erkenntnisse zur Engine |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Starter, Protokoll, Sitzungsablauf |
| [docs/DEBUGGING.md](docs/DEBUGGING.md) | Logs, Abstürze, Vorgehen bei Bugs |
| [docs/ENVIRONMENT.md](docs/ENVIRONMENT.md) | Umgebungsvariablen |
| [docs/HISTORY.md](docs/HISTORY.md) | Alte Patch-Nummern und Commits |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Bugs melden, mitmachen |
| [docs/REPOSITORIES.md](docs/REPOSITORIES.md) | Gitea, GitHub, Branches und Synchronisation |
| [AGENTS.md](AGENTS.md) | Einstieg für Mitwirkende und KI-Agenten |

## Lizenz
GPL-3.0-or-later, siehe [LICENSE](LICENSE) und [COPYING](COPYING). Komponenten von Dritten: [THIRD-PARTY.md](THIRD-PARTY.md). Änderungen gegenüber Upstream stehen in der Commit-Historie ab `d6ce567b`.
