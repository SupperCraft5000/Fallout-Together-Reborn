# Fallout Together Reborn

Inoffizieller Co-op für Fallout 4. Spielbar im LAN mit zwei Spielern auf **Fallout 4 1.11.240**.

*Unofficial co-op for Fallout 4, based on Tilted Online. Documentation is currently German only.*

> Dieses Projekt ist ein Fanprojekt. Es steht in keiner Verbindung zu Bethesda Softworks, ZeniMax, Microsoft oder Tilted Phoques.
> Fallout ist eine Marke ihrer jeweiligen Inhaber. Du brauchst eine eigene, gekaufte Kopie des Spiels. Dieses Repo enthält keine Spieldateien.

## Herkunft
Fallout Together Reborn ist ein Fork von [TiltedEvolution / Tilted Online](https://github.com/tiltedphoques/TiltedEvolution) von Tilted Phoques und baut auf deren letztem Stand mit Fallout-Unterstützung auf (`d6ce567b`). Die Upstream-Historie ist erhalten, die ursprüngliche README steht in [README-UPSTREAM.md](README-UPSTREAM.md). Upstream-Fragen gehören nicht zu Tilted Phoques, sondern hierher.

## Was geht
- Zwei Spieler sehen sich, Figuren, Quests, Gegner, Ausrüstung
- Einzel-exe mit eingebautem Server, Terminal-Oberfläche und Statusanzeige
- Spielstand des Hosts übernehmen, dabei die eigene Figur mit Aussehen, Inventar und SPECIAL mitnehmen
- Automatisches Laden des Spielstands, Versionsabgleich zwischen Host und Mitspieler
- Tod und Respawn mit echten Spielständen

## Bekannte Grenzen
- Getestet nur im LAN mit zwei Spielern auf 1.11.240
- 1.10.163 ist seit vielen Änderungen nicht mehr getestet
- Spiel über das Internet und längere Sitzungen sind nicht getestet
- Stufe und Perks der eigenen Figur werden nicht übernommen
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
| [AGENTS.md](AGENTS.md) | Einstieg für Mitwirkende und KI-Agenten |

## Lizenz
GPL-3.0-or-later, siehe [LICENSE](LICENSE) und [COPYING](COPYING). Änderungen gegenüber Upstream stehen in der Commit-Historie ab `d6ce567b`.
