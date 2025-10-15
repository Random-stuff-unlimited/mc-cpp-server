#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "world/world.hpp"

void levelChunkWithLight(Packet& packet, Server& server) {
	int chunkX = 0;
	int chunkZ = 0;

	Buffer buf;

	try {
		// Use your new chunk loading system
		World::Query	 query	   = server.getWorldQuery();
		World::ChunkData chunkData = query.fetchChunk(chunkX, chunkZ);

		buf.writeByte(0x27);
		// Write chunk coordinates
		buf.writeInt(chunkX);
		buf.writeInt(chunkZ);

		// Write heightmaps (proper NBT format)
		if (!chunkData.heightmaps.empty()) {
			buf.writeBytes(chunkData.heightmaps);
		} else {
			// Empty heightmap NBT compound
			buf.writeByte(0x0A); // TAG_Compound
			buf.writeUShort(0);	 // Empty name length (must be short, not VarInt)
			buf.writeByte(0x00); // TAG_End
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
				// Block count (Short - number of non-air blocks)
				emptyData.writeUShort(0); // All air blocks

				// Block states palette
				emptyData.writeByte(0);	  // Bits per entry (single value)
				emptyData.writeVarInt(1); // Palette length (must be 1 when bpe=0)
				emptyData.writeVarInt(0); // Air block state ID (should be 0)
				emptyData.writeVarInt(0); // Data array length (no data needed for single value)

				// Biomes palette
				emptyData.writeByte(0);	  // Bits per entry (single value)
				emptyData.writeVarInt(1); // Palette length (must be 1 when bpe=0)
				emptyData.writeVarInt(0); // Safest biome ID (should always exist)
				emptyData.writeVarInt(0); // Data array length (no data needed for single value)
			}

			buf.writeVarInt(emptyData.getData().size());
			buf.writeBytes(emptyData.getData());
		}

		// Block entities
		buf.writeVarInt(0);

		// Light data (proper format)
		// Sky Light Mask
		buf.writeVarInt(1);		  // Sky light mask array length
		buf.writeLong(0x3FFFFFF); // Mask for 26 sections (all have sky light)

		// Block Light Mask
		buf.writeVarInt(1); // Block light mask array length
		buf.writeLong(0);	// No block light sections

		// Empty Sky Light Mask
		buf.writeVarInt(1); // Empty sky light mask array length
		buf.writeLong(0);	// No empty sky light sections

		// Empty Block Light Mask
		buf.writeVarInt(1);		  // Empty block light mask array length
		buf.writeLong(0x3FFFFFF); // All block light sections are empty

		// Sky light data arrays (26 sections)
		for (int i = 0; i < 26; i++) {
			buf.writeVarInt(2048); // Light array size (16x16x16 / 2)
			for (int j = 0; j < 2048; j++) {
				buf.writeByte(0xFF); // Full sky light (15 << 4 | 15)
			}
		}

		// No block light arrays since mask is 0

	} catch (const std::exception& e) {
		std::cerr << "Error in sendChunkData: " << e.what() << std::endl;
		// Return without setting packet data - this will cause the packet to be skipped
		return;
	}

	// Set packet data (no manual length encoding)
	packet.setPacketId(0x27); // Level Chunk with Light packet ID
	packet.getData() = buf;
	packet.setPacketSize(buf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}
