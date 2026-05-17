#!/usr/bin/env python3
"""
openjazzxb_server.py -- OpenJazzXB relay server

Protocol phases:
  1. LOBBY    -- client sends player name, server sends room list, client picks room
  2. WAITING  -- players gathered in a room, waiting for host to start
  3. PLAYING  -- OJ protocol relayed transparently (MT_G_*, MT_L_*, MT_P_*)

Custom lobby packet types (0xF0-0xFF, never used by OpenJazz):
  0xF0  OJXB_HELLO    client -> server: player name (null-terminated, max 16 bytes)
  0xF1  OJXB_ROOMLIST server -> client: room list
  0xF2  OJXB_JOIN     client -> server: join room_id (0 = create new)
  0xF3  OJXB_ROOMINFO server -> client: room state update (player joined/left)
  0xF4  OJXB_START    server -> client: host triggered game start

Packet framing: same as OpenJazz -- buffer[0] = total length including itself.

Port: 10052  (NET_PORT from OpenJazz network.h)

Author: Darkone83 / Team Resurgent
License: GPL v2
"""

import asyncio
import logging
import time
import sys
from dataclasses import dataclass, field
from typing import Optional

# ── Config ────────────────────────────────────────────────────────────────────

HOST        = "0.0.0.0"
PORT        = 10052
MAX_ROOMS   = 8
MAX_PLAYERS = 2          # Xbox: 2 players to start
TIMEOUT_S   = 60
VERSION     = 1

# ── OpenJazz message types ────────────────────────────────────────────────────

MT_G_PROPS = 0x00
MT_G_PJOIN = 0x01
MT_G_PQUIT = 0x02
MT_G_LEVEL = 0x03
MT_G_CHECK = 0x04
MT_G_SCORE = 0x05
MT_G_LTYPE = 0x06
MT_L_PROP  = 0x10
MT_L_GRID  = 0x11
MT_L_STAGE = 0x12
MT_P_ANIMS = 0x20
MT_P_TEMP  = 0x21

MTL_G_PROPS = 8
MTL_G_PJOIN = 10
MTL_G_PQUIT = 3

# ── Custom lobby packet types ─────────────────────────────────────────────────

OJXB_HELLO    = 0xF0   # client -> server: player name
OJXB_ROOMLIST = 0xF1   # server -> client: room list
OJXB_JOIN     = 0xF2   # client -> server: join room_id
OJXB_ROOMINFO = 0xF3   # server -> client: room state update
OJXB_START    = 0xF4   # server -> client: game starting
OJXB_MAPSEL   = 0xF5   # host -> server: level selected (filename)

# ── Logging ───────────────────────────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("ojxb")

# ── Data model ────────────────────────────────────────────────────────────────

@dataclass
class Player:
    name:   str = "PLAYER"
    writer: Optional[asyncio.StreamWriter] = None
    slot:   int = 0
    last_rx: float = field(default_factory=time.monotonic)

    def addr(self) -> str:
        if self.writer:
            peer = self.writer.get_extra_info("peername")
            return f"{peer[0]}:{peer[1]}" if peer else "?"
        return "?"


@dataclass
class Room:
    room_id:  int
    players:  list = field(default_factory=list)   # list[Player]
    started:  bool = False
    level_name: str = ""

    def is_full(self)  -> bool: return len(self.players) >= MAX_PLAYERS
    def is_empty(self) -> bool: return len(self.players) == 0

    def host(self) -> Optional[Player]:
        return self.players[0] if self.players else None

    def others(self, player: "Player") -> list:
        return [p for p in self.players if p is not player and p.writer]


# ── Server state ──────────────────────────────────────────────────────────────

rooms:       dict[int, Room] = {}
next_room_id: int = 1


def get_or_create_room(room_id: int) -> Optional[Room]:
    """Return existing room by id, or None if it doesn't exist / is full."""
    global next_room_id
    if room_id == 0:
        # Create new room
        rid = next_room_id
        next_room_id += 1
        room = Room(room_id=rid)
        rooms[rid] = room
        log.info(f"Room {rid} created")
        return room
    room = rooms.get(room_id)
    if room and not room.is_full() and not room.started:
        return room
    return None


def remove_player(room: Room, player: Player):
    if player in room.players:
        room.players.remove(player)
    if room.is_empty():
        rooms.pop(room.room_id, None)
        log.info(f"Room {room.room_id} closed (empty)")


# ── Packet I/O ────────────────────────────────────────────────────────────────

async def read_packet(reader: asyncio.StreamReader) -> Optional[bytes]:
    try:
        header = await reader.readexactly(1)
    except (asyncio.IncompleteReadError, ConnectionResetError):
        return None
    length = header[0]
    if length < 2:
        return None
    try:
        rest = await reader.readexactly(length - 1)
    except (asyncio.IncompleteReadError, ConnectionResetError):
        return None
    return header + rest


async def send_packet(writer: asyncio.StreamWriter, data: bytes):
    try:
        writer.write(data)
        await writer.drain()
    except Exception:
        pass


async def broadcast(room: Room, sender: Player, data: bytes):
    for p in room.others(sender):
        await send_packet(p.writer, data)


# ── Lobby packet builders ─────────────────────────────────────────────────────

def build_roomlist() -> bytes:
    """
    OJXB_ROOMLIST:
      [0]  length
      [1]  OJXB_ROOMLIST
      [2]  nRooms
      per room (20 bytes each):
        [0]  room_id
        [1]  nPlayers
        [2]  max_players
        [3]  status  (0=waiting, 1=playing)
        [4..19] host name (16 bytes, null-padded)
    """
    entries = b""
    for room in rooms.values():
        if room.started:
            status = 1
        else:
            status = 0
        host_name = room.host().name if room.host() else ""
        name_bytes = host_name[:16].encode("ascii", errors="replace")
        name_bytes = name_bytes.ljust(16, b"\x00")
        entry = bytes([
            room.room_id & 0xFF,
            len(room.players),
            MAX_PLAYERS,
            status,
        ]) + name_bytes
        entries += entry
    n = len(rooms)
    length = 3 + len(entries)
    if length > 255: length = 255
    return bytes([length, OJXB_ROOMLIST, n]) + entries[:length - 3]


def build_roominfo(room: Room) -> bytes:
    """
    OJXB_ROOMINFO: sent to all players in a room when someone joins/leaves.
      [0]  length
      [1]  OJXB_ROOMINFO
      [2]  room_id
      [3]  nPlayers
      per player (17 bytes):
        [0]  slot
        [1..16] name (16 bytes, null-padded)
    """
    entries = b""
    for p in room.players:
        name_bytes = p.name[:16].encode("ascii", errors="replace").ljust(16, b"\x00")
        entries += bytes([p.slot]) + name_bytes
    length = 4 + len(entries)
    return bytes([length & 0xFF, OJXB_ROOMINFO, room.room_id, len(room.players)]) + entries


# ── OJ handshake builders ─────────────────────────────────────────────────────

def build_props(room: Room, for_slot: int) -> bytes:
    n = len(room.players)
    return bytes([MTL_G_PROPS, MT_G_PROPS, VERSION, 0, 1, MAX_PLAYERS, n, for_slot])


def build_pjoin(player: Player, slot: int) -> bytes:
    name_bytes = player.name[:16].encode("ascii", errors="replace")
    return bytes([MTL_G_PJOIN + len(name_bytes), MT_G_PJOIN, slot, slot, slot % 2]) + name_bytes


def build_pquit(slot: int) -> bytes:
    return bytes([MTL_G_PQUIT, MT_G_PQUIT, slot])


# ── Connection handler ────────────────────────────────────────────────────────

async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    peer = writer.get_extra_info("peername")
    log.info(f"Connect from {peer[0]}:{peer[1]}")

    player = Player(writer=writer)
    room:   Optional[Room] = None

    try:
        # ── Phase 1: LOBBY ────────────────────────────────────────────────────

        # Step 1: receive OJXB_HELLO (player name)
        pkt = await asyncio.wait_for(read_packet(reader), timeout=10.0)
        if pkt is None or len(pkt) < 3 or pkt[1] != OJXB_HELLO:
            log.warning(f"{peer[0]}: bad hello")
            return
        name_bytes = pkt[2:]
        player.name = name_bytes.decode("ascii", errors="replace").rstrip("\x00 ") or "PLAYER"
        log.info(f"{peer[0]}: hello as '{player.name}'")

        # Step 2: send room list
        await send_packet(writer, build_roomlist())

        # Step 3: receive OJXB_JOIN (room_id to join, 0 = create new)
        pkt = await asyncio.wait_for(read_packet(reader), timeout=30.0)
        if pkt is None or len(pkt) < 3 or pkt[1] != OJXB_JOIN:
            log.warning(f"{peer[0]}: expected join, got nothing")
            return

        requested_room_id = pkt[2]
        room = get_or_create_room(requested_room_id)
        if room is None:
            # Room full or gone -- send fresh list and bail
            await send_packet(writer, build_roomlist())
            log.warning(f"{peer[0]}: room {requested_room_id} unavailable")
            return

        player.slot = len(room.players)
        room.players.append(player)
        log.info(f"Room {room.room_id}: '{player.name}' joined as slot {player.slot}")

        # Step 4: notify everyone in the room of the new state
        ri = build_roominfo(room)
        for p in room.players:
            await send_packet(p.writer, ri)

        # ── Phase 2: WAITING -- in-room lobby ────────────────────────────────
        # MT_G_PROPS is NOT sent yet -- it goes out after host sends OJXB_MAPSEL

        # ── Phase 3: RELAY ────────────────────────────────────────────────────

        while True:
            try:
                pkt = await asyncio.wait_for(read_packet(reader), timeout=TIMEOUT_S)
            except asyncio.TimeoutError:
                log.info(f"Room {room.room_id} slot {player.slot}: timeout")
                break

            if pkt is None:
                break

            player.last_rx = time.monotonic()
            msg_type = pkt[1] if len(pkt) > 1 else 0xFF

            # Host picks a level: send OJ handshake to all then start relay
            if msg_type == OJXB_MAPSEL and player.slot == 0:
                room.started = True
                level_name = pkt[2:].decode("ascii", errors="replace").rstrip("\x00")
                room.level_name = level_name
                log.info(f"Room {room.room_id}: host picked level '{level_name}'")
                # Send OJ handshake to ALL players now
                for i, p in enumerate(room.players):
                    await send_packet(p.writer, build_props(room, i))
                    for j, q in enumerate(room.players):
                        await send_packet(p.writer, build_pjoin(q, j))
                # Send OJXB_MAPSEL to all clients (not host)
                for p in room.others(player):
                    start_pkt = bytes([2 + len(pkt) - 2, OJXB_MAPSEL]) + pkt[2:]
                    await send_packet(p.writer, start_pkt)
                continue

            # Update player name if client sends its own PJOIN
            if msg_type == MT_G_PJOIN and len(pkt) >= MTL_G_PJOIN:
                n = pkt[5:].decode("ascii", errors="replace").rstrip("\x00 ")
                if n: player.name = n
                log.info(f"Room {room.room_id} slot {player.slot}: name -> '{player.name}'")
                await broadcast(room, player, build_pjoin(player, player.slot))
                continue

            if msg_type == MT_G_LEVEL and player.slot == 0:
                room.started = True

            await broadcast(room, player, pkt)

            if msg_type not in (MT_P_TEMP, MT_P_ANIMS):
                log.debug(f"Room {room.room_id} slot {player.slot} -> 0x{msg_type:02X} ({len(pkt)}B)")

    except asyncio.TimeoutError:
        log.info(f"{peer[0]}: lobby timeout")
    except Exception as e:
        log.warning(f"{peer[0]}: {e}")
    finally:
        log.info(f"{peer[0]}: disconnected ('{player.name}')")
        try:
            writer.close()
        except Exception:
            pass
        if room:
            quit_pkt = build_pquit(player.slot)
            remove_player(room, player)
            for p in list(room.players):
                if p.writer:
                    await send_packet(p.writer, quit_pkt)
                    # Send updated room info
                    await send_packet(p.writer, build_roominfo(room))


# ── Entry point ───────────────────────────────────────────────────────────────

async def main():
    server = await asyncio.start_server(handle_client, HOST, PORT)
    addrs  = ", ".join(str(s.getsockname()) for s in server.sockets)
    log.info(f"OpenJazzXB relay server v{VERSION} on {addrs}")
    log.info(f"Max {MAX_PLAYERS} players/room, timeout {TIMEOUT_S}s")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        log.info("Stopped")
        sys.exit(0)