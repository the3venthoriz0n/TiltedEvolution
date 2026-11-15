# Skyrim Together
#### codename "TiltedEvolution"
![Build status](https://github.com/tiltedphoques/TiltedEvolution/workflows/Build%20windows/badge.svg?branch=master) [![Build linux](https://github.com/tiltedphoques/TiltedEvolution/actions/workflows/linux.yml/badge.svg)](https://github.com/tiltedphoques/TiltedEvolution/actions/workflows/linux.yml)  [![Discord](https://img.shields.io/discord/247835175860305931.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/skyrimtogether)

<img src="https://avatars.githubusercontent.com/u/52131158?s=200&v=4" align="right"
     alt="Size Limit logo by Anton Lovchikov" width="110" height="100">

TiltedEvolution is a framework created to enable multiplayer in Bethesda games, currently supporting **Skyrim Special Edition**.

## Getting started
To play Skyrim Together, go to the [nexus page](https://www.nexusmods.com/skyrimspecialedition/mods/69993).

For general information, go to the [TiltedEvolution Wiki](https://wiki.tiltedphoques.com/tilted-online/).

Check out the [build guide](https://wiki.tiltedphoques.com/tilted-online/technical-documentation/build-guide) for setup and development info on the project. When writing code, check the CODE_GUIDELINES.md and make sure to run clang-format!

## Reporting bugs
If you would like to report a bug please report them in the "Issues" tab on this page. Detailed and reproducible bug reports are of great importance for the development of the project.

## Contributing
Have some experience in C++, and want to help advance the project faster? Contribute!
- Check the issues, it's a good place to start when you don't know what to do.
- Fork the repository and create pull requests to this repository.
- Create pull requests to the "dev" branch, not to master.
- Try to keep your code clean, following the code guidelines.

## Main project source tree

* [**client/**](./Code/client): Skyrim SE client: engine hooks, reverse-engineered game types and the sync logic.
* [**immersive_launcher/**](./Code/immersive_launcher): `SkyrimTogether.exe`, which starts the game with the client loaded.
* [**immersive_elf/**](./Code/immersive_elf): `EarlyLoad.dll` helper DLL for the launcher. Detects if Mod Organizer 2 was injected first.
* [**tp_process/**](./Code/tp_process): Worker for the CEF (Chromium Embedded Framework) overlay.
* [**skyrim_ui/**](./Code/skyrim_ui): In-game UI, written in Angular/TypeScript.
* [**server/**](./Code/server): Game server (`STServer.dll`).
* [**server_runner/**](./Code/server_runner): Dedicated server executable that hosts the server DLL.
* [**encoding/**](./Code/encoding): Network message and struct definitions shared by the client and server.
* [**common/**](./Code/common), [**base/**](./Code/base): Shared utilities.
* [**components/**](./Code/components): Shared modules (console/settings, ESP loader, resources, ImGui).
* [**tests/**](./Code/tests): Unit tests.
* [**GameFiles/**](./GameFiles): Plugins, scripts and assets shipped with the mod.
* [**Libraries/**](./Libraries): Networking, hooking, reverse-engineering and UI libraries (git submodules).
* [**Tools/**](./Tools): Reverse-engineering and address scripts.

## Releasing the potion-sync client

Push a tag `potions-v<STR version>-<build number>` on a commit that is on `potion-sync` or `potion-sync-crashfix` to build the Windows client and publish it. The tag only chooses the release; the client still reports the version string `v1.8.4-potions1`, because the server refuses clients with a different string.

        git tag potions-v1.8.4-1
        git push origin potions-v1.8.4-1

A tag with a further suffix, for example `potions-v1.8.4-1-test1`, makes a GitHub *pre-release*. The release has `SkyrimTogether-1.8.4-potions1-windows-x64.zip` and `SkyrimTogether-1.8.4-potions1-windows-x64-debug-symbols.zip`. The workflow is `.github/workflows/release-potion-client.yml`; it is separate from the upstream `release.yml` (tags `vX.Y.Z`), which it never triggers. Run it from the Actions tab (without a tag) to build only.

## License
[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

TiltedEvolution is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

## Modified version, source and credits

**This is a modified version of Skyrim Together Reborn.** It is not the official release and is not made or endorsed by the Tilted Online / Skyrim Together Reborn team. Do not ask them for support for this build. What changed is listed in [MODIFICATIONS.md](./MODIFICATIONS.md), where the source for a build is found is in [SOURCE.txt](./SOURCE.txt), and the licence is in [LICENSE](./LICENSE) (GPL-3.0-or-later; see also its note on the LGPLv2 launcher component). Upstream: https://github.com/tiltedphoques/TiltedEvolution.

### Credits

- **Tilted Online / Skyrim Together Reborn team** (TiltedPhoques) and all upstream contributors, whose work this is built on. From the upstream git history (`git shortlog -sn`, as of the fork point `54b3a858`), the authors with the most commits are: Robbe Bryssinck, yamashi, Force67, Maxime Griot, Sturle Spetland (also committing as Sturlen), Dragonisser, Daniil Zakharov, Rich Fortier (rfortier), Vincent Hengel, Sebastian Comans (scomans), Shynd (Outshynd), DJLegends, Lee, kulcris and E F, with many more with fewer commits. The full list is in the git history and at https://github.com/tiltedphoques/TiltedEvolution/graphs/contributors. Upstream copyright notices in the source files are kept.
- **Third-party libraries** (git submodules in `.gitmodules`, and the xmake packages pinned in `xmake.lua`). Each is under its own licence, which applies to it; check the library's own repository for the licence text:
  - Submodules, all by TiltedPhoques: [TiltedReverse](https://github.com/TiltedPhoques/TiltedReverse), [TiltedUI](https://github.com/TiltedPhoques/TiltedUI), [TiltedConnect](https://github.com/TiltedPhoques/TiltedConnect), [TiltedHooks](https://github.com/TiltedPhoques/TiltedHooks).
  - Copied into `Libraries/`: cxxopts (Jarryd Beck, MIT licence text in `Libraries/cxxopts.hpp`) and magic_enum.
  - xmake packages: entt, tiltedcore, cryptopp, spdlog, cpp-httplib, gtest, mem, glm, zlib, and on Windows discord and imgui.
  - `Code/launcher` is under the LGPLv2 and derives from code by the CitizenFX authors (see `LICENSE`).
- **Modifications** (potion and poison sync, crash fixes, build and release tooling): the3venthoriz0n, https://github.com/the3venthoriz0n.
- **AI assistance:** Claude (Anthropic).
