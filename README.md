# csgo_gc

> [!CAUTION]
> This project is incomplete and not ready for general use.

## What is this?
In Valve games, the Game Coordinator (GC) is a backend service most notably responsible for matchmaking and inventory management (like loadouts and skins). This project redirects the GC traffic to a custom, in-process implementation.

## Why would you want this?
While it's still possible to connect CS:GO to CS2's GC by spoofing the version number, this may break in the future if Valve updates the GC protocol. This project aims to restore most GC-related functionality without relying on a centralized server.

## Current features
- Editable inventory (inventory.txt)
- Item equipping
- Opening cases (including sticker capsules, patch packs, graffiti boxes and music kit boxes)
- Graffiti support
- Weapon StatTrak support
- Stickers and patches
- Name tags
- Music kits
- In-game store
- Works without full Steam API emulation
- Full Windows, Linux and macOS support
- Functional lobbies
- Dedicated server support
- Functional server browser (only shows csgo_gc servers by default)
- Networking using Steam's P2P interface

## Planned features
- Rest of the core features (trade ups, souvenirs, storage units, StatTrak swaps...)

I'm still looking for the **full** CS:GO Item Schema. If you have a relatively recent copy of it and are willing to share it, let me know!

## Not planned
- Matchmaking (can't be implemented without a centralized server)

## Installation
- Download [CS:GO from Steam](steam://install/4465480)
- Download the latest release for your platform from the [releases page](https://github.com/mikkokko/csgo_gc/releases/latest)
- Navigate to the game's installation directory
- Back up your existing launcher executables as they'll be overwritten (i.e. csgo.exe, srcds.exe, csgo_linux64, etc.)
- Extract the contents of the downloaded archive to your game directory, replace the executables when prompted
- Launch the game. If you get the annoying VAC message box, launch the game with the -steam argument
- macOS users: The release binaries are not notarized, so if you're using them, you'll have to deal with that somehow

## Inventory editing
For GUI inventory editors, see https://github.com/mikkokko/csgo_gc/issues/82. For manual editing, there is a guide made by someone else [here](https://gist.github.com/dricotec/1ae3deb06c42012970c00df914348e76).

## Configuration
See [csgo_gc/config.txt](examples/config.txt) for available options.

## Bug reports and support

**Got a crash or found a bug?**
* **Broken csgo_gc feature?** Open an issue and follow the instructions.
* **Game crashing or broken non-GC features?** If you are unsure if csgo_gc caused it, test the game without it installed first. If the bug still happens on a clean build, it's an issue with the game itself. **Do not report base game bugs here.** If you confirm csgo_gc is definitely causing the crash, open an issue.

**Need help, want to share ideas, or self-promote?**
Use [Discussions](https://github.com/mikkokko/csgo_gc/discussions). Do not open issues for support.

## Building
Requirements:
- Git
- vcpkg
- CMake 3.20 or newer
- C++ compiler with C++17 support (VS 2017 or later, Clang 5 or later, GCC 7 or later)

See [the continuous build workflow](.github/workflows/build.yml) for details.

## License
This project is licensed under the 2-Clause BSD License. See [LICENSE.md](LICENSE.md) for details.

## Credits
* **Mikko Kokko** - Author
* **Theeto** - Code reused from the predecessor project, unusual loot lists
