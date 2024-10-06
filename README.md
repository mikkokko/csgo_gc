# csgo_gc

## What is this?
In Valve games, the Game Coordinator (GC) is a backend service most notably responsible for matchmaking and inventory management (like loadouts and skins). This project redirects the GC traffic to a custom implementation running locally.

## Why would you want this?
While it's still possible to connect CS:GO to CS2's GC by spoofing the version number, this may break in the future if Valve updates the GC protocol. This project aims to restore most GC-related functionality without relying on a centralized server.

## Current features
- Editable inventory (inventory.txt)
- Item equipping
- Graffiti support
- Works without full Steam API emulation
- Full Windows, Linux and MacOS support
- Funtional lobbies
- Dedicated server support
- Networking using Steam's P2P interface

## Planned features
- Rest of the inventory features (case opening, stickers, trade ups...)
- Graphical inventory editor
- A tool to copy your CS2 inventory over

## Not planned
- Matchmaking (can't be implemented without a centralized server)

## Installation
- Download the last version of the game before CS2's release using depotdownloader:
```
731 718406683749122620
732 2224497558453288476
733 7173575548168592307
734 3106517550092294329
740 1512455234357538911
```
- Download the latest artifacts for your platform from the [actions page](https://github.com/mikkokko/csgo_gc/actions)
- Back up your existing launcher executables as they'll be overwritten (i.e. csgo.exe, srcds.exe, csgo_linux64, etc.)
- Extract the contents of the downloaded archive to your game directory, replace the executables when prompted
- Launch the game. If you get the annoying VAC message box, launch the game with the -steam argument.

## Building
Requirements:
- Git
- CMake 3.20 or newer
- C++ compiler with C++17 support (VS 2017 or later, Clang 5 or later, GCC 7 or later)

The game is 32-bit on Windows so you need to build as 32-bit:

`cmake -A Win32 ..`

Linux dedicated servers are also 32-bit:

`cmake -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_ASM_FLAGS=-m32 ..`

For Linux and MacOS clients you don't have to specify any additional options.

## Third party dependencies
- [Crypto++](https://github.com/weidai11/cryptopp) ([Boost Software License](https://github.com/weidai11/cryptopp/blob/master/License.txt))
- [funchook](https://github.com/kubo/funchook) ([GPL v2 with Classpath Exception](https://github.com/kubo/funchook/blob/master/LICENSE))
- [diStorm3](https://github.com/gdabah/distorm) ([3-Clause BSD License](https://github.com/gdabah/distorm/blob/master/COPYING))
- [protobuf](https://github.com/protocolbuffers/protobuf) ([3-Clause BSD License](https://github.com/protocolbuffers/protobuf/blob/main/LICENSE))
