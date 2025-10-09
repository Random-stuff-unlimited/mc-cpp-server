#include "lib/UUID.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <iostream>

void sendChunkData(Packet& packet, Server& server, int chunkX, int chunkZ) {
	std::cout << "=== Sending Chunk Data (" << chunkX << ", " << chunkZ << ") ===\n";

	Buffer buf;

	// Chunk X and Z coordinates
	buf.writeInt(chunkX);
	buf.writeInt(chunkZ);

	// Heightmaps (NBT) - minimal implementation using empty compound tag
	buf.writeByte(0x0A);
	buf.writeByte(0x00);
	buf.writeByte(0x00);
	buf.writeByte(0x00);

	// Chunk data array - simplified version
	Buffer chunkData;

	// Number of sections (24 sections for world height -64 to 319)
	const int NUM_SECTIONS = 24;

	for (int section = 0; section < NUM_SECTIONS; section++) {
		// Block count (non-air blocks in this section) - write as 2 bytes for short
		buf.writeByte(0x00);
		buf.writeByte(0x00);

		// Block states palette
		chunkData.writeByte(0);
		chunkData.writeVarInt(0);
		chunkData.writeVarInt(0);

		// Biomes palette
		chunkData.writeByte(0);
		chunkData.writeVarInt(1);
		chunkData.writeVarInt(0);
	}

	// Write chunk data size and data
	buf.writeVarInt(chunkData.getData().size());
	buf.writeBytes(chunkData.getData());

	// Number of block entities
	buf.writeVarInt(0);

	// Light data - simplified version
	// Sky Light Mask (BitSet) - all sections have sky light
	buf.writeVarInt(1);
	buf.writeLong(0x1FFFFFF);

	// Block Light Mask (BitSet) - no block light
	buf.writeVarInt(1);
	buf.writeLong(0);

	// Empty Sky Light Mask
	buf.writeVarInt(1);
	buf.writeLong(0);

	// Empty Block Light Mask
	buf.writeVarInt(1);
	buf.writeLong(0);

	// Sky Light arrays (2048 bytes each for sections with sky light)
	for (int i = 0; i < 25; i++) {
		buf.writeVarInt(2048);
		for (int j = 0; j < 2048; j++) {
			buf.writeByte(0xFF);
		}
	}

	// No Block Light arrays since mask is 0

	// Create final packet
	int packetId		 = 0x27;
	int packetIdSize	 = packet.getVarintSize(packetId);
	int totalPayloadSize = packetIdSize + buf.getData().size();

	Buffer finalBuf;
	finalBuf.writeVarInt(totalPayloadSize);
	finalBuf.writeVarInt(packetId);
	finalBuf.writeBytes(buf.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}

void sendPlayerPositionAndLook(Packet& packet, Server& server) {
	std::cout << "=== Sending Player Position and Look ===\n";

	Buffer buf;

	// Teleport ID
	buf.writeVarInt(1);

	// Player position (using writeLong for double precision storage)
	// Note: This is a simplified approach - ideally you'd add writeDouble to Buffer
	int64_t x_bits = 0x3FE0000000000000;
	int64_t y_bits = 0x4050000000000000;
	int64_t z_bits = 0x3FE0000000000000;

	buf.writeLong(x_bits);
	buf.writeLong(y_bits);
	buf.writeLong(z_bits);

	// Velocity (all zero)
	buf.writeLong(0);
	buf.writeLong(0);
	buf.writeLong(0);

	// Rotation (using writeInt for float storage)
	buf.writeInt(0);
	buf.writeInt(0);

	// Flags (0x00 = absolute positioning)
	buf.writeInt(0x00);

	int packetId		 = 0x41;
	int packetIdSize	 = packet.getVarintSize(packetId);
	int totalPayloadSize = packetIdSize + buf.getData().size();

	Buffer finalBuf;
	finalBuf.writeVarInt(totalPayloadSize);
	finalBuf.writeVarInt(packetId);
	finalBuf.writeBytes(buf.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}

void sendSpawnPosition(Packet& packet, Server& server) {
	std::cout << "=== Sending Spawn Position ===\n";

	Buffer buf;

	// Encode position as long (X=0, Y=64, Z=0 packed into 64 bits)
	// Position format: ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF)
	int64_t encodedPos = ((int64_t)0 << 38) | ((int64_t)0 << 12) | (64 & 0xFFF);
	buf.writeLong(encodedPos);

	// Spawn angle (0.0f as int bits)
	buf.writeInt(0);

	int packetId		 = 0x5A; // Set Default Spawn Position packet ID for protocol 770
	int packetIdSize	 = packet.getVarintSize(packetId);
	int totalPayloadSize = packetIdSize + buf.getData().size();

	Buffer finalBuf;
	finalBuf.writeVarInt(totalPayloadSize);
	finalBuf.writeVarInt(packetId);
	finalBuf.writeBytes(buf.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}
