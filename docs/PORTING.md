# Spieladressen auf neue Fallout-4-Versionen portieren

## Warum
Der Client hookt und ruft Engine-Funktionen über Adressen. Next-Gen (ab 1.10.980) wurde neu kompiliert, die
Address Library nutzt dort eine **neue ID-Nummerierung**, und für neue Patches (z. B. 1.11.240) gibt es oft
lange keine Address Library. Deshalb portieren wir selbst, direkt aus den exe-Dateien.

## Wie der Client Adressen lädt (Patch 0002, `Code/client/VersionDb.h`)
- 1.10.163: `Data\F4SE\Plugins\version-1-10-163-0.bin` (Address Library).
- andere Versionen: `Data\F4SE\Plugins\FalloutTogether\offsets-<a-b-c-d>.bin`, Layout wie die Old-Gen-Library
  (`u64 Anzahl`, dann `u64 ID, u64 RVA`), aber nach **Old-Gen-ID** geschlüsselt.
- Jede `VersionDbPtr` meldet ihre ID an. Fehlen IDs, schreibt der Client `logs\missing-offsets-<version>.txt`
  und bricht ab (außer `TP_ALLOW_MISSING_OFFSETS=1`).

## Ablauf bei einem neuen Fallout-Patch
```powershell
# 1. einmalig: eine lokal analysierbare 1.10.163-exe als Fallout4.analysis.exe bereitstellen (nicht Teil des Projekts)
# 2. optional: eine Next-Gen-Zwischenversion mit Address Library (z. B. 1.11.191) als zweiter Weg
python -I Tools\port\pipeline.py --ids data\ids-1.10.163.csv --og-exe Fallout4.analysis.exe `
    --new "<Steam>\Fallout 4\Fallout4.exe" --mid-exe Fallout4-1.11.191.exe --mid-anchors anchors-163-191-func.csv `
    --version 1-11-240-0 --install "<Testkopie>"
```
Dauer: Sekunden. Ergebnis `offsets-<version>.csv` + `.bin` im Spielordner.

## Ghidra-Ablauf für schwierige Adressen
Für Adressen, die `graphport.py` nicht findet. Braucht `FTR_WORK`, `FTR_GHIDRA` und `FTR_JAVA_HOME` ([ENVIRONMENT.md](ENVIRONMENT.md)) sowie `Fallout4.analysis.exe` in `%FTR_WORK%\exe-1.10.163` und `Fallout4.exe` in `%FTR_WORK%\exe-1.11.240`.

```powershell
Tools\port\ghidra\run-dump.ps1 old     # Listing aller Funktionen -> %FTR_WORK%\ghidra\dump\old.txt
Tools\port\ghidra\run-dump.ps1 new
python Tools\port\ghidra\lst.py        # Blattfunktionen ohne .pdata auflisten
Tools\port\ghidra\run-extra.ps1 old <in> <out>   # deren Listings nachladen, ebenso new
python Tools\port\ghidra\prop.py       # Zuordnung ausbreiten -> ext.pkl
python Tools\port\ghidra\final.py      # offene Adressen bewerten -> final.pkl
python Tools\port\ghidra\report.py     # data\manual-ghidra-*.csv und Bericht
```
Startpunkt ist `data/out/map-240-plus.csv`, die Zuordnung alt nach neu. Einzelne Funktionen dekompiliert `run-decomp.ps1 new|old <liste> <ausgabe>`, Vergleich zweier Funktionen `cmpf.py alt:neu`.
Für eine andere Fallout-Version müssen die Versionsnummern in `fdis.py`, `run-dump.ps1` und `ctx.py` angepasst werden.

Grenze dieses Repos: Die Analyse setzt eine lesbare 1.10.163-exe voraus, die hier nicht enthalten ist. Fertig portierte Adressen für 1.11.240 stehen in `data/final-1-11-240-0.csv`.

## Werkzeuge (`Tools/port`, nur Python-Standardbibliothek)
| Skript | Zweck |
|---|---|
| `inventory.py` | alle IDs im Code mit echter ID und 1.10.163-Adresse → `data/ids-1.10.163.csv` |
| `build_map.py` | OG→NG-ID-Paare aus CommonLibF4 (`RelocationID(og, ng)`) → `data/og_to_ng.csv` |
| `sigport.py` | Byte-Signatur am Funktionsanfang, rel32-Ziele als Joker, Suche nur an `.pdata`-Funktionsanfängen, eindeutiger Treffer nötig |
| `graphport.py` | Hauptverfahren, siehe unten |
| `make_offsets.py` | baut `offsets-<version>.bin` aus NG-Address-Library + Zuordnung und/oder CSV `og_id,rva` |
| `validate.py` | misst Treffer- und Fehlerquote gegen CommonLibF4 + NG-Address-Library |
| `pipeline.py` | alles zusammen, zwei Wege, verwirft Widersprüche |

### graphport.py – Verfahren
1. **String-Anker**: Funktionen, die als einzige einen bestimmten C-String per RIP-relativer Adresse laden
   (gleicher Text in beiden Versionen). Ca. 6.300 Anker.
2. Optional: Signatur-Treffer (`--sig`), gesicherte Paare (`--anchors`), VTable-Slots (`--vtable-anchors`).
3. **Ausbreitung** (12 Runden): Für zugeordnete Funktionspaare werden Aufrufziele (`call rel32`) und
   RIP-Datenzugriffe in Reihenfolge gepaart; bei ungleicher Länge über bekannte Paare als Stützpunkte nur
   gleich lange Lücken. Stimmen sammeln, Mehrdeutiges verwerfen, eine neue Funktion gehört nur einer alten.
4. **Lückenfüllung** (`--gaps N`): Zwischen zwei benachbarten Zuordnungen mit gleich vielen Funktionen
   dazwischen (≤ N) nach Reihenfolge paaren (Linker-Reihenfolge bleibt oft erhalten).
5. **VTables über RTTI**: CompleteObjectLocator → TypeDescriptor-Name + Offset → exakte Zuordnung.
6. Stellen mitten in Funktionen (Hook-Punkte): 12 Bytes ab der Stelle in der zugeordneten neuen Funktion suchen.

## Validierung (1.10.163 → 1.11.191)
Wahrheit: CommonLibF4-Paare (OG-ID, NG-ID) + `version-1-11-191-0.bin` → 2.066 Paare. Davon sind 896 VTables,
98 Funktionsanfänge.
- Ohne Anker: 106 richtig, 1 falsch.
- Kreuzvalidierung (Hälfte als Anker, Hälfte als Test), nur Funktionen: 49 richtig, **0 falsch**, 49 offen
  (`--gaps 4 --vtable-anchors`). Mit `--gaps 16` kamen 2 Fehler → Standard bleibt 4.
- VTables: RTTI widerspricht CommonLibF4 in vielen Fällen; Gegenprobe mit den Namen aus
  alandtse/fallout_vr_address_library bestätigt RTTI. **CommonLibF4-NG-VTable-IDs sind teilweise falsch.**

## Ergebnis 1.11.240
- Benötigt (Fallout-Build, ohne `Games/Skyrim`): 133 IDs.
- Gefunden: 80 (`data/offsets-1-11-240-0.csv`). Weg A (direkt) und Weg B (über 1.11.191) stimmen in allen
  77 gemeinsamen Zuordnungen überein.
- Offen: 53 (`data/open-1-11-240-0.csv`): 46 Funktionen, 7 Daten.

## Offene Adressen – Ideen
- Ähnlichkeitsmaß pro Funktion: Menge zugeordneter Aufrufer/Aufgerufener/Datenziele/Strings/Immediates,
  Kandidaten aus der Nachbarschaft zugeordneter Funktionen, Jaccard mit Abstand zum Zweitbesten.
- Globale Variablen: Zugriffe aus bereits zugeordneten Funktionen an gleicher Stelle (Instruktionsreihenfolge).
- F4SE-NG-Quelltext (`f4se/Game*.cpp`) enthält benannte RVAs für 1.11.x → mit Namen aus
  `fo4_database.csv` (alandtse/fallout_vr_address_library) verknüpfen.
- Manuell: Ghidra/IDA mit beiden exe, Bericht `*.report.csv` zeigt alte RVA und Name.
- Jede neue Heuristik vorher mit `validate.py` gegen 1.11.191 messen.

## Wichtig zum Format der Old-Gen-Address-Library
`version-1-10-163-0.bin` = `u64 Anzahl`, dann `u64 ID, u64 RVA`. Der ursprüngliche Fallout-Together-Code las
`(RVA, ID)` ab Byte 0 – siehe docs/FINDINGS.md.

## Nachtrag: fuzzport.py und sitevote.py
`graphport.py --dump-map` schreibt alle Paare. `fuzzport.py` (Ähnlichkeit der Nachbarschaft) und `sitevote.py` (Abstimmung an Verwendungsstellen) bauen darauf auf.
Holdout mit 2.000 versteckten Funktionen: fuzzport streng 670 richtig, 4 falsch; locker 1.098 richtig, 10 falsch. sitevote: 204 richtig, 0 falsch; Daten 357 richtig, 6 falsch.
Ergebnis: 11 weitere Adressen, Stand 97 von 139. Das sind einstufige Treffer ohne zweiten Weg, also als unsicherer einzustufen. Weitere Durchläufe bringen nichts mehr, der Rest braucht Handarbeit im Disassembler.
