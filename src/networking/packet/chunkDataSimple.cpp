#include "UUID.hpp"
#include "buffer.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <iostream>

void sendChunkData(Packet& packet, Server& server, int chunkX, int chunkZ) {
	Buffer buf;

	// --- Coordinates ---
	buf.writeInt(chunkX);
	buf.writeInt(chunkZ);

	// --- Heightmaps (NBT minimal valide) ---
	Buffer nbt;
	nbt.writeByte(0x0A); // TAG_Compound
	nbt.writeShort(0);   // name length = 0
	nbt.writeByte(0x00); // TAG_End
	buf.writeBytes(nbt.getData());

	// --- Chunk Sections ---
	Buffer chunkData;
	const int SECTION_COUNT = 24;

	for (int sectionY = 0; sectionY < SECTION_COUNT; ++sectionY) {
		bool hasDirt = (sectionY == 4);

		if (hasDirt) {
			chunkData.writeShort(4096);                     // non-air count
			chunkData.writeByte(8);                         // bits per block
			chunkData.writeVarInt(2);                       // palette length
			chunkData.writeVarInt(0);                       // air
			chunkData.writeVarInt(3);                       // dirt
			chunkData.writeVarInt(16 * 16 * 16 / (64 / 8)); // data array length

			for (int i = 0; i < 4096; i++) {
				chunkData.writeByte(1); // palette index for dirt
			}
		} else {
			chunkData.writeShort(0);
			chunkData.writeByte(0);
			chunkData.writeVarInt(1);
			chunkData.writeVarInt(0);
			chunkData.writeVarInt(0);
		}

		// Biomes
		chunkData.writeByte(0);
		chunkData.writeVarInt(1);
		chunkData.writeVarInt(1);
		chunkData.writeVarInt(0);
	}

	buf.writeVarInt(chunkData.getData().size());
	buf.writeBytes(chunkData.getData());

	// No block entities
	buf.writeVarInt(0);

	// Minimal light data
	buf.writeVarInt(0); // sky mask
	buf.writeVarInt(0); // block mask
	buf.writeVarInt(0); // empty sky mask
	buf.writeVarInt(0); // empty block mask

	// --- Finalize packet ---
	int packetId = 0x27;

	Buffer payload;
	payload.writeVarInt(packetId);     // packet ID first
	payload.writeBytes(buf.getData()); // then packet data

	Buffer finalBuf;
	finalBuf.writeVarInt(payload.getData().size()); // total length prefix
	finalBuf.writeBytes(payload.getData());

	packet.getData() = finalBuf;
	packet.setPacketId(packetId);
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	std::cout << "Chunk (" << chunkX << ", " << chunkZ
	          << ") built, size = " << finalBuf.getData().size() << " bytes\n";
}

void sendPlayerPositionAndLook(Packet& packet, Server& server) {
	std::cout << "=== Sending Player Position and Look ===\n";

	Buffer buf;

	// Teleport ID
	buf.writeVarInt(1);

	// Player position (double encoded via int64_t bits)
	int64_t x_bits = 0x3FE0000000000000; // 0.5
	int64_t y_bits = 0x4050000000000000; // 64.0
	int64_t z_bits = 0x3FE0000000000000; // 0.5

	buf.writeLong(x_bits);
	buf.writeLong(y_bits);
	buf.writeLong(z_bits);

	// Velocity (all zero)
	buf.writeLong(0);
	buf.writeLong(0);
	buf.writeLong(0);

	// Rotation (using int for float storage)
	buf.writeInt(0);
	buf.writeInt(0);

	// Flags (0x00 = absolute positioning)
	buf.writeInt(0x00);

	// --- Finalize packet ---
	int packetId = 0x41;

	Buffer payload;
	payload.writeVarInt(packetId);     // write packet ID first
	payload.writeBytes(buf.getData()); // then write packet data

	// The length prefix is the size of payload (packetId + data)
	Buffer finalBuf;
	finalBuf.writeVarInt(payload.getData().size());
	finalBuf.writeBytes(payload.getData());

	packet.getData() = finalBuf;
	packet.setPacketId(packetId);
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}

void sendSpawnPosition(Packet& packet, Server& server) {
	std::cout << "=== Sending Spawn Position ===\n";

	Buffer buf;

	// Encode position as long (X=0, Y=64, Z=0 packed into 64 bits)
	int64_t encodedPos = ((int64_t)0 << 38) | ((int64_t)0 << 12) | (64 & 0xFFF);
	buf.writeLong(encodedPos);

	// Spawn angle (0.0f as int bits)
	buf.writeInt(0);

	// --- Finalize packet ---
	int packetId = 0x5A;

	Buffer payload;
	payload.writeVarInt(packetId);
	payload.writeBytes(buf.getData());

	Buffer finalBuf;
	finalBuf.writeVarInt(payload.getData().size());
	finalBuf.writeBytes(payload.getData());

	packet.getData() = finalBuf;
	packet.setPacketId(packetId);
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}
