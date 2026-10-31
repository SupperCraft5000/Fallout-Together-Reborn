# Third-party components

Besides our own code, the single exe contains these components. The rights remain with their respective owners, and their licenses apply. Versions are in `xmake.lua` and the `xmake.lua` files under `Code/`.

| Component | Purpose | License |
|---|---|---|
| Tilted Online / TiltedEvolution and Tilted libraries | basis of the project | GPL-3.0 or the license of the respective library |
| Chromium Embedded Framework and Chromium | overlay (`libcef.dll` and resources) | BSD-3-Clause, Chromium includes further licenses |
| SwiftShader, ANGLE, Vulkan Loader | graphics for the overlay | Apache-2.0 and BSD |
| Crashpad, Sentry Native | crash reports, local only | Apache-2.0, MIT |
| Valve GameNetworkingSockets | networking | BSD-3-Clause |
| Dear ImGui, spdlog, EnTT, mimalloc, MinHook, libuv, glm, Lua, sol2, SQLite, Crypto++ | libraries | MIT, BSD, Boost or public domain, depending on the project |
| Discord Game SDK | Discord integration | Discord Developer Terms |
| Microsoft `d3dcompiler_47.dll` | shaders for the overlay | Microsoft license |

This list is an overview and does not replace the license texts of the projects. When building, xmake places the package sources in the package cache.

Fallout 4 and all game files are not included.
