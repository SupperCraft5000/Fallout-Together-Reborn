# Fallout Together Reborn: guide for players

**You need:** Fallout 4 from Steam in version 1.11.240 and the file `FalloutTogetherReborn.exe`.

1. Start Steam and sign in.
2. Start `FalloutTogetherReborn.exe`. Windows will probably warn you because the file is not signed: choose "More info", then "Run anyway".
3. Enter the server address and password from the host, then click **Spielen** (Play).
   The program sets itself up briefly on the first start.
4. In the game, load a save or start a new game. The connection starts automatically.

The starter's interface is in German. This guide gives the original label of each option, with the translation in brackets.

**Good to know**
- Use a separate save for co-op, the mod is not mature yet.
- Your game folder is not changed. The program files are in `%LOCALAPPDATA%\FalloutTogetherReborn`.
- It is better to turn other mods off.
- If there are problems, please send `logs\tp_client.log` from that folder.

**Host:** Tick "Server auf diesem PC starten" (start server on this PC), enter a password and play.
Players need your address (`IP` or `IP:10578`). Either forward UDP port 10578 in your router or use a VPN such as Tailscale together.

## Same version

Host and players need the same `FalloutTogetherReborn.exe` and the same Fallout 4 version. The starter checks this before the game starts and otherwise stops with a message. For this the host must click **Spielen** first, and TCP port 10578 must be open on the host, even if no save is transferred. The server checks the version again on connecting.

## Connecting over Steam
Over Steam you need no port forwarding and no VPN. Both players need Steam running and Fallout 4 on their account.

**Host:** Tick "Server auf diesem PC starten" and "Über Steam erreichbar" (reachable over Steam), then click "Spielen". Your Steam ID then appears in the status and in the clipboard. Give it to the other player. Set a password, because otherwise anyone who knows your Steam ID can reach your server.

**Player:** Enter the host's Steam ID instead of an address, then click "Spielen". The starter recognises the Steam ID by its 17 digits.

Steam shows Fallout 4 as running before the game actually starts. Connecting by IP address still works. Problems are recorded in `%LOCALAPPDATA%\FalloutTogetherReborn\tunnel.log`.

## New version

Just start the new exe. It removes the folders of older versions by itself. Your character, settings and saves are kept, and the latest logs are then in `%LOCALAPPDATA%\FalloutTogetherReborn\logs-previous`.

## Starting automatically

Without the tick at "Hauptmenü anzeigen" (show main menu), the game loads the save right after starting: for the host the chosen or newest one, for a player the host's. If that fails, the game stays in the main menu.

If the host quits the game, players see a message and their game then closes.

## Taking over the host's save

This way both of you are in the same world:

**Host:** Tick "Server auf diesem PC starten" and "Meinen Spielstand freigeben" (share my save). Without a selection the newest save is shared, "Wählen" (choose) picks another. Then click "Spielen" and load exactly the save the starter shows at the bottom. If it differs, you start in different places and will not see each other. Windows asks about the firewall the first time, allow access on the private network. TCP port 10578 is now needed in addition to UDP.

**Player:** Only start once the host's game is running. Tick "Spielstand vom Host übernehmen" (take over the host's save), then click "Spielen". In the main menu click **Weiter** (Continue). The copy has the host's file name with the current time and is deleted again when the game ends. If **Weiter** does not lead into the host's world, choose the newest save with the name of the host's character under **Laden** (Load). Your own saves are not changed.

Without "Eigene Figur mitnehmen" (bring your own character) you play a copy of the host's character.

**Eigene Figur mitnehmen** (default): The game first starts briefly with your newest own save, or the one picked under "Wählen", saves your character and closes by itself. It then restarts with the host's save and applies appearance, name, inventory and SPECIAL values about 10 seconds after connecting. It reconnects once briefly while doing so. If your save has not changed, the first start is skipped. Level and perks are not carried over yet.
