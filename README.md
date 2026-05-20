# OpenJazzXB

**Jazz Jackrabbit™ for the Original Xbox**

OpenJazzXB is a port of [OpenJazz](https://github.com/AlisterT/openjazz) — the open-source reimplementation of Jazz Jackrabbit™ — to the original Microsoft Xbox, built with the RXDK homebrew toolchain.

> **You must own a legal copy of Jazz Jackrabbit™ to use this port.**
> OpenJazzXB does not include any game data. You will need the original game files from Jazz Jackrabbit™ (shareware or retail) placed on your Xbox's hard drive.

---

## Features

- Full Jazz Jackrabbit™ Episode 1 gameplay on real Xbox hardware
- Xbox controller support with remappable buttons
- Video output: 480i, 480p, 720p (selected via setup menu, requires restart)
- Aspect modes: 4:3 corrected, stretch, pixel-perfect, fill
- Texture filtering: sharp (point), smooth (linear), Scale2x
- Scanline overlay: off, light, medium, heavy
- Save and load game (4 slots)
- In-game pause menu (Start button)
- Network play support for online/server-based sessions and local LAN games
- Demo playback
- Plasma menu backgrounds
- Persistent configuration saved to `D:\xbjazz.cfg`

---

## Requirements

- Original Xbox (any revision, softmodded or hardmodded)
- Component, HDMI adapter, or composite AV cable
- Jazz Jackrabbit™ game data files (shareware or retail)

### Game Data

Place the following files in the root of your Xbox's `D:\` drive alongside the OpenJazzXB XBE:

- `JAZZ.EXE` (or equivalent data files from your version)
- All `.000`–`.009` episode files
- All `.MUS` music files
- Font and panel files

Refer to the OpenJazz documentation for a complete file list. The shareware episode (Episode 1) is freely available and fully supported.

---

## Controls

### Fixed Bindings

| Button | Action |
|---|---|
| D-pad / Left stick | Move |
| A | Jump / Swim / Confirm |
| B | Back (menus) |
| Start | In-game menu |
| Back | Back (menus) |

### Remappable (via Setup → Controls)

| Action | Default |
|---|---|
| Primary fire | L-Trigger |
| Secondary fire | X |
| Alt jump / swim | R-Trigger |
| Change weapon | Y |
| Stats overlay | Black |

---

## Setup Menu

Access via **Start** during gameplay, or from the main menu under **Setup**.

- **Controls** — remap the five configurable actions to any face button or trigger
- **Video** — resolution, aspect mode, filter, scanlines
- **Audio** — music and sound effect volume
- **Gameplay** — slow motion, extra items, bird limit, HUD style
- **Network** — configure player name, join network games, or host/join local LAN games

> Resolution changes require a restart to take effect. All other settings apply immediately.

---

## Network Play

OpenJazzXB includes experimental multiplayer support. Network play is available in two modes:

- **Join Net Game** — connect to a network game service, browse available rooms, create a room, or join another player's room.
- **LAN Game** — host or join a direct local network game between Xbox consoles on the same LAN.

Network play is still intended for playtesting and may have edge cases. If a host leaves or a session drops, the game should return safely to the network menu instead of locking up.

### Join Net Game

Use **Setup → Network → Join Net Game** for server-based multiplayer rooms.

From this menu, you can:

- Browse available rooms
- Create a new room
- Join another player's room
- Select episode, level, and difficulty as the host
- Start the game once both players are ready

This mode is useful when players are not using direct LAN play or when a central game service is preferred.

### LAN Game

Use **Setup → Network → LAN Game** for local network play between Xbox consoles on the same network.

From the LAN Game menu, you can:

- **Host Game** — advertise a local game and wait for another Xbox to join
- **Join Game** — scan the local network and join an available LAN host

LAN Game does not require an external game service. Both Xbox consoles must be on the same local network and must have matching OpenJazzXB builds and the required Jazz Jackrabbit™ game data installed locally.

Once connected, the host selects the episode, level, and difficulty, then starts the game.

### Network Setup

Use **Setup → Network → Network Setup** to configure:

- Player name
- Network connection settings used by Join Net Game

LAN Game discovery does not require manual host entry when both consoles are on the same local network.

---

## Installation

1. Build the XBE using the RXDK toolchain (see **Building** below), or obtain a pre-built release.
2. Copy `default.xbe` and your game data files to a folder on your Xbox hard drive (e.g. `E:\Games\OpenJazzXB\`).
3. Launch via your dashboard of choice.
4. On first boot, `D:\openjazzxb.cfg` is created automatically with safe defaults (480i output).

---

## Building

OpenJazzXB is built with the RXDK (Retail XDK) homebrew toolchain for original Xbox.

**Requirements:**
- RXDK / Xbox SDK headers and libraries
- MSVC (matching RXDK version)
- SDL 1.2 Xbox port headers

The project is a flat source layout — all files live in the project root with no subdirectory includes. See individual source files for platform-specific notes.

---

## License

OpenJazzXB is a derivative work of OpenJazz and is distributed under the **GNU General Public License, version 2 or later**, in compliance with OpenJazz's license terms.

This means:

- The full source code for OpenJazzXB must be made available alongside any binary distribution.
- Any modifications must also be distributed under the GPL v2 or later.
- See [`COPYING`](COPYING) for the full license text.

Xbox-specific port code (the `xb_*` files, `video.cpp`, `controls.cpp`, `main.cpp`, and related platform files) is original work by **Darkone83 / Team Resurgent**, and is also released under GPL v2 or later to maintain license compatibility.

---

## Credits

### OpenJazz

OpenJazzXB is built on top of OpenJazz, without which this port would not exist.

| Contributor | Role |
|---|---|
| **AJ Thomson** | Original author and creator of OpenJazz (2005–2025) |
| **Carsten Teibes** | Code cleanup, restructuring, platform ports (2015–2026) |
| **Alireza Nejati** | Menu plasma effect (2010) |

OpenJazz source code and project: https://github.com/AlisterT/openjazz

### Jazz Jackrabbit™

Jazz Jackrabbit™ is a trademark of **Epic Games, Inc.** (formerly Epic MegaGames). OpenJazzXB is not affiliated with or endorsed by Epic Games. No game data is included.

### Third-Party Libraries (via OpenJazz)

| Library | Author | License |
|---|---|---|
| **SDL 1.2** | Sam Lantinga et al. | GNU LGPL 2.1 |
| **libpsmplug** | Olivier Lapicque (public domain MOD rendering) | Public Domain |
| **Scale2x** | Andrea Mazzoleni (2001–2004) | GPL v2 |
| **miniz** | RAD Game Tools, Rich Geldreich | MIT |
| **stb_rect_pack** | Sean Barrett | MIT |

### Xbox Port

| | |
|---|---|
| **Port author** | Darkone83 (Team Resurgent) |
| **Community** | Original Xbox Homebrew Community, Xbox-Scene |

---

## Disclaimer

OpenJazzXB is a hobby homebrew project. It is not affiliated with Epic Games, the OpenJazz team, or any commercial entity. Jazz Jackrabbit™ is the property of Epic Games, Inc.

This software is provided as-is with no warranty. Use at your own risk on your own legally owned hardware and game software.
