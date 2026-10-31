# AGENTS.md – Einstieg für KI-Agenten und neue Mitwirkende

Lies zuerst [docs/STATUS.md](docs/STATUS.md) (was geht, was offen ist), dann je nach Aufgabe
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/DEBUGGING.md](docs/DEBUGGING.md), [docs/PORTING.md](docs/PORTING.md) oder [docs/BUILD.md](docs/BUILD.md).

## Projekt in Kürze
- Fallout Together Reborn: Co-op-Mod für Fallout 4 auf Basis von Fallout Together (TiltedEvolution, C++20, xmake, Windows x64).
- Architektur: `FalloutTogether.exe` (Immersive Launcher) lädt `Fallout4.exe` selbst in seinen Prozess,
  installiert Hooks und startet das Spiel. Der Client-Code (`Code/client`) ist per `/WHOLEARCHIVE`
  eingebettet. Server: `FalloutTogetherServer.exe` + `FTServer.dll`, Netzwerk über
  GameNetworkingSockets (TiltedConnect), Nachrichten in `Code/encoding`.
- Spieladressen: Jede genutzte Engine-Funktion/-Variable hat eine **Old-Gen-Address-Library-ID** (1.10.163)
  im Code (`POINTER_FALLOUT4(Typ, name, ID)` bzw. `VersionDbPtr<T> x(ID)`). Für andere Versionen löst
  `VersionDb` die IDs über `Data\F4SE\Plugins\FalloutTogether\offsets-<version>.bin` auf.

## Harte Regeln
1. **Original-Spielinstallation nie verändern.** Tests laufen in einer Testkopie (Hardlinks für große Daten).
   Hardlinks **nie beschreiben**, nur löschen und ersetzen (sonst ändert man das Original mit).
2. Keine Spieldateien, exe, davon abgeleitete Dateien, Address-Library-`.bin` oder Fremd-Binaries committen.
3. IDs im Code sind **echte** Old-Gen-IDs (seit Patch 0002). Neue Adressen immer als 1.10.163-ID eintragen;
   die Portierung erzeugt die Offsets pro Version.
4. Jede neue Adress-Zuordnungsheuristik mit `Tools/port/validate.py` (Kreuzvalidierung) prüfen.
   Falsche Adressen sind schlimmer als fehlende: lieber verwerfen als raten.
5. Skyrim-Ziele und der Skyrim-Client sind entfernt. Die `#if TP_SKYRIM`-Blöcke im gemeinsamen Code werden nicht übersetzt und sind Altlast. `Code/encoding/Structs/Skyrim` enthält nur noch vier Header mit Animations-Kennungen, die der Fallout-Code einbindet.

## Fallstricke (kosten sonst Stunden)
- xmake **2.9.5** benutzen (wie CI des Projekts) und das Paket-Repo auf `978015f8b` pinnen; neuere Pakete brechen
  den Link (mimalloc 3.x, abseil/protobuf). Details in docs/BUILD.md.
- `set_policy("package.precompiled", false)` ist nötig, sonst kommen inkompatible vorkompilierte Pakete.
- MSVC 14.44 wird von xmake 2.9.5 als Toolset `v144` erkannt → Patch in `toolchain.lua` (docs/BUILD.md).
- Die Old-Gen-`Fallout4.exe` (1.10.163) ist nicht direkt statisch analysierbar, Next-Gen-exe schon.
- CommonLibF4-NG-IDs für **VTables** sind teilweise falsch (mehrere Klassen auf derselben NG-ID).
  Für VTables RTTI verwenden, CommonLibF4 nur für Funktionen als Wahrheit nehmen.
- Werkzeuge in Sandboxes/Containern: GUI-Prozesse (Launcher, Spiel) können dort unsichtbar hängen.
  Spieltests aus einer normalen Benutzer-Shell starten.

## Typische Aufgaben
| Aufgabe | Wo anfangen |
|---|---|
| Neue Fallout-Version unterstützen | docs/PORTING.md → `Tools/port/pipeline.py` |
| Offene Adressen finden | `data/open-1-11-240-0.csv`, docs/PORTING.md „Offene Adressen“ |
| Netzwerk/Server-Bug | `Code/server`, `Code/encoding`, Test mit `scripts/test-server-bots.ps1` |
| Spielseitiger Bug | `Code/client/Games/Fallout4`, `Code/client/Services` |
| Neue Nachricht im Protokoll | `Code/encoding/Messages`, Opcodes in `Code/encoding/Opcodes.h`, Bot erweitern (`Code/bot`) |

## Konventionen
- Code-Kommentare im TiltedEvolution-Code auf Englisch (wie upstream), Doku und Skripte auf Deutsch. `docs/en` spiegelt die deutsche Doku, also beide pflegen.
- Entwicklung auf `dev`, `main` ist der getestete Stand. Gitea und GitHub bleiben gleich, siehe [docs/REPOSITORIES.md](docs/REPOSITORIES.md).
- Commits mit aussagekräftiger Nachricht. `patches/` enthält nur den Patch für das Submodul TiltedHooks.
