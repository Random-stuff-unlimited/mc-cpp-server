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

	try {
		// Use your new chunk loading system
		World::Query	 query	   = server.getWorldQuery();
		World::ChunkData chunkData = query.fetchChunk(chunkX, chunkZ);

		// Write chunk coordinates
		buf.writeInt(chunkX);
		buf.writeInt(chunkZ);

		// Write heightmaps (simplified)
		if (!chunkData.heightmaps.empty()) {
			buf.writeBytes(chunkData.heightmaps);
		} else {
			// Empty heightmap
			buf.writeByte(0x0A); // Compound tag
			buf.writeByte(0x00); // Empty name length
			buf.writeByte(0x00); // End tag
		}

		// Write chunk data
		if (!chunkData.blockData.empty()) {
			buf.writeVarInt(chunkData.blockData.size());
			buf.writeBytes(chunkData.blockData);
		} else {
			// Generate simple empty chunk inline
			Buffer	  emptyData;
			const int NUM_SECTIONS = 24;

			for (int section = 0; section < NUM_SECTIONS; section++) {
				emptyData.writeByte(0x00); // Block count low
				emptyData.writeByte(0x00); // Block count high

				// Block states - single air block
				emptyData.writeByte(0);	  // Bits per entry
				emptyData.writeVarInt(0); // Air block state
				emptyData.writeVarInt(0); // No data array

				// Biomes - single plains biome
				emptyData.writeByte(0);	  // Bits per entry
				emptyData.writeVarInt(1); // Plains biome
				emptyData.writeVarInt(0); // No data array
			}

			buf.writeVarInt(emptyData.getData().size());
			buf.writeBytes(emptyData.getData());
		}

		// Block entities
		buf.writeVarInt(0);

		// Light data (simplified version)
		buf.writeVarInt(1);		  // Sky light mask length
		buf.writeLong(0x1FFFFFF); // All sections have sky light
		buf.writeVarInt(1);		  // Block light mask length
		buf.writeLong(0);		  // No block light
		buf.writeVarInt(1);		  // Empty sky light mask length
		buf.writeLong(0);		  // No empty sky light sections
		buf.writeVarInt(1);		  // Empty block light mask length
		buf.writeLong(0);		  // No empty block light sections

		// Sky light arrays
		for (int i = 0; i < 25; i++) {
			buf.writeVarInt(2048);
			for (int j = 0; j < 2048; j++) {
				buf.writeByte(0xFF);
			}
		}

	} catch (const std::exception& e) {
		std::cerr << "Error in sendChunkData: " << e.what() << std::endl;
		// Return without setting packet data - this will cause the packet to be skipped
		return;
	}

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

// Helper function to generate empty chunk sections
Buffer generateEmptyChunkSections() {
	Buffer	  chunkData;
	const int NUM_SECTIONS = 24; // Sections for world height -64 to 319

	for (int section = 0; section < NUM_SECTIONS; section++) {
		// Block count (non-air blocks) - 0 for empty sections
		chunkData.writeByte(0x00);
		chunkData.writeByte(0x00);

		// Block states palette - single entry for air
		chunkData.writeByte(0);	  // Bits per entry (0 = single valued)
		chunkData.writeVarInt(0); // Air block state ID
		chunkData.writeVarInt(0); // No data array needed for single valued

		// Biomes palette - single entry for plains
		chunkData.writeByte(0);	  // Bits per entry (0 = single valued)
		chunkData.writeVarInt(1); // Plains biome ID
		chunkData.writeVarInt(0); // No data array needed for single valued
	}

	return chunkData;
}

// Helper function to write light data from loaded chunk
void writeLightData(Buffer& buf, const World::ChunkData& chunkData) {
	if (!chunkData.skyLight.empty() || !chunkData.blockLight.empty()) {
		// Use actual light data from chunk
		writeActualLightData(buf, chunkData);
	} else {
		// Fallback to default light data
		writeEmptyLightData(buf);
	}
}

void writeActualLightData(Buffer& buf, const World::ChunkData& chunkData) {
	// This is complex - you need to reconstruct the light data format
	// For now, fallback to empty light data
	// TODO: Implement proper light data reconstruction from chunkData.skyLight and chunkData.blockLight
	writeEmptyLightData(buf);
}

void writeEmptyLightData(Buffer& buf) {
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
			buf.writeByte(0xFF); // Full sky light
		}
	}
	// No Block Light arrays since mask is 0
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
