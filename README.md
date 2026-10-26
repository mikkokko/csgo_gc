# csgo_gc

> [!CAUTION]
> This project is incomplete and not ready for general use.

In Valve games, the Game Coordinator (GC) is a backend service most notably responsible for matchmaking and inventory management (like loadouts and skins). This project redirects the GC traffic to a custom, in-process implementation, eliminating the need for a separate GC server.

## Features

- **Inventory:** Editing, equipping, case opening, stickers, patches, graffiti, StatTrak, storage units, name tags, music kits and the in-game store
- **Multiplayer:** Lobbies, dedicated servers and a functional server browser
- Steam P2P networking for inventory data
- Live inventory editing via a WebSocket server
- [Multi-version support](docs/versions.md) (experimental)
- Full Windows, Linux and macOS support
- Works without full Steam API emulation

Missing inventory features (trade ups, souvenirs, StatTrak swaps, etc.) are planned. Operation-related features are not currently planned. Matchmaking is not planned, as a proper implementation would require a centralized server, which this project intentionally avoids.

## Installation

- Download [CS:GO from Steam](steam://install/4465480)
- Download the latest release for your platform from the [releases page](https://github.com/mikkokko/csgo_gc/releases/latest)
- Navigate to the game's installation directory
- Back up your existing launcher executables as they'll be overwritten (i.e. csgo.exe, srcds.exe, csgo_linux64, etc.)
- Extract the contents of the downloaded archive to your game directory, replace the executables when prompted
- Launch the game. If you get the annoying VAC message box, launch the game with the -steam argument
- macOS users: The release binaries are not notarized, so if you're using them, you'll have to deal with that somehow

## Inventory editing

### Realtime editing

The GC exposes a local WebSocket API for inventory editors. An editor can read the inventory and send changes while the game is running. Changes appear in-game immediately and are saved to inventory.txt. In-game inventory changes are also sent to the connected editor.

A web-based realtime editor is available [here](https://github.com/mikkokko/csgo_invedit). The default `inventory_editor_origin` in `csgo_gc/config.txt` allows connections from its site. To use another editor, set it to the `Origin` that editor provides.

### Offline editing

See [available GUI editors](https://github.com/mikkokko/csgo_gc/issues/82). There is also a guide for editing inventory.txt by hand [here](https://gist.github.com/dricotec/1ae3deb06c42012970c00df914348e76) if you want to do that for some reason.

## Configuration

See [csgo_gc/config.txt](examples/config.txt) for available options.

## Bug reports and support

Report bugs by opening an issue and following the instructions. If the issue doesn't appear to be strictly GC related, verify that it doesn't happen on a clean game (no csgo_gc installed).

For support, ideas, and general discussion, use [Discussions](https://github.com/mikkokko/csgo_gc/discussions).

## Building

Requires Git, vcpkg, CMake 3.20+, and a compiler with C++20 support. See [the continuous build workflow](.github/workflows/build.yml) for details.

## License

[2-Clause BSD](LICENSE.md)

## Credits
* **Mikko Kokko** - Author
* **Theeto** - Code reused from the predecessor project, unusual loot lists
