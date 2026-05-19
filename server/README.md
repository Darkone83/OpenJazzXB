# OpenJazzXB Relay Server

`openjazzxb_server.py` is the lightweight relay/lobby server used by OpenJazzXB network play.

The relay server handles the multiplayer lobby, room creation, room joining, and the initial game handshake between an Xbox hosting a game and an Xbox joining a game. Once the game starts, normal gameplay packets are forwarded between the host and client.

This server does **not** include or distribute Jazz Jackrabbit™ game data. Each Xbox must have its own local copy of the required game files.

---

## What the Relay Does

The relay server provides:

- A simple lobby for OpenJazzXB clients
- Room creation and room listing
- Host/client pairing
- Player name exchange
- Map-selection notification from host to client
- Initial OpenJazz multiplayer handshake setup
- Packet forwarding once gameplay begins
- Basic disconnect cleanup when a player leaves

The relay is intentionally small and only supports the OpenJazzXB network flow.

---

## Requirements

- Python 3.8 or newer recommended
- A machine that can accept TCP connections from the Xbox consoles
- OpenJazzXB builds with network play support

No extra Python packages are required. The server uses Python's built-in `asyncio`, `argparse`, and `logging` modules.

---

## Basic Usage

From the folder containing the relay script:

```bash
python3 openjazzxb_server.py
```

On Windows, depending on your Python install:

```bat
py openjazzxb_server.py
```

or:

```bat
python openjazzxb_server.py
```

By default, the relay listens on:

```text
0.0.0.0:10052
```

That means it accepts connections on all local network interfaces using port `10052`.

---

## Command Line Options

```bash
python3 openjazzxb_server.py [options]
```

Available options:

| Option | Default | Description |
|---|---:|---|
| `--host` | `0.0.0.0` | Interface/address to bind the relay to |
| `--port` | `10052` | TCP port used by the relay |
| `--max-players` | `2` | Maximum players per room |
| `--lobby-timeout` | `120.0` | Seconds before idle lobby clients time out |
| `--game-timeout` | `60.0` | Seconds before idle game connections time out |
| `--log-level` | `info` | Logging level: `info`, `debug`, or `trace` |

Example:

```bash
python3 openjazzxb_server.py --port 10052 --log-level debug
```

---

## Log Levels

### `info`

Recommended for normal use.

Shows:

- Server start
- Client connections
- Player joins
- Room creation
- Game start
- Cleanup/disconnects

```bash
python3 openjazzxb_server.py --log-level info
```

### `debug`

Useful for testing.

Shows packet names and packet direction during lobby/game flow.

```bash
python3 openjazzxb_server.py --log-level debug
```

### `trace`

Very verbose.

Shows packet names and partial packet hex dumps. This is mainly useful while debugging protocol issues.

```bash
python3 openjazzxb_server.py --log-level trace
```

---

## Network / Firewall Notes

The relay uses TCP.

Make sure the selected port is allowed through the host machine's firewall. If the Xbox consoles are on the same LAN as the relay machine, opening the port on the local firewall is usually enough.

For play across different networks, the relay machine must be reachable by both Xbox consoles, and the selected port must be forwarded or otherwise exposed appropriately.

---

## Typical Play Flow

1. Start the relay server.
2. Launch OpenJazzXB on the host Xbox.
3. Enter the Network menu.
4. Create or host a room.
5. Launch OpenJazzXB on the client Xbox.
6. Enter the Network menu.
7. Browse or join the available room.
8. The host selects a level and starts the game.
9. The relay performs the handshake and forwards gameplay traffic.

If the host disconnects, the client should return gracefully to the network menu.

---

## Room Behavior

The relay tracks rooms in memory only.

- Rooms are created when a host creates a game.
- Rooms are removed when empty.
- A room becomes unavailable for new joins once gameplay has started.
- If a player disconnects, the relay cleans up that connection.
- The default maximum is two players per room.

Because rooms are memory-only, restarting the relay clears all rooms.

---

## Running on Windows

Basic command:

```bat
py openjazzxb_server.py
```

Debug logging:

```bat
py openjazzxb_server.py --log-level debug
```

Custom port:

```bat
py openjazzxb_server.py --port 10052
```

If Windows Firewall prompts for access, allow the relay on the network profile used by your Xbox consoles.

---

## Running on Linux

Basic command:

```bash
python3 openjazzxb_server.py
```

Run with debug logging:

```bash
python3 openjazzxb_server.py --log-level debug
```

Run on a custom port:

```bash
python3 openjazzxb_server.py --port 10052
```

---

## Optional Linux systemd Service

This is optional, but useful if you want the relay to start automatically.

Create a service file such as:

```ini
[Unit]
Description=OpenJazzXB Relay Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=/opt/openjazzxb-relay
ExecStart=/usr/bin/python3 /opt/openjazzxb-relay/openjazzxb_server.py --port 10052 --log-level info
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Then enable and start it:

```bash
sudo systemctl daemon-reload
sudo systemctl enable openjazzxb-relay
sudo systemctl start openjazzxb-relay
```

Check status:

```bash
sudo systemctl status openjazzxb-relay
```

View logs:

```bash
journalctl -u openjazzxb-relay -f
```

---

## Troubleshooting

### Clients cannot see or join rooms

Check:

- The relay server is running.
- Both Xbox consoles can reach the relay machine.
- The correct port is open in the firewall.
- The host created a room before the client tried to join.
- The room has not already started gameplay.

### Client returns to the network menu

This usually means the host disconnected, the relay connection dropped, or the game session ended before the client fully joined.

### Game starts but disconnects quickly

Try running the relay with debug logging:

```bash
python3 openjazzxb_server.py --log-level debug
```

Look for disconnect messages, room cleanup, or unexpected timeout behavior.

### Relay starts but nothing connects

Check the bind address and port. The default bind address, `0.0.0.0`, listens on all local interfaces. If you bind to a specific address, make sure it is the address reachable by the Xbox consoles.

---

## Notes for Test Builds

During active testing, `--log-level debug` is usually the best balance. It gives enough information to follow the connection flow without producing excessive packet dumps.

Use `--log-level trace` only when trying to diagnose packet-level behavior.

---

## Limitations

Current relay behavior is intentionally simple:

- Rooms are not persistent.
- Rooms are cleared when the relay restarts.
- The default target is two-player host/client play.
- The relay does not provide matchmaking.
- The relay does not provide game data.
- The relay does not modify gameplay once the game phase begins.

---

## License

This relay server is part of the OpenJazzXB project and follows the same license terms as the rest of the OpenJazzXB source distribution.

OpenJazzXB is a derivative work of OpenJazz and is distributed under the GNU General Public License, version 2 or later.
