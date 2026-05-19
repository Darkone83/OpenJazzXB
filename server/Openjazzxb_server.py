#!/usr/bin/env python3
"""
openjazzxb_server.py -- OpenJazzXB relay server v4

The relay is the true middleman. It owns the handshake.

LOBBY PHASE (custom 0xF0-0xFF packets):
  Relay handles rooms, names, MAPSEL. Proven working from v1.

HANDSHAKE PHASE (triggered by host MAPSEL):
  Relay sends to CLIENT only:
    1. OJXB_MAPSEL  -- signals client to launch ClientGame
    2. MT_G_PROPS   -- built by relay with correct clientID and M_COOP
    3. MT_G_PJOIN   -- one per player in room, built by relay

  Host is NOT sent these -- it runs ServerGame which manages its own state.

GAME PHASE (after handshake):
  Pure transparent bidirectional forward. No inspection, no rebuilding.
    Host -> Client: all packets verbatim
    Client -> Host: all packets verbatim

  The relay drops OJ handshake packets that the ServerGame pump sends
  (MT_G_PROPS, MT_G_PJOIN, MT_G_LEVEL) to avoid confusing the client.

Custom lobby packets (0xF0-0xFF):
  0xF0  OJXB_HELLO    [len][0xF0][name\\0]
  0xF1  OJXB_ROOMLIST [len][0xF1][nRooms][entries...]
  0xF2  OJXB_JOIN     [len][0xF2][room_id]  (0=create)
  0xF3  OJXB_ROOMINFO [len][0xF3][room_id][nPlayers][slot+name*17 ...]
  0xF5  OJXB_MAPSEL   [len][0xF5][difficulty][levelfile\\0]

OJ framing: buffer[0] = total length (min 2).

MT_G_PROPS layout (8 bytes):
  [8][0x00][version=1][modeType][difficulty][maxPlayers][nPlayers][clientID]

MT_G_PJOIN layout (10 + namelen bytes):
  [len][0x01][clientID][playerSlot][team][col0][col1][col2][col3][name+\\0]

Usage:
  python3 openjazzxb_server.py [--port N] [--log-level info|debug|trace]
"""

import argparse
import asyncio
import logging
import sys
from dataclasses import dataclass, field
from typing import Optional

# ── OJ constants ──────────────────────────────────────────────────────────────

VERSION      = 4
MT_G_PROPS   = 0x00
MT_G_PJOIN   = 0x01
MT_G_PQUIT   = 0x02
MT_G_LEVEL   = 0x03
MT_G_LTYPE   = 0x06
MT_P_TEMP    = 0x21

MTL_G_PROPS  = 8
MTL_G_PJOIN  = 10

OJXB_HELLO    = 0xF0
OJXB_ROOMLIST = 0xF1
OJXB_JOIN     = 0xF2
OJXB_ROOMINFO = 0xF3
OJXB_MAPSEL   = 0xF5

M_COOP = 1   # GameModeType: matches OJ ServerGame

# OJ game packet type table for debug logging
PNAMES = {
    0x00:"MT_G_PROPS",  0x01:"MT_G_PJOIN",  0x02:"MT_G_PQUIT",
    0x03:"MT_G_LEVEL",  0x04:"MT_G_CHECK",  0x05:"MT_G_SCORE",
    0x06:"MT_G_LTYPE",  0x07:"MT_G_STAGE",
    0x21:"MT_P_TEMP",   0x22:"MT_P_ANIMS",
    0x40:"MT_L_GRID",   0x41:"MT_L_PROP",   0x42:"MT_L_STAGE",
    0xF0:"OJXB_HELLO",  0xF1:"OJXB_ROOMLIST", 0xF2:"OJXB_JOIN",
    0xF3:"OJXB_ROOMINFO", 0xF5:"OJXB_MAPSEL",
}
def pname(b): return PNAMES.get(b, f"0x{b:02X}")

# ── CLI ───────────────────────────────────────────────────────────────────────

def parse_args():
    p = argparse.ArgumentParser(description="OpenJazzXB relay server v4")
    p.add_argument("--host",          default="0.0.0.0")
    p.add_argument("--port",          default=10052,  type=int)
    p.add_argument("--max-players",   default=2,      type=int)
    p.add_argument("--lobby-timeout", default=120.0,  type=float)
    p.add_argument("--game-timeout",  default=60.0,   type=float)
    p.add_argument("--log-level",     default="info",
                   choices=["info","debug","trace"])
    return p.parse_args()

# ── Logging ───────────────────────────────────────────────────────────────────

TRACE = 5
logging.addLevelName(TRACE, "TRACE")
logging.Logger.trace = lambda self,m,*a,**k: \
    self._log(TRACE,m,a,**k) if self.isEnabledFor(TRACE) else None
logging.basicConfig(level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(message)s", datefmt="%H:%M:%S")
log = logging.getLogger("ojxb")

def set_log_level(s):
    lv = {"info":logging.INFO,"debug":logging.DEBUG,"trace":TRACE}[s]
    log.setLevel(lv); logging.getLogger().setLevel(lv)

def log_pkt(arrow, room_id, name, pkt, note=""):
    if not log.isEnabledFor(logging.DEBUG) or not pkt: return
    tb  = pkt[1] if len(pkt) > 1 else 0
    tag = f" [{note}]" if note else ""
    log.debug(f"Room {room_id} {name:10s} {arrow} {pname(tb):<16s} {len(pkt):4d}B{tag}")
    if log.isEnabledFor(TRACE):
        log.trace("  " + " ".join(f"{b:02X}" for b in pkt[:64])
                  + ("..." if len(pkt) > 64 else ""))

# ── Data model ────────────────────────────────────────────────────────────────

@dataclass
class Player:
    name:   str = "PLAYER"
    slot:   int = 0
    writer: Optional[asyncio.StreamWriter] = None
    reader: Optional[asyncio.StreamReader] = None

@dataclass
class Room:
    room_id: int
    players: list = field(default_factory=list)
    started: bool = False
    def is_full(self):   return len(self.players) >= MAX_PLAYERS
    def is_empty(self):  return len(self.players) == 0
    def host(self):      return self.players[0] if self.players else None
    def others(self, p): return [x for x in self.players if x is not p and x.writer]

rooms:         dict  = {}
next_room_id:  int   = 1
MAX_PLAYERS:   int   = 2
LOBBY_TIMEOUT: float = 120.0
GAME_TIMEOUT:  float = 60.0

def alloc_room():
    global next_room_id
    r = Room(room_id=next_room_id); next_room_id += 1
    rooms[r.room_id] = r
    log.info(f"Room {r.room_id} created")
    return r

def remove_player(room, player):
    if player in room.players: room.players.remove(player)
    if room.is_empty():
        rooms.pop(room.room_id, None)
        log.info(f"Room {room.room_id} closed")

# ── Packet I/O ────────────────────────────────────────────────────────────────

async def read_packet(reader, timeout=None):
    try:
        if timeout is not None:
            hdr = await asyncio.wait_for(reader.readexactly(1), timeout=timeout)
        else:
            hdr = await reader.readexactly(1)
    except: return None
    length = hdr[0]
    if length < 2: return None
    try:
        rest = await reader.readexactly(length - 1)
    except: return None
    return hdr + rest

async def write_packet(writer, data):
    try: writer.write(data); await writer.drain()
    except: pass

# ── Lobby builders ────────────────────────────────────────────────────────────

def build_roomlist():
    entries = b""
    for room in rooms.values():
        hn = (room.host().name if room.host() else "")[:16]
        entries += bytes([room.room_id & 0xFF, len(room.players),
                          MAX_PLAYERS, 1 if room.started else 0]) \
                   + hn.encode("ascii","replace").ljust(16, b"\x00")
    n  = len(rooms)
    ln = min(3 + len(entries), 255)
    return bytes([ln, OJXB_ROOMLIST, n]) + entries[:ln-3]

def build_roominfo(room):
    entries = b""
    for p in room.players:
        entries += bytes([p.slot]) + \
                   p.name[:16].encode("ascii","replace").ljust(16, b"\x00")
    ln = 4 + len(entries)
    return bytes([ln & 0xFF, OJXB_ROOMINFO, room.room_id,
                  len(room.players)]) + entries

def build_pquit(slot):
    return bytes([3, MT_G_PQUIT, slot])

# ── Handshake builders ────────────────────────────────────────────────────────

def build_props(room, for_slot):
    """
    MT_G_PROPS sent by relay to client after MAPSEL.
    [8][0x00][version=1][M_COOP][difficulty=1][MAX_PLAYERS][nPlayers][clientID]
    M_COOP=1 matches OJ ServerGame -- gives client multiplayer=true.
    clientID=for_slot so client can identify its own PJOIN.
    """
    n = len(room.players)
    return bytes([MTL_G_PROPS, MT_G_PROPS, 1, M_COOP, 1, MAX_PLAYERS, n, for_slot])

def build_pjoin(player, slot):
    """
    MT_G_PJOIN sent by relay to client.

    OpenJazz length rule:
      buffer[0] = MTL_G_PJOIN + strlen(name)

    The trailing NUL is sent, but it is already accounted for by
    the fixed MTL_G_PJOIN base size.
    """
    name = player.name[:15].encode("ascii", "replace")
    name_bytes = name + b"\x00"

    pkt = bytes([
        MTL_G_PJOIN + len(name),  # NOT len(name_bytes)
        MT_G_PJOIN,
        slot,        # clientID
        slot,        # playerSlot
        slot % 2,    # team
        0, 0, 0, 0,  # colours
    ]) + name_bytes

    assert len(pkt) == pkt[0], f"PJOIN length mismatch: len={len(pkt)} pkt[0]={pkt[0]}"
    return pkt

# ── Game phase forward filter ─────────────────────────────────────────────────

# Packet types the ServerGame pump sends that the client must NOT receive --
# it already got the relay-built versions of these, and a second copy would
# corrupt nPlayers and localPlayer.
_HOST_SUPPRESS = {MT_G_PROPS, MT_G_PJOIN}

def should_suppress_host_pkt(pkt):
    """Return True if this host->client packet should be dropped in game phase."""
    if len(pkt) < 2: return False
    return pkt[1] in _HOST_SUPPRESS

# ── Connection handler ────────────────────────────────────────────────────────

async def handle_client(reader, writer):
    peer = writer.get_extra_info("peername")
    log.info(f"Connect: {peer[0]}:{peer[1]}")
    player = Player(writer=writer, reader=reader)
    room   = None

    try:
        # ── HELLO ─────────────────────────────────────────────────────────────
        pkt = await read_packet(reader, timeout=15.0)
        if not pkt or len(pkt) < 3 or pkt[1] != OJXB_HELLO:
            log.warning(f"{peer[0]}: bad HELLO"); return
        player.name = pkt[2:].rstrip(b"\x00 ").decode("ascii","replace") or "PLAYER"
        log.info(f"{peer[0]}: HELLO as '{player.name}'")

        # ── ROOMLIST + wait for JOIN ───────────────────────────────────────────
        await write_packet(writer, build_roomlist())
        pkt = None
        deadline = asyncio.get_event_loop().time() + LOBBY_TIMEOUT
        while asyncio.get_event_loop().time() < deadline:
            try:
                pkt = await asyncio.wait_for(read_packet(reader), timeout=2.0)
                if pkt: break
            except asyncio.TimeoutError:
                await write_packet(writer, build_roomlist())
        if not pkt or pkt[1] != OJXB_JOIN:
            log.warning(f"{peer[0]}: no JOIN"); return

        # ── Assign room ───────────────────────────────────────────────────────
        rid = pkt[2]
        if rid == 0:
            room = alloc_room()
        else:
            room = rooms.get(rid)
            if not room or room.is_full() or room.started:
                await write_packet(writer, build_roomlist())
                log.warning(f"{peer[0]}: room {rid} unavailable"); return

        player.slot = len(room.players)
        room.players.append(player)
        log.info(f"Room {room.room_id}: '{player.name}' joined as slot {player.slot}")

        ri = build_roominfo(room)
        for p in room.players: await write_packet(p.writer, ri)

        # ── Pre-game wait (v1 lobby -- proven working) ────────────────────────
        if player.slot == 0:
            # HOST: wait for MAPSEL
            while True:
                pkt = await read_packet(reader, timeout=5.0)
                if pkt is None:
                    if reader.at_eof(): return
                    # timeout -- refresh roominfo to clients
                    for p in room.others(player):
                        await write_packet(p.writer, build_roominfo(room))
                    continue

                if pkt[1] == OJXB_MAPSEL:
                    room.started = True
                    diff    = pkt[2] if len(pkt) > 2 else 0
                    lvname  = pkt[3:].rstrip(b"\x00").decode("ascii","replace")
                    log.info(f"Room {room.room_id}: MAPSEL '{lvname}' difficulty={diff}")

                    # Send MAPSEL to clients first so they launch ClientGame
                    for p in room.others(player):
                        await write_packet(p.writer, pkt)

                    # Relay builds and sends OJ handshake to each client
                    # Host does NOT need this -- it runs ServerGame
                    for i, p in enumerate(room.players):
                        if i == 0:
                            continue  # skip host
                        log.info(f"Room {room.room_id}: sending handshake to slot {i} '{p.name}'")
                        await write_packet(p.writer, build_props(room, i))
                        for j, q in enumerate(room.players):
                            pjoin = build_pjoin(q, j)
                            log_pkt("->", room.room_id, p.name, pjoin, note=f"PJOIN slot {j}")
                            await write_packet(p.writer, pjoin)
                    break

                # Any other lobby packet -- refresh roominfo
                ri = build_roominfo(room)
                for p in room.players: await write_packet(p.writer, ri)

        else:
            # CLIENT: wait for host MAPSEL
            while not room.started:
                await asyncio.sleep(0.1)
                if room.is_empty() or not room.host(): return

        # ── Game phase ────────────────────────────────────────────────────────
        log.info(f"Room {room.room_id}: slot {player.slot} '{player.name}' entering game relay")

        others = room.others(player)
        if not others: return
        other = others[0]

        if player.slot == 0:
            # HOST -> CLIENT: forward with suppression of handshake packets
            # that the ServerGame pump will resend (client already has relay versions)
            suppressed = 0
            while True:
                pkt = await read_packet(reader, timeout=GAME_TIMEOUT)
                if pkt is None:
                    log.info(f"Room {room.room_id}: host '{player.name}' disconnected")
                    return
                log_pkt("H>C", room.room_id, player.name, pkt)
                if should_suppress_host_pkt(pkt):
                    suppressed += 1
                    log_pkt("DROP", room.room_id, player.name, pkt)
                    continue
                await write_packet(other.writer, pkt)
        else:
            # CLIENT -> HOST: verbatim, no filtering
            while True:
                pkt = await read_packet(reader, timeout=GAME_TIMEOUT)
                if pkt is None:
                    log.info(f"Room {room.room_id}: client '{player.name}' disconnected")
                    return
                log_pkt("C>H", room.room_id, player.name, pkt)
                await write_packet(other.writer, pkt)

    except Exception as e:
        log.warning(f"{peer[0]}: {e}", exc_info=log.isEnabledFor(logging.DEBUG))
    finally:
        log.info(f"{peer[0]}: cleanup ('{player.name}')")
        try: writer.close()
        except: pass
        if room and player in room.players:
            slot = player.slot
            remove_player(room, player)
            for p in list(room.players):
                if p.writer: await write_packet(p.writer, build_pquit(slot))

# ── Entry point ───────────────────────────────────────────────────────────────

async def main(args):
    global MAX_PLAYERS, LOBBY_TIMEOUT, GAME_TIMEOUT
    MAX_PLAYERS   = args.max_players
    LOBBY_TIMEOUT = args.lobby_timeout
    GAME_TIMEOUT  = args.game_timeout
    set_log_level(args.log_level)

    srv   = await asyncio.start_server(handle_client, args.host, args.port)
    addrs = ", ".join(str(s.getsockname()) for s in srv.sockets)
    log.info(f"OpenJazzXB relay server v{VERSION} on {addrs}")
    log.info(f"players/room={MAX_PLAYERS}  lobby={LOBBY_TIMEOUT}s  "
             f"game={GAME_TIMEOUT}s  log={args.log_level}")
    log.info("Handshake: relay owns MT_G_PROPS + MT_G_PJOIN, sent at MAPSEL time")
    log.info("Game phase: H->C verbatim (handshake types suppressed), C->H verbatim")
    async with srv:
        await srv.serve_forever()

if __name__ == "__main__":
    args = parse_args()
    try:
        asyncio.run(main(args))
    except KeyboardInterrupt:
        log.info("Stopped"); sys.exit(0)