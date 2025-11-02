# csgo_gc

> [!WARNING]
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
- In-game store
- Works without full Steam API emulation
- Full Windows, Linux and MacOS support
- Functional lobbies
- Dedicated server support
- Functional server browser (only shows csgo_gc servers)
- Networking using Steam's P2P interface

## Planned features
- Rest of the core features (trade ups, souvenirs, StatTrak swaps...)
- Graphical inventory editor
- A tool to copy your CS2 inventory over

I'm still looking for the **full** CS:GO Item Schema. If you have a relatively recent copy of it and are willing to share it, let me know!

## Not planned
- Matchmaking (can't be implemented without a centralized server)

## Installation
- Download the last version of the game before CS2's release using [DepotDownloader](https://github.com/SteamRE/DepotDownloader). Other versions might work, but are not tested or supported. Also note that **this is not the same version as the csgo_legacy branch**. Manifest IDs:
```
731 718406683749122620
732 2224497558453288476
733 7173575548168592307
734 3106517550092294329
740 1512455234357538911
```
- Download the latest release for your platform from the [releases page](https://github.com/mikkokko/csgo_gc/releases/latest)
- Back up your existing launcher executables as they'll be overwritten (i.e. csgo.exe, srcds.exe, csgo_linux64, etc.)
- Extract the contents of the downloaded archive to your game directory, replace the executables when prompted
- Launch the game. If you get the annoying VAC message box, launch the game with the -steam argument.

## Building
Requirements:
- Git
- CMake 3.20 or newer
- C++ compiler with C++17 support (VS 2017 or later, Clang 5 or later, GCC 7 or later)

The game is 32-bit on Windows so you need to build as 32-bit:

`cmake -A Win32 -B build`

Linux dedicated servers are also 32-bit:

`cmake -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_ASM_FLAGS=-m32 -B build`

On MacOS, you need to build for x86_64 instead of arm64:

`cmake -DCMAKE_OSX_ARCHITECTURES=x86_64 -DFUNCHOOK_CPU=x86 -B build`

For Linux clients you don't have to specify any additional options.

## Third party dependencies
- [Crypto++](https://github.com/weidai11/cryptopp) ([Boost Software License](https://github.com/weidai11/cryptopp/blob/master/License.txt))
- [funchook](https://github.com/kubo/funchook) ([GPL v2 with Classpath Exception](https://github.com/kubo/funchook/blob/master/LICENSE))
- [diStorm3](https://github.com/gdabah/distorm) ([3-Clause BSD License](https://github.com/gdabah/distorm/blob/master/COPYING))
- [protobuf](https://github.com/protocolbuffers/protobuf) ([3-Clause BSD License](https://github.com/protocolbuffers/protobuf/blob/main/LICENSE))
