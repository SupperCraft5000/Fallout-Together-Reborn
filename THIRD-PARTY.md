# Komponenten von Dritten

Die Einzel-exe enthält neben dem eigenen Code diese Komponenten. Die Rechte liegen bei den jeweiligen Inhabern, es gelten deren Lizenzen. Die Versionen stehen in `xmake.lua` und den `xmake.lua`-Dateien unter `Code/`.

| Komponente | Zweck | Lizenz |
|---|---|---|
| Tilted Online / TiltedEvolution und Tilted-Bibliotheken | Grundlage des Projekts | GPL-3.0 beziehungsweise Lizenz der jeweiligen Bibliothek |
| Chromium Embedded Framework und Chromium | Overlay (`libcef.dll` und Ressourcen) | BSD-3-Clause, Chromium enthält weitere Lizenzen |
| SwiftShader, ANGLE, Vulkan Loader | Grafik für das Overlay | Apache-2.0 und BSD |
| Crashpad, Sentry Native | Absturzberichte, nur lokal | Apache-2.0, MIT |
| Valve GameNetworkingSockets | Netzwerk | BSD-3-Clause |
| Dear ImGui, spdlog, EnTT, mimalloc, MinHook, libuv, glm, Lua, sol2, SQLite, Crypto++ | Bibliotheken | MIT, BSD, Boost oder gemeinfrei, je nach Projekt |
| Discord Game SDK | Discord-Anbindung | Discord Developer Terms |
| Microsoft `d3dcompiler_47.dll` | Shader für das Overlay | Microsoft-Lizenz |

Diese Liste ist eine Übersicht und ersetzt nicht die Lizenztexte der Projekte. Beim Bau legt xmake die Quellen der Pakete im Paket-Cache ab.

Fallout 4 und alle Spieldateien sind nicht enthalten.
