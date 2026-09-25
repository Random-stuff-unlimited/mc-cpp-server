#!/usr/bin/env python3
"""Fake clients for testing and benchmarking the server (Minecraft 1.21.10, offline mode).

Each bot logs in, goes through the configuration, then plays: it answers keep-alives, acknowledges chunk batches
and teleports, and walks in a circle (one movement packet per client tick, like the real client).

    python3 tools/bench_bots.py --bots 50 --duration 30
    python3 tools/bench_bots.py --bots 200 --spread 3000 --prefix a   # scattered over 3000x3000 blocks
"""

import argparse
import asyncio
import math
import random
import struct
import time
import zlib
from collections import Counter

PROTOCOL = 773

# Packet ids (see include/network/PacketIds.hpp)
LOGIN_CB_DISCONNECT, LOGIN_CB_FINISHED, LOGIN_CB_COMPRESSION = 0x00, 0x02, 0x03
LOGIN_SB_HELLO, LOGIN_SB_ACK = 0x00, 0x03
CONF_CB_DISCONNECT, CONF_CB_FINISH, CONF_CB_KEEP_ALIVE, CONF_CB_KNOWN_PACKS = 0x02, 0x03, 0x04, 0x0E
CONF_SB_CLIENT_INFO, CONF_SB_FINISH, CONF_SB_KEEP_ALIVE, CONF_SB_KNOWN_PACKS = 0x00, 0x03, 0x04, 0x07
PLAY_CB_CHUNK_BATCH_FINISHED, PLAY_CB_DISCONNECT, PLAY_CB_KEEP_ALIVE = 0x0B, 0x20, 0x2B
PLAY_CB_LEVEL_CHUNK, PLAY_CB_LOGIN, PLAY_CB_PLAYER_POSITION = 0x2C, 0x30, 0x46
PLAY_CB_SET_TIME, PLAY_CB_TICKING_STATE = 0x6F, 0x7D
PLAY_SB_ACCEPT_TELEPORT, PLAY_SB_CHUNK_BATCH_RECEIVED, PLAY_SB_KEEP_ALIVE, PLAY_SB_MOVE_POS = 0x00, 0x0A, 0x1B, 0x1D


def varint(value):
    value &= 0xFFFFFFFF
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def string(text):
    data = text.encode()
    return varint(len(data)) + data


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def varint(self):
        result = 0
        for i in range(5):
            byte = self.data[self.pos]
            self.pos += 1
            result |= (byte & 0x7F) << (7 * i)
            if not byte & 0x80:
                return result - (1 << 32) if result & 0x80000000 else result
        raise ValueError("VarInt too long")

    def string(self):
        length = self.varint()
        text = self.data[self.pos:self.pos + length].decode()
        self.pos += length
        return text

    def unpack(self, fmt):
        values = struct.unpack_from(fmt, self.data, self.pos)
        self.pos += struct.calcsize(fmt)
        return values

    def rest(self):
        return self.data[self.pos:]


class Stats:
    def __init__(self):
        self.packets = Counter()
        self.bytes = 0
        self.in_play = 0
        self.join_times = []
        self.errors = []
        self.last_time = None
        self.ticking_state = None


class Bot:
    def __init__(self, name, args, stats):
        self.name, self.args, self.stats = name, args, stats
        self.threshold = -1
        self.x = self.y = self.z = 0.0
        self.positioned = False

    async def read_packet(self):
        length = await self.read_varint()
        frame = await self.reader.readexactly(length)
        self.stats.bytes += length
        reader = Reader(frame)
        if self.threshold >= 0:
            size = reader.varint()
            body = zlib.decompress(reader.rest()) if size else reader.rest()
            if size and len(body) != size:
                raise ValueError("wrong decompressed size")
            reader = Reader(body)
        return reader.varint(), reader

    async def read_varint(self):
        result = 0
        for i in range(3):
            byte = (await self.reader.readexactly(1))[0]
            result |= (byte & 0x7F) << (7 * i)
            if not byte & 0x80:
                return result
        raise ValueError("frame length too long")

    def send(self, packet_id, payload=b""):
        body = varint(packet_id) + payload
        if self.threshold >= 0:
            if len(body) >= self.threshold:
                body = varint(len(body)) + zlib.compress(body)
            else:
                body = varint(0) + body
        self.writer.write(varint(len(body)) + body)

    async def run(self):
        start = time.monotonic()
        self.reader, self.writer = await asyncio.open_connection(self.args.host, self.args.port)
        self.send(0x00, varint(PROTOCOL) + string(self.args.host) + struct.pack(">H", self.args.port) + varint(2))
        self.send(LOGIN_SB_HELLO, string(self.name) + bytes(16))

        # Login
        while True:
            packet_id, data = await self.read_packet()
            if packet_id == LOGIN_CB_COMPRESSION:
                self.threshold = data.varint()
            elif packet_id == LOGIN_CB_FINISHED:
                self.send(LOGIN_SB_ACK)
                break
            elif packet_id == LOGIN_CB_DISCONNECT:
                raise RuntimeError("login refused: " + data.string())

        # Configuration
        self.send(CONF_SB_CLIENT_INFO, string(getattr(self, "locale", "en_us")) + bytes([self.args.view_distance]) + varint(0) + b"\x01" + b"\x7f" + varint(1) +
                  b"\x00\x01" + varint(0))
        while True:
            packet_id, data = await self.read_packet()
            if packet_id == CONF_CB_KNOWN_PACKS:
                self.send(CONF_SB_KNOWN_PACKS, data.rest())
            elif packet_id == CONF_CB_KEEP_ALIVE:
                self.send(CONF_SB_KEEP_ALIVE, data.rest())
            elif packet_id == CONF_CB_FINISH:
                self.send(CONF_SB_FINISH)
                break
            elif packet_id == CONF_CB_DISCONNECT:
                raise RuntimeError("configuration refused")

        # Play
        await self.writer.drain()
        mover = asyncio.create_task(self.move())
        try:
            while True:
                packet_id, data = await self.read_packet()
                self.stats.packets[packet_id] += 1
                if packet_id == PLAY_CB_LOGIN:
                    self.stats.in_play += 1
                    self.stats.join_times.append(time.monotonic() - start)
                elif packet_id == PLAY_CB_KEEP_ALIVE:
                    self.send(PLAY_SB_KEEP_ALIVE, data.rest())
                elif packet_id == PLAY_CB_CHUNK_BATCH_FINISHED:
                    self.send(PLAY_SB_CHUNK_BATCH_RECEIVED, struct.pack(">f", 64.0))
                elif packet_id == PLAY_CB_PLAYER_POSITION:
                    teleport = data.varint()
                    self.x, self.y, self.z = data.unpack(">ddd")
                    if self.args.spread and not self.positioned:
                        # Walks away from the spawn: the server doesn't check movements yet
                        self.x += random.uniform(-self.args.spread / 2, self.args.spread / 2)
                        self.z += random.uniform(-self.args.spread / 2, self.args.spread / 2)
                    self.positioned = True
                    self.send(PLAY_SB_ACCEPT_TELEPORT, varint(teleport))
                elif packet_id == PLAY_CB_SET_TIME:
                    self.stats.last_time = data.unpack(">qq?")
                elif packet_id == PLAY_CB_TICKING_STATE:
                    self.stats.ticking_state = data.unpack(">f?")
                elif packet_id == PLAY_CB_DISCONNECT:
                    raise RuntimeError("kicked")
                await self.writer.drain()
        finally:
            mover.cancel()

    async def move(self):
        angle, radius = 0.0, 8.0
        while True:
            await asyncio.sleep(0.05)
            if not self.positioned or not self.args.move:
                continue
            angle += 0.05
            x = self.x + math.cos(angle) * radius
            z = self.z + math.sin(angle) * radius
            self.send(PLAY_SB_MOVE_POS, struct.pack(">ddd", x, self.y, z) + b"\x01")


async def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=25565)
    parser.add_argument("--bots", type=int, default=1)
    parser.add_argument("--duration", type=float, default=10)
    parser.add_argument("--view-distance", type=int, default=4)
    parser.add_argument("--no-move", dest="move", action="store_false")
    parser.add_argument("--spread", type=float, default=0, help="scatter the bots over a square this wide (blocks)")
    parser.add_argument("--prefix", default="", help="bot name prefix, to run several processes at once")
    args = parser.parse_args()

    stats = Stats()

    async def run_bot(i):
        try:
            await Bot(f"bot{args.prefix}_{i}", args, stats).run()
        except asyncio.CancelledError:
            pass
        except Exception as e:  # noqa: BLE001 - reported at the end
            stats.errors.append(f"bot{i}: {type(e).__name__}: {e}")

    tasks = []
    for i in range(args.bots):
        tasks.append(asyncio.create_task(run_bot(i)))
        await asyncio.sleep(0.02)
    await asyncio.sleep(args.duration)
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)

    total = sum(stats.packets.values())
    print(f"bots in play: {stats.in_play}/{args.bots}")
    if stats.join_times:
        print(f"join time: avg {sum(stats.join_times) / len(stats.join_times) * 1000:.0f} ms, max {max(stats.join_times) * 1000:.0f} ms")
    print(f"received: {total} play packets, {stats.bytes / 1e6:.1f} MB ({total / args.duration:.0f} packets/s)")
    print(f"chunks: {stats.packets[PLAY_CB_LEVEL_CHUNK]}, set time: {stats.packets[PLAY_CB_SET_TIME]} (last {stats.last_time}), "
          f"ticking state: {stats.ticking_state}")
    print("top packets:", ", ".join(f"0x{pid:02X}={count}" for pid, count in stats.packets.most_common(8)))
    for error in stats.errors[:10]:
        print("error:", error)


if __name__ == "__main__":
    asyncio.run(main())
