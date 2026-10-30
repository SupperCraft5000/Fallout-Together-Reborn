# Ghidra-Bericht: fehlende Adressen für Fallout 4 1.11.240

Stand: 40 hoch, 1 mittel, 1 offen von 42. Keine Zuordnung wurde im laufenden Spiel getestet.

## Wichtigste Befunde
1. **`offsets-1-11-240-0.csv` enthält einen Fehler.** `1331728` (s_actorCtor2) steht auf `C57B60`. Das ist der Ctor mit Signatur `(this, uint8)`, also s_actorCtor. Richtig ist `1331728,C57F90`: Signatur `(this, int**, uint8)`, gleiche VTables, Aufruf aus 0x2E5110. Die Datei wurde nicht geändert.
2. **Der initWindowLoc-Patch läuft ins Leere.** `0x1825340` ist eine tote Kopie ohne Aufrufer oder Zeiger. Im Spiel ist InitWindows in renderInit `0x1816080` inline. Die wirksame Stelle ist `0x1816257` (`mov eax,0xC00000`), Patch auf `0x1816258`.
3. **`Renderer.cpp:94` schreibt an eine feste 1.10.163-Adresse.** `0x140000000 + 0x1D17EE8` liegt in 1.11.240 im Code einer fremden Funktion (Start 0x1D17EE0) und zerstört ihn. Der Block ist im Fallout-Build aktiv. Die Zeile muss vor dem Start ersetzt oder entfernt werden.
4. **`ctor` (994875) bleibt offen.** Siehe Inlining.
5. **Acht Code-Offsets stimmen nicht mehr.** Siehe Tabelle unten. Ohne Anpassung werden falsche Bytes überschrieben.

## Alle 42 Adressen
| Name | og_id | alte RVA | neue RVA | Sicherheit | Beleg |
|---|---|---|---|---|---|
| PerformComplexAction | 1445652 | 0xE211A0 | 0xCE9A70 | mittel | Rumpf gleich (Ähnlichkeit 0,92, Konstanten 0,95, Aufgerufene 4/4), aber keine direkten Aufrufer: eigenständige Kopie, im Spiel in performAction 0xCE97A0 inline. |
| performAction | 502376 | 0xE20FB0 | 0xCE97A0 | hoch | Aufrufer 5/5 gleich, Rumpf enthält nun PerformComplexAction inline (Größe 0xAF zu 0x199). |
| s_unequipAll | 1260317 | 0xD8E370 | 0xC78980 | hoch | Papyrus Actor.UnequipAll: Wrapper 0x10FC460 springt nach 0xC78980 (alt 0x138BFA0 nach 0xD8E370), Eingangscode mit +0xF8 und +0x78 gleich. |
| s_actorCtor | 1027500 | 0xD6E9A0 | 0xC57B60 | hoch | Gleiche Signatur (MOVZX ESI,DL), gleiche Aufrufstelle in SpawnNewREFR, Aufrufer 3/3, Aufgerufene 34/34. Diese RVA stand bisher fälschlich bei s_actorCtor2. |
| s_actorDtor | 1104082 | 0xD6F1C0 | 0xC58400 | hoch | Gleiche VTable-Referenz wie die Ctors, Aufrufer 2/2, Aufgerufene 11/12, Ähnlichkeit 0,86. |
| s_applyActorEffect | 703726 | 0xC8B180 | 0xBA0400 | hoch | VTable-Slots 18/18 gleich, Aufgerufene 18/19, Aufrufer 2/2. |
| s_runDetection | 906784 | 0xF60320 | 0xDF21E0 | hoch | Aufrufer 0xDF1990 passt inhaltlich zu alt 0xF5FBA0, Aufruf direkt vor derselben Folgefunktion, Aufgerufene 9/11, Referenzfolge 0,83. |
| initWindowLoc | 193854 | 0x1D17E10 | 0x1825340 | hoch | Rumpf fast gleich (0,99), gleiche Größe 0x21D, aber tote Kopie ohne Referenzen. Lebende Stelle inline in renderInit, siehe Code-Offsets. |
| pollInputDevices | 1328119 | 0x1B2BE30 | 0x1672540 | hoch | Aufrufer 1/1, Konstanten 0,93, Größe 0x5B zu 0x5C. |
| s_equipFunc | 1474878 | 0xE1EB50 | 0xCE7FB0 | hoch | Aufrufer equipObject 1/1, Aufgerufene 29/29, Referenzfolge 0,77. |
| s_unequipFunc | 1265292 | 0xE203A0 | 0xCE8FE0 | hoch | Aufrufer unequipObject 1/1, Aufgerufene 16/16, Referenzfolge 0,80. |
| s_registerSink | 57456 | 0xCD3980 | 0xBE3CC0 | hoch | Aufrufender Thunk 0xBDC8B0 zugeordnet, gleiches Dispatcher-Global, Ähnlichkeit 0,63. |
| s_unregisterSink | 1089537 | 0xCD6680 | 0xBE6710 | hoch | Aufrufender Thunk 0xBDC8D0 zugeordnet, gleiches Dispatcher-Global, Ähnlichkeit 0,64. |
| SetActive | 1567562 | 0x5D6AC0 | 0x66AE00 | hoch | Aufrufer 2/2, Aufgerufene 2/2, Referenzfolge 1,00. |
| mainMenuCtor | 1079380 | 0x129FF30 | 0x106E5F0 | hoch | Aufgerufene 38/39, Patchstellen mit gleicher Logik gefunden (Globals 0x2F3AEB9/BC, Engagement-Aufruf 0x1072400). |
| addTarget | 385035 | 0xC6C5E0 | 0xB8B460 | hoch | VTable MagicTarget Slot 1 gleich, Aufrufer 2/2, Aufgerufene 4/4. |
| checkAddEffectTargetData | 113033 | 0xC6D4F0 | 0xB8C940 | hoch | Einziger Aufrufer addTarget 1/1 an gleicher Stelle, Aufgerufene 14/15. Größe 0x52F zu 0xA00 durch Inlining. |
| s_setSize | 1293890 | 0x551440 | 0x60E430 | hoch | Code gleich aufgebaut (Grow-Pfad, 0x20ECB60 entspricht 0x26F0960), Aufrufer 2/2, 85 zu 83 Aufrufstellen. |
| s_launch | 1452334 | 0xFCA260 | 0xE421E0 | hoch | Aufrufer 3/3, Aufgerufene 23/27, von fire aus aufgerufen. |
| fire | 1056037 | 0x34D490 | 0x4799B0 | hoch | Aufrufer 1/1, Aufgerufene 24/26, ruft s_launch 0xE421E0 auf. |
| s_removeItem | 943233 | 0x3F9830 | 0x500BA0 | hoch | VTable-Slots 12/12 (TESObjectREFR Slot 109), Aufgerufene 53/54, Aufrufer 1/1. |
| s_registerPapyrusFunction | 919893 | 0x27338A0 | 0x2119EA0 | hoch | VTable BSScript::Internal::VirtualMachine gleicher Slot, Ähnlichkeit 0,60. |
| realSetLock | 157617 | 0xB7E10 | 0x2A3670 | hoch | Aufrufer 13/13, Code gleich (Bit 0 von +0x10, +0x14 nullen). |
| s_showSubtitle | 875508 | 0x12B2F70 | 0x107DE80 | hoch | Aufrufer 4/4, Aufgerufene 10/10, Referenzfolge 0,80. |
| s_greetDistance | 855965 | 0x371B168 | 0x2F0DD40 | hoch | Setting fAIMinGreetingDistance per Name, Startwert 150,0 gleich, Leser 0xD11170 entspricht alt 0xE4AC30. |
| s_vatsSelectTargetMult | 302826 | 0x372FAA0 | 0x2F226C0 | hoch | Setting fVATSTimeMultTargetSelect per Name, Startwert gleich, Leser 0xB16A40 entspricht alt 0xBD5860. |
| s_getLevel | 661617 | 0xD79E90 | 0xC633E0 | hoch | Code gleich (0x10 Byte), Aufrufer 28/28, Tail-Jump-Ziel zugeordnet. |
| setWeaponState | 835806 | 0xE22DF0 | 0xCEB0B0 | hoch | VTable ActorState Slot 33 gleich, Aufrufer 1/1, Bitlogik gleich. |
| s_setNoBleedoutRecovery | 925298 | 0xDFCAB0 | 0xCCD840 | hoch | Papyrus-Wrapper 0x1107CC0 springt nach 0xCCD840 (alt 0x1391510 nach 0xDFCAB0), Bit 5 von +0x43C gleich. |
| dispelAllSpells | 629902 | 0xC6B910 | 0xB8A510 | hoch | Aufrufer 2/2, gleiche Schleife über die Liste aus vcall +0x38, 0xC6ED20 neu inline (0x97 zu 0x138). |
| bGodMode | 806706 | 0x5AC7BB4 | 0x3300A9D | hoch | Alle 4 Zugreifer zugeordnet, Setter 0xD60000 (mov byte, ret) gleich. |
| knockExplosion | 803087 | 0xE83A60 | 0xD46AB0 | hoch | Aufrufer 1/1, Aufgerufene 26/28, Referenzfolge 0,82. |
| s_actorProcess | 1479787 | 0xD7CEB0 | 0xC66910 | hoch | Aufrufer 1/1, Aufgerufene 17/17, Ähnlichkeit 0,76. |
| setWeather | 1244028 | 0x651A60 | 0x6BFB60 | hoch | Ähnlichkeit 1,00, Aufrufer 1/1, Aufgerufene 1/1. |
| updateWeather | 1278859 | 0x64E530 | 0x6BC5E0 | hoch | Aufrufer 1/1, Aufgerufene 27/27, Ähnlichkeit 0,82. |
| addDeathItems | 708753 | 0xD70AF0 | 0xC59D90 | hoch | Ähnlichkeit 1,00, Größe 0xA3 gleich, Aufrufer 2/2. |
| s_device | 454121 | 0x61E0918 | 0x3A25D18 | hoch | Einziger direkter Zugriff in 0x1821C00 (zugeordnet) gleich, Nachbarfeld +0x10 wird wie alt von einer Init-Routine beschrieben. |
| ctor | 994875 | 0x123F00 | offen | - | Standalone-Ctor gibt es nicht mehr: alle Nutzer der VTable 0x2474CD0 bauen das Objekt inline. |
| s_viewportLock | 1549777 | 0x3846668 | 0x2F4C000 | hoch | Setting bBackgroundMouse:Controls per Name. Alt und neu zeigt die ID auf den Objektanfang, nicht auf den Wert (+8). |
| winMain | 668528 | 0xD35930 | 0xC2F910 | hoch | Größe gleich, Ähnlichkeit 1,00, ruft 0xC314F0 auf, das bAlwaysActive liest wie alt 0xD36D50. |
| bUseFaceGenPreprocessedHeads | 196396 | 0x3733CE0 | 0x2F25EC0 | hoch | Setting bUseFaceGenPreprocessedHeads:General per Name, Startwert 1, mehrere Zugreifer zugeordnet. |
| bAlwaysActive | 1420077 | 0x378E618 | 0x2F321B8 | hoch | Setting bAlwaysActive:General per Name, Zugreifer 0xC37C00 und 0xC314F0 entsprechen alt 0xD3C5A0 und 0xD36D50. |

Bewertung: hoch = mindestens zwei unabhängige Belege (Aufrufer, VTable-Slot, Papyrus-Wrapper, Setting-Name, Rumpfvergleich). Zahlen wie 5/5 bedeuten: von 5 zugeordneten Aufrufern oder Aufgerufenen der alten Funktion passen 5 in der neuen. Mittel = starker Beleg nur einer Art.

## Code-Offsets
| Stelle | Quelle | alt | 1.11.240 | Status |
|---|---|---|---|---|
| initWindowLoc dwStyle | BSGraphicsRenderer.cpp:80 | +0xD7+1 | +0xD7+1 nur in der toten Kopie; wirksam absolut `0x1816258` | **ändern** |
| dwStyle fest | Renderer.cpp:94 | `0x1D17EE8` | `0x1816258` | **ändern**, feste Adresse zerstört Code |
| timerLoc StopTimer | BSGraphicsRenderer.cpp:85 | +0xE | +0xE (`call 0x22AF2E0`, Ziel zugeordnet) | passt |
| mainMenuCtor Nop 14 | UI.cpp:307 | +0x279 | **+0x280** (`0x106E870`, gleiche Länge 14) | ändern |
| mainMenuCtor JZ zu JMP | UI.cpp:304 | +0x2DC | **+0x2E8** (`0x106E8D8`, `0F 84`) | ändern |
| mainMenuLoop Nop 0x29 | UI.cpp:311 | +0x28 | **+0x42** (`0x106FA62`, Länge 0x29 gleich) | ändern |
| mainMenuLoop Prompt | UI.cpp:320-322 | +0x51 | **+0x6B** (`0x106FA8B`, `movzx r8d` 8 Byte gleich) | ändern |
| handleInput Nop 10 | UI.cpp:325 | +0x56 | **+0x5A** (`0x1070DDA`, Länge 10 gleich) | ändern |
| processMenus SwapCall | UI.cpp:328 | +0xAD8 | **+0xBA2** (`0x1A811B2`, Ziel `0x1A82FD0` zugeordnet) | ändern |
| updateGameCursor JZ | BSInputDeviceManager.cpp:35 | +0xB1 | **+0xB5** (`0xC33765`, `74 2C`) | ändern |
| updateGameCursor JZ lang | BSInputDeviceManager.cpp:38 | +0xE1 | **+0xE5** (`0xC33795`, `0F 84`) | ändern |
| updateGameCursor ClipCursor | BSInputDeviceManager.cpp:41-42 | +0x27A | +0x27A (`FF 15`, Import ClipCursor geprüft) | passt |

Bytes und Befehle wurden in beiden exe verglichen. Die Offsets unterscheiden sich je Version, der Code braucht also eine Fallunterscheidung.

## Inlining
- **PerformComplexAction**: in performAction `0xCE97A0` inline. `0xCE9A70` ist eine eigenständige Kopie ohne direkte Aufrufer. Für den Aufruf in ForceAction geeignet, kein Hook-Ziel.
- **InitWindows (initWindowLoc)**: in renderInit `0x1816080` inline, Stelle `0x1816257`.
- **BGSLoadFormBuffer::ctor (994875)**: kein Standalone mehr. Der alte Rumpf ruft `D069A0(this)` und `D06340(this+0x28)` auf und setzt die VTable. Die Entsprechungen sind `0xC0F7B0`, `0xC0F380` und `0x2474CD0`. Der Client könnte das nachbauen, aber Objektgröße und Feldlayout in 1.11.240 sind nicht geprüft. Deshalb kein CSV-Eintrag.
- **Teilweise inline**: checkAddEffectTargetData (0x52F zu 0xA00), dispelAllSpells (0xC6ED20), s_unequipAll (0x88 zu 0x1E8), mainMenuLoop (0x1F6 zu 0x623). Die Einstiege bleiben gültig.
- **s_applyActorEffect**: direkte Aufrufstellen 9 zu 4, VTable-Slots aber 18/18. Ein Hook am Einstieg erfasst die virtuellen Aufrufe, einzelne direkte Stellen können inline gewandert sein.

## Weitere Auffälligkeiten
- **s_viewportLock**: `*s_viewportLock = ...` schreibt in 1.10.163 wie in 1.11.240 in das niederwertige Byte des VTable-Zeigers des Settings `bBackgroundMouse:Controls`, nicht in den Wert (+8). Die CSV überträgt das Verhalten 1:1 (`0x2F4C000`). Vermutlich ein alter Fehler; der Wert läge bei `0x2F4C008`. Nicht geändert.
- **Struktur-Offsets**: MainMenu-Felder sind verschoben (z. B. +0x268 zu +0x288, +0x27A zu +0x29B), ebenso ein Feld im Rückgabeobjekt von `0x1411C3F70` (+0xE4 zu +0x110). Das betrifft Client-Strukturen, falls sie diese Felder nutzen.
- **Prüfung der bisherigen Offsets**: Die Funktions-Einträge wurden gegen Aufrufer und Rumpf gehalten. Klar falsch war nur s_actorCtor2. Daten-Einträge wurden dabei nicht einzeln geprüft.
- **`out/map-240-plus.csv`**: enthält dieselbe Verwechslung (`0xD6ED80` zu `0xC57B60`) und ein weiteres falsches Paar auf `0xC57F90` (alt `0x12CBD0`).

## Vorgehen
- Ghidra 12.2 headless mit JDK 25 (Temurin 25.0.5), Prüfsummen geprüft. Keine volle Auto-Analyse: `.pdata` disassembliert, Funktionen angelegt, Listing exportiert (je rund 3 Minuten). Der Decompiler wurde nicht gebraucht; `Decomp.java` liegt für spätere Fragen bereit.
- Skripte in `Tools/port/ghidra/`. Arbeitsdaten in `$FTR_WORK/ghidra`, Arbeitskopie der neuen exe in `$FTR_WORK/exe-1.11.240`.
- Verkettete `.pdata`-Abschnitte wurden auf die Stammfunktion zurückgeführt. Die Ausbreitung nutzt zusätzlich Tail-Jumps, LEA-Funktionszeiger und VTable-Slots. Ergebnis: 48.681 Funktions- und 7.700 Datenpaare.
- Holdout mit 2.000 versteckten Paaren: 971 richtig, 2 falsch, 1.027 nicht wiedergefunden. Die Ausbreitung allein ist also nicht fehlerfrei, deshalb wurde jede der 41 Zuordnungen zusätzlich einzeln belegt.
