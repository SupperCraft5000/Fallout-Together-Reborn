# Entdeckungen und Fallstricke

Verweise wie `patches/0014` sind Commits, siehe [HISTORY.md](HISTORY.md).

## 1. Off-by-one beim Lesen der Address Library (Fallout Together, upstream)
`VersionDb::LoadFallout4` las `version-1-10-163-0.bin` ab Byte 0 als Paare `(Adresse, ID)` und ignorierte die
letzten 8 Byte („strangely has no ID“). Tatsächlich ist das Format `u64 Anzahl`, dann `(u64 ID, u64 Adresse)`.
Folge: Jede ID bekam die Adresse des **vorherigen** Eintrags. Die Entwickler haben IDs passend dazu gewählt,
deshalb funktionierte es – aber alle IDs im Code waren „echte ID + 1“ (z. B. 633525 statt 633524 =
`Main::OnIdle`). Darum passte keine einzige ID zu Community-Zuordnungen (CommonLibF4, VR-Datenbank).
Fix: Patch 0002 (alle 159 Stellen, Loader korrigiert). Für 1.10.163 nachweislich identische Adressen.

## 2. Next-Gen ist neu kompiliert
Byte-Signaturen vom Funktionsanfang finden 1.10.163 → 1.11.240 nur 20 von 153 Adressen. Aufrufgraph +
String-Anker + RTTI funktionieren deutlich besser (80 von 133 benötigten).

## 3. Analyse der Old-Gen-exe
Die Steam-exe von 1.10.163 ist für die statische Analyse nicht direkt lesbar, Next-Gen-exe (1.11.x) dagegen schon. Wie man das lokal löst, ist nicht Teil dieses Projekts. Abgeleitete Dateien der exe gehören nie ins Repo.

## 4. CommonLibF4-NG-VTable-IDs teils falsch
Mehrere Klassen teilen dieselbe NG-ID (z. B. vier `AutoRegister…`-VTables → `2731340`), und RTTI zeigt eine
andere Klasse als erwartet. Für Funktionen sind die Paare zuverlässig.

## 5. Build-Umgebung
Siehe docs/BUILD.md (xmake 2.9.5, Paket-Pin, precompiled aus, MSVC-14.44-Toolset, lange Pfade, DirectXTK).

## 6. Launcher
- Sucht die Installation über `HKLM\Software\Wow6432Node\Bethesda Softworks\Fallout4` bzw. HKCU-TiltedPhoques-Key;
  `--exePath` überschreibt und speichert in HKCU.
- Lädt `Fallout4.exe` in einen reservierten Bereich (`exeLoadSz` 0x70000000; 1.11.240 braucht 0x425A000).
- Lädt F4SE selbst (`f4se_<version>.dll` aus dem Spielordner), falls vorhanden.
- In Container/Sandbox-Umgebungen (z. B. App-Container) hängt er vor dem ersten Fenster (Wartezustand LpcReply);
  selbst eine einfache MessageBox erscheint dort nicht. Normal aus der Benutzer-Shell starten.

## 7. Spielprofil
INIs/Spielstände (`Dokumente\My Games\Fallout4`) und `plugins.txt` (`AppData\Local\Fallout4`) teilen sich alle
Installationen. Für Tests Profil tauschen (`scripts/test-profile.ps1`).

## 8. Downgrade-Daten
Depot-Manifeste für 1.10.163 aus zerratar/fallout4-downgrader (`DepotManager.cs`). Next-Gen-only
Creation-Club-Dateien und NG-F4SE-Plugins müssen in der Testkopie beiseitegelegt werden.
`xSE Plugin Preloader` (`winmm.dll`) ebenfalls deaktivieren.

## 9. Fallout 4 importiert kein dinput8.dll
`DInputHook::Install` (Submodul TiltedHooks) kopiert den Funktionsanfang von `DirectInput8Create`. Fallout 4 importiert
die DLL nicht (weder 1.10.163 noch 1.11.240), der Zeiger ist also null: Absturz beim Start des Clients.
Fix: `patches/0003` (im Ordner `Libraries/TiltedHooks` anwenden: `git apply -p3 <patch>`).
Absturzadressen auflösen: `Tools/port/addr2sym.py`.
Das Basis-Offset ist `Adresse - 0x140000000`, Adressen ab `0x1B0000000` gehören zum Launcher/Client,
darunter zum Spiel. WER-Dumps liegen in `%LOCALAPPDATA%\CrashDumps`.

## 10. Sechs IDs wurden bei der Umstellung übersehen
`inventory.py` erkannte `DECLARE_DISPATCHER(Name, id)` und `internal::DynamicCast(id)` nicht. Diese sechs IDs blieben
„echte ID + 1“ und zeigten mit dem korrigierten Loader auf falsche Einträge. Folge: Absturz kurz nach dem Start mit
RIP in `.pdata` (0x146A31CA8 = ID 823571). Behoben in `patches/0004`, `data/ids-1.10.163.csv` ist ergänzt.
Die Offsets für 1.11.240 enthalten diese sechs IDs noch nicht: Benötigt sind jetzt 139 statt 133.
`inventory.py` erwartet die alten IDs, also nicht auf dem korrigierten Code erneut ausführen.

## 11. Auch 3.235 RTTI-IDs waren betroffen
`registerRtti_*` in `Games/Fallout4/RTTI.cpp` nutzen ebenfalls Address-Library-IDs. Mit alten IDs lieferte `Cast<T>`
null, Absturz in `CharacterService::OnActorAdded` (`pActor->GetExtension()`). Behoben in `patches/0005`; alle
3.235 Einträge wurden gegen den Typnamen im TypeDescriptor der 1.10.163-exe geprüft.
Folge für 1.11.240: Diese IDs fehlen in den Offsets noch. Sie lassen sich über den Typnamen zuordnen (`.?AV…@@`),
müssen aber in die Pipeline. Der Client meldet sie sonst als fehlend.

## 12. Ghosting kam von der Testzelle QASmoke
In `QASmoke` schlierte das Bild stark, bei Red Rocket (`coc RedRocketExt`) nicht. TAA, Motion Blur und das Overlay
waren nicht die Ursache, die Tests dazu änderten nichts. Testprofil startet deshalb bei Red Rocket.
`iPresentInterval=3` ergab einen schwarzen Bildschirm (Ursache nicht untersucht), den Wert auf 1 lassen.
Diagnose: `test-ingame.ps1 -NoOverlay` setzt `TP_NO_OVERLAY_RENDER`, `-Hold` lässt das Spiel offen.
Für Screenshots: Das Fenster heißt „Fallout Together“, Computer-Use findet es nicht als App.

## 13. Quests synchronisieren nur in einer Party, und es gab keine Party
`QuestService` sendet nur als Party-Leader, der Server verteilt nur an die Party. Die Party-Oberfläche fehlt im Fallout-Build
(kein `UI`-Ordner), `bAutoPartyJoin` greift erst, wenn schon eine Party besteht. Also blieb jeder Spieler für sich.
`patches/0007`: Der erste Spieler bekommt automatisch eine Party und ist Leader, weitere treten bei.
Nur der Fortschritt des Leaders wird übertragen. Im Bot-Test bestätigt (Nachrichten 11 und 9), im Spiel noch ungetestet.

Zwei-Spieler-Test (2026-10-30, -Clients 2): Beide verbunden und einander sichtbar. Beide Clients schreiben in dieselbe
`tp_client.log`. Noch offen im Log: `MoveActor: failed to fetch cell` (22x), `Character with remote id 6 is already spawned`,
`Failed to retrieve Actor 60BFE`.

## 14. „Aufgaben“ (Miscellaneous) wurden gefiltert
`QuestService::IsNonSyncableQuest` blockierte alle Quests vom Typ Miscellaneous. In Fallout 4 sind das auch die Einträge unter
„Aufgaben“ im Pip-Boy. `patches/0008`: Miscellaneous wird nur noch ohne Objectives blockiert. Interne Quests wie 11679D feuern
sonst im Sekundentakt. Client-Log zeigt jetzt Typ und Objectives, Server-Log die Weiterleitung. Im Spiel ungetestet.

## 15. Next-Gen-exe wurde vom Launcher beschädigt
Der Immersive Launcher behandelte die 1.11.240-exe wie die alte Version und zerstörte dabei `.text`, Absturz direkt hinter dem Original-Einsprungpunkt.
Behoben in `patches/0011`. Dazu lief `VectoredExceptionHandler` in eine Endlosrekursion (Mutex-Ausnahme beim erneuten Eintritt), bis der
Stack voll war, darum gab es keinen Dump. Ebenfalls behoben.

Nachtrag zu 15: Der erste Fix wirkte nicht, weil im Header-Struct das Feld `SteamAppId` in einem Kommentar verschwunden war. `Flags` las dadurch die App-ID.
Außerdem startete der Client auf Next-Gen nie: Der Start-Hook hing an `_ismbblead` aus MSVCR110, das die 1.11.240-exe nicht importiert. Jetzt `GetStartupInfoW` wie bei Skyrim (`patches/0012`).

Zweiter Nachtrag zu 15: Auch `patches/0012` griff nicht. Die Weiche prüfte, ob `msvcr110.dll` geladen ist, und `GFSDK_GodraysLib.x64.dll` lädt sie auch unter 1.11.240.
Folge: Kein Hook griff, `BeginMain` lief nie, und der erste Fenster-Callback in `InputService::WndProc` stürzte über den Nullzeiger `s_pOverlay` ab. Die Weiche richtet sich jetzt nach den Importen der exe (`patches/0013`).

## 16. Struktur-Offset: Client lief auf Next-Gen nicht, Absturz beim Beenden
Das Flag `VMContext::inactive` liegt in 1.11.240 bei 0x610 statt 0x600 (Dekompilat von `cVMUpdate` beider Versionen). Mit dem alten Offset rief `HookVMUpdate` `Update()` nie auf: Das Spiel lief, aber ohne Client, und es gab kein „Auto connect“. Beleg im Dump: `bAlwaysActive` war 0 und `bUseFaceGenPreprocessedHeads` 1, obwohl `Update()` beide jedes Bild setzt.
Der Absturz beim Beenden (`0x4CB273`, Nullzeiger über die globale Variable `0x30EB310`) liegt im Engine-Shutdown `Main::Shutdown`, dritter Aufruf in WinMain (OG-ID 234348, 1.11.240 `0xC31700`). Vermutlich gehört das Einfrieren auf 1.10.163 (`0xCFFFFFFF`) dazu, das ist aber nicht belegt. `patches/0014` beendet den Prozess jetzt direkt vor diesem Shutdown. Ungetestet im Spiel, auf beiden Versionen.

## 17. Respawn-Schleife nach dem Tod eines Spielers
`PlayerService::RunRespawnUpdates` rief alle 5 Sekunden `RespawnPlayer` auf, weil der Spieler nach dem Auffüllen der Gesundheit im Essential-Down-Zustand blieb (`actorState`, Bits 17 bis 20, Wert 7 oder 8). Jeder Durchlauf teleportierte ihn und zeigte den Ladebildschirm. Beim anderen Client löschte das die Figur und erzeugte sie neu, daher im Log `Temporary Remote Deleted` und `Actor to remove not found in form ids map`.
`patches/0015` setzt den Lebenszustand zurück und erlaubt pro Tod nur einen Respawn. Ungeprüft: ob der Spieler danach aus der Liegepose aufsteht, und ob der Fehler auch auf 1.10.163 besteht. Der Code war dort schon als ungeprüft markiert (`TODO: ft (verify)`).

Nachtrag zu 17: Im Spieltest auf 1.11.240 mit zwei Clients lief der Respawn laut Nutzer, das Log zeigt drei einzelne Löschungen im Abstand von 26 und 50 Sekunden statt einer Schleife, keinen Absturz und ein sauberes Beenden beider Clients.

## 18. Crash-Handler schickte Berichte an das Sentry-Projekt von Tilted Phoques
`InstallCrashHandler` setzte fest eingetragene DSNs des Originalprojekts. Sentry und Crashpad können Absturzberichte samt Minidump dorthin hochladen. Ob Berichte aus unseren Tests angekommen sind, ist nicht geprüft. `patches/0016` entfernt die DSNs, Berichte bleiben lokal.

## 19. `realloc` fehlte in den Heap-Hooks
Absturz beim Wechsel ins Hauptmenü, auf einem Steam-Thread in `ntdll`. Rückgabeadresse im Stack `ucrtbase+0x318B5` liegt in `_realloc_base`, `rbx` zeigte in den mimalloc-Bereich (`0x2001403...`). 1.11.240 importiert `realloc` aus der UCRT, der Client hookte aber nur `malloc`, `calloc`, `free`, `_msize` und die Aligned-Varianten. So bekam der Windows-Heap einen mimalloc-Zeiger. `patches/0018` ergänzt den Hook. Dass der Absturz damit weg ist, ist im Spiel noch nicht bestätigt.

Nachtrag zu 19: Nur `realloc` zu hooken reichte nicht. Das Spiel gibt auch Blöcke frei und vergrößert sie, die von nicht gehookten CRT-Funktionen stammen. `mi_realloc` kann das nicht (`mi_free: pointer does not point to a valid heap space`, dann Absturz in mimalloc). `patches/0019`: `free`, `realloc` und `_msize` prüfen mit `mi_is_in_heap_region` und geben fremde Blöcke an die CRT zurück.

## 20. Lebenszustand und Essential auf echten Spielständen
Bit 18 von `Actor+0x43C` wird von `0xD71E60` (OG, ID 1085880) / `0xC5B2B0` (NG) neu berechnet. Vorrang hat ein Wert an der Referenz, dann die Basisform, dann Aliase. `HookUpdateEssential` setzt es für den Spieler danach neu.
Der Lebenszustand (`actorState`, Bits 17 bis 20) darf nicht direkt geschrieben werden. `Actor::SetLifeState` (OG `0xD98580`, ID 1300465, NG `0xC83990`) legt beim Wechsel nach 7/8 für den Spieler eine `ExtraInputEnableLayer` mit Maske `0x424` und einen Kamerazustand an und führt die Bleedout-Aktion aus, der Rückweg entfernt das. Die Spieler-Überschreibung liegt in Slot 0x105 (OG `0xE9C0C0`, NG `0xD57980`). Beide Versionen sind hier im Dekompilat gleich.
`Resurrect` beim Spieler (NG `0xD5E2E0`) löscht `currentProcess` und legt ihn neu an, für den Respawn ungeeignet.

## 21. Server meldete sich bei der Server-Liste von Upstream
`ServerListService` sendete pro Minute und bei jedem Beitritt Name, Beschreibung, Port, Spielerzahl, Tickrate und Version an `fallout-reborn-list.skyrim-together.com`, auch mit `bAnnounceServer=false`. Der Empfänger sieht dabei die öffentliche IP. Eine Antwort `403` beendete den Server (`GameServer::Kill`). Der Dienst ist entfernt. Ob die Adresse erreichbar war, ist nicht geprüft.

## 22. Bau-Ereignisse der Werkstatt
Die Empfänger, die Skript-Ereignisse wie `OnWorkshopObjectPlaced` auslösen, sind virtuelle `ProcessEvent` ohne direkte Aufrufer. Signatur `(this, event*)`, das Ereignis hält zwei `TESObjectREFR*`.

| Ereignis | OG-ID | OG-RVA | NG-RVA |
|---|---|---|---|
| Destroyed | 313584 | `0x145D920` | `0x1195F90` |
| Moved | 725213 | `0x145DB70` | `0x1196200` |
| Placed | 697786 | `0x145DDB0` | `0x1196470` |
| Repaired | 670087 | `0x145DFF0` | `0x11966E0` |
| Grabbed | 642544 | `0x145E230` | `0x1196950` |

Belegt ist, dass jeder Empfänger das Skript-Ereignis an beide Referenzen schickt. Dass `[0]` die Werkbank und `[1]` das Objekt ist, ist eine Annahme. `WorkshopService` tauscht beide, falls nur `[0]` temporär ist. `WorkshopReferenceEventSink` ist der falsche Ansatzpunkt, die Stat-Handler werten die Ereignisdaten nicht aus.

Papyrus-Natives auf NG: `PlaceAtMe` `0x1159E80` liefert `TESObjectREFR*` und gibt die eigene Referenz wieder ab, `abDeleteWhenAble` muss `false` sein, sonst verschwindet das Objekt. `SetPosition` `0x115DED0` und `SetAngle` `0x115CEE0` nehmen `RefrOrInventoryObj` und drei floats, `SetAngle` in Grad. `SetLinkedRef` `0x11815D0` nimmt `(self, ziel, keyword)` als einfache Zeiger. `WorkshopItemKeyword` ist `0x54BA6`.

`Game.FindClosestReferenceOfType` (OG-ID 1459241, OG `0x13C0490`, NG `0x1121840`) hat die Form `(vm, stack, nullptr, form, x, y, z, radius)` und durchsucht die geladenen Zellen. `PapyrusService::Get("Game", ...)` findet sie im Spiel nicht, daher der direkte Zeiger. Vermutlich kommt der Klassenname aus dem Stringpool in anderer Schreibweise, das ist nicht belegt.

## 23. Kabel und Stromnetz
Ein Kabel ist eine Referenz mit BNDS-Basis (Formtyp 35), `ExtraPowerLinks` (Typ `0xD0`, zwei Enden) und `ExtraBendableSplineParams` (Typ `0xB4`). Quellen waren die Doku von F4Multiplayer, der Code von FathersFallout und CommonLibF4, die Adressen sind in beiden Spielversionen im Listing zugeordnet.

| Funktion | OG-ID | OG-RVA | NG-RVA |
|---|---|---|---|
| `SplineUtils::ConnectSpline` | 59311 | `0x200E50` | `0x38DFE0` |
| `SplineUtils::DisconnectSpline` | 750682 | `0x200F50` | `0x38E0E0` |
| `TaskQueueInterface::QueueRebuildBendableSpline` | 198419 | `0xD5F120` | `0xC4C090` |
| `ExtraDataList::SetBendableSplineInfo` | 894306 | `0x9A800` | `0x2915A0` |
| `Workshop::IsPowered` | 1546240 | `0x1F2F60` | `0x382530` |

`ConnectSpline` hat keine direkten Aufrufer im Spiel und schreibt nur die `ExtraPowerLinks` an beide Enden und das Kabel. Das Stromnetz fasst es nicht an. Das Netz liegt in `Workshop::ExtraData` (Typ `0xBF`) an der Werkbank. Eine Werkbank, an der nie gearbeitet wurde, hat dieses Extra nicht, im Test in Red Rocket fehlte es.

Der Weg des Spiels, wie an `0x39982C` zu sehen, im Test bestätigt:

| Schritt | OG-ID | OG-RVA | NG-RVA |
|---|---|---|---|
| Stromdaten anlegen: Speicher `0x58` | 898179 | `0x30D0` | `0x1E0320` |
| Stromdaten anlegen: Konstruktor | 1290994 | `0xA9190` | `0x29C7E0` |
| Stromdaten anlegen: an die Extra-Liste | 787248 | `0x42FA0` | `0x29F910` |
| Verbindung eintragen `(daten, a, b, kabel)` | 523652 | `0x1F6E20` | `0x3876D0` |
| Ende aktualisieren `(ende, daten)`, für beide | 958345 | `0x201B10` | `0x38ECB0` |
| Werkbank neu berechnen | 63358 | `0x1F6170` | `0x3867B0` |
| Werkbank neu berechnen, zweiter Teil | 730242 | `0x1F5D40` | `0x386240` |
| Verbindung entfernen `(daten, a, b, kabel, true)` | 800489 | `0x1F7440` | `0x387BD0` |
| Netz eines Objekts suchen `(daten, objekt)` | 1529707 | `0x1F7950` | `0x3881D0` |
| Objekt aus dem Netz nehmen `(daten, objekt)` | 1456203 | `0x1F69C0` | `0x3871C0` |

`IsPowered` ohne dieses Extra ist immer falsch. Die OG-Zuordnungen stammen aus gleichen Aufrufstellen, auf 1.10.163 sind sie nicht getestet. Die Task-Queue liest das Spiel aus der Adresse von ID 7491, im Client heißt dieselbe ID `WeatherManager`, die Zuordnung dort wirkt falsch.

`PapyrusService` fand `Game.FindClosestReferenceOfType` und `GetLinkedRef` im Spiel nicht, während andere Namen gingen. Die Schlüssel sind jetzt unabhängig von der Schreibweise, und `GetLinkedRef` wird damit gefunden. Das stützt die Vermutung aus Abschnitt 22 zum Stringpool, ohne sie direkt zu messen.
