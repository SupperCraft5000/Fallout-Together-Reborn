# Fallout Together Reborn: Anleitung für Mitspieler

**Du brauchst:** Fallout 4 von Steam in Version 1.11.240 und die Datei `FalloutTogetherReborn.exe`.

1. Steam starten und anmelden.
2. `FalloutTogetherReborn.exe` starten. Windows warnt vermutlich, weil die Datei nicht signiert ist: „Weitere Informationen“, dann „Trotzdem ausführen“.
3. Die Server-Adresse und das Passwort vom Gastgeber eintragen, dann auf **Spielen** klicken.
   Beim ersten Start richtet sich das Programm kurz ein.
4. Im Spiel einen Spielstand laden oder ein neues Spiel beginnen. Die Verbindung startet automatisch.

**Gut zu wissen**
- Am besten ein eigener Spielstand für den Co-op, die Mod ist noch nicht ausgereift.
- Dein Spielordner wird nicht verändert. Die Programmdateien liegen unter `%LOCALAPPDATA%\FalloutTogetherReborn`.
- Andere Mods besser abschalten.
- Bei Problemen bitte `logs\tp_client.log` aus dem dortigen Ordner schicken.

**Gastgeber:** Haken bei „Server auf diesem PC starten“ setzen, ein Passwort eintragen und spielen.
Die Mitspieler brauchen deine Adresse (`IP` oder `IP:10578`). Entweder den UDP-Port 10578 im Router freigeben oder gemeinsam ein VPN wie Tailscale nutzen.

## Gleiche Version

Host und Mitspieler brauchen dieselbe `FalloutTogetherReborn.exe` und dieselbe Fallout-4-Version. Der Starter prüft das vor dem Spielstart und bricht sonst mit einer Meldung ab. Dafür muss der Host zuerst auf **Spielen** klicken, und TCP-Port 10578 muss bei ihm frei sein, auch wenn kein Spielstand übertragen wird. Der Server prüft die Version beim Verbinden noch einmal.

## Neue Version

Die neue exe einfach starten. Sie räumt die Ordner älterer Versionen von selbst auf. Deine Figur, Einstellungen und Spielstände bleiben erhalten, die letzten Logs liegen danach in `%LOCALAPPDATA%\FalloutTogetherReborn\logs-previous`.

## Automatisch starten

Ohne Haken bei „Hauptmenü anzeigen“ lädt das Spiel direkt nach dem Start den Spielstand: beim Host den gewählten oder neuesten, beim Mitspieler den vom Host. Klappt das nicht, bleibt das Spiel im Hauptmenü.

Beendet der Host das Spiel, erscheint beim Mitspieler eine Meldung, danach schließt sich sein Spiel.

## Spielstand vom Host übernehmen

So seid ihr beide in derselben Welt:

**Host:** Haken bei „Server auf diesem PC starten“ und „Meinen Spielstand freigeben“ setzen. Ohne Auswahl wird der neueste Spielstand freigegeben, mit „Wählen“ ein anderer. Dann „Spielen“ und genau den Spielstand laden, den der Starter unten anzeigt. Weicht er ab, startet ihr an verschiedenen Orten und seht euch nicht. Windows fragt beim ersten Mal nach der Firewall, dort den Zugriff im privaten Netz erlauben. Neben UDP wird jetzt auch TCP-Port 10578 gebraucht.

**Mitspieler:** Erst starten, wenn das Spiel des Hosts läuft. Haken bei „Spielstand vom Host übernehmen“ setzen, dann „Spielen“. Im Hauptmenü auf **Weiter** klicken. Die Kopie trägt den Dateinamen des Hosts mit aktueller Zeit und wird beim Spielende wieder gelöscht. Falls **Weiter** nicht in die Host-Welt führt, wähle unter **Laden** den neuesten Spielstand mit dem Namen der Host-Figur. Eigene Spielstände werden nicht verändert.

Ohne „Eigene Figur mitnehmen“ spielst du eine Kopie der Host-Figur.

**Eigene Figur mitnehmen** (Standard): Das Spiel startet zuerst kurz mit deinem neuesten eigenen Spielstand oder dem unter „Wählen“ gesuchten, speichert deine Figur und schließt sich von selbst. Danach startet es mit dem Spielstand des Hosts neu und legt rund 10 Sekunden nach dem Verbinden Aussehen, Name, Inventar und SPECIAL-Werte an. Dabei verbindet es sich einmal kurz neu. Hat sich dein Spielstand nicht geändert, entfällt der erste Start. Stufe und Perks werden noch nicht übernommen.

