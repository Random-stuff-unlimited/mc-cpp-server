#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

std::vector<uint8_t> encodeVarInt(int32_t value) {
	std::vector<uint8_t> output;
	uint32_t			 uvalue = static_cast<uint32_t>(value);

	while (uvalue >= 0x80) {
		output.push_back(static_cast<uint8_t>((uvalue & 0x7F) | 0x80));
		uvalue >>= 7;
	}
	output.push_back(static_cast<uint8_t>(uvalue & 0x7F));

	return output;
}

void levelChunkWithLight(Packet& packet, Server& server, int32_t chunkX, int32_t chunkZ) {
	try {
		// Load the specific chunk from world
		auto chunkData = server.getWorldQuery().fetchChunkDirect(chunkX, chunkZ);

		Buffer buf;
		buf.writeByte(0x27); // Packet ID

		// Write chunk coordinates
		buf.writeInt(chunkData.chunkX);
		buf.writeInt(chunkData.chunkZ);

		// Write heightmaps
		auto heightmapData	= chunkData.heightmaps.serializeToNBT();
		auto availableTypes = chunkData.heightmaps.getAvailableTypes();
		buf.writeVarInt(static_cast<int>(availableTypes.size())); // Count of heightmaps
		for (const auto& heightmapType : availableTypes) {
			buf.writeVarInt(static_cast<int>(heightmapType)); // Use the enum value as dataType
			auto packedData = chunkData.heightmaps.packHeightMap(heightmapType);
			buf.writeVarInt(static_cast<int>(packedData.size()));
			for (uint64_t value : packedData) {
				buf.writeUnsignedLong(value);
			}
		}

		// Calculate chunk section data
		std::vector<uint8_t> chunkSectionData;

		for (int sectionIndex = 0; sectionIndex < 24; sectionIndex++) {
			const auto& section = chunkData.sections[sectionIndex];

			// Write block count (2 bytes, big-endian)
			chunkSectionData.push_back((section.blockCount >> 8) & 0xFF);
			chunkSectionData.push_back(section.blockCount & 0xFF);

			// Write block states for ALL sections (including empty ones)
			if (section.blockStates && !section.isEmpty) {
				auto blockStateData = section.blockStates->serialize();
				chunkSectionData.insert(chunkSectionData.end(), blockStateData.begin(), blockStateData.end());
			} else {
				// Empty section - single-valued palette with air
				chunkSectionData.push_back(0); // bits per entry = 0 (single valued)
				// For single-valued, write the value directly as VarInt (no palette array)
				auto airIdBytes = encodeVarInt(0); // Air block ID = 0
				chunkSectionData.insert(chunkSectionData.end(), airIdBytes.begin(), airIdBytes.end());
				// No data array for single-valued (0 bits per entry means no data needed)
			}

			// Write biomes for ALL sections
			if (section.biomes && !section.isEmpty) {
				auto biomeData = section.biomes->serialize();
				chunkSectionData.insert(chunkSectionData.end(), biomeData.begin(), biomeData.end());
			} else {
				// Default single-valued biome palette
				chunkSectionData.push_back(0);		  // bits per entry = 0
				auto plainsIdBytes = encodeVarInt(1); // Plains biome ID = 1
				chunkSectionData.insert(chunkSectionData.end(), plainsIdBytes.begin(), plainsIdBytes.end());
			}
		}

		// Write chunk data size and data
		buf.writeVarInt(static_cast<int>(chunkSectionData.size()));
		buf.writeBytes(chunkSectionData);

		// Write block entities
		buf.writeVarInt(static_cast<int>(chunkData.blockEntities.size()));
		for (const auto& blockEntity : chunkData.blockEntities) {
			buf.writeByte(blockEntity.packedXZ);
			buf.writeUShort(static_cast<uint16_t>(blockEntity.y));
			buf.writeVarInt(blockEntity.type);
			buf.writeBytes(blockEntity.nbtData);
		}

		// Write light data
		// Sky light update mask (bitmask for sections with sky light)
		uint64_t skyLightMask	= 0;
		uint64_t blockLightMask = 0;

		for (size_t i = 0; i < chunkData.sections.size(); i++) {
			if (chunkData.sections[i].hasSkyLight) {
				skyLightMask |= (1ULL << i);
			}
			if (chunkData.sections[i].hasBlockLight) {
				blockLightMask |= (1ULL << i);
			}
		}

		// Bitsets are: VarInt(array_length) + array_of_ulongs
		// Sky light mask
		buf.writeVarInt(1);					 // Bitset length = 1
		buf.writeUnsignedLong(skyLightMask); // 64-bit mask as ULong

		// Block light mask
		buf.writeVarInt(1);					   // Bitset length = 1
		buf.writeUnsignedLong(blockLightMask); // 64-bit mask as ULong

		// Empty sky light mask
		buf.writeVarInt(1);		  // Bitset length = 1
		buf.writeUnsignedLong(0); // No empty sections

		// Empty block light mask
		buf.writeVarInt(1);		  // Bitset length = 1
		buf.writeUnsignedLong(0); // No empty sections

		std::vector<std::vector<uint8_t>> skyLightArraysToWrite;
		for (size_t i = 0; i < chunkData.sections.size(); i++) {
			if (chunkData.sections[i].hasSkyLight && !chunkData.sections[i].skyLight.empty()) {
				skyLightArraysToWrite.push_back(chunkData.sections[i].skyLight);
			}
		}

		// Write sky light array count
		buf.writeVarInt(static_cast<int>(skyLightArraysToWrite.size()));

		// Write each sky light array
		for (const auto& lightArray : skyLightArraysToWrite) {
			buf.writeVarInt(static_cast<int>(lightArray.size()));
			buf.writeBytes(lightArray);
		}

		// Same for block light arrays
		std::vector<std::vector<uint8_t>> blockLightArraysToWrite;
		for (size_t i = 0; i < chunkData.sections.size(); i++) {
			if (chunkData.sections[i].hasBlockLight && !chunkData.sections[i].blockLight.empty()) {
				blockLightArraysToWrite.push_back(chunkData.sections[i].blockLight);
			}
		}

		buf.writeVarInt(static_cast<int>(blockLightArraysToWrite.size()));
		for (const auto& lightArray : blockLightArraysToWrite) {
			buf.writeVarInt(static_cast<int>(lightArray.size()));
			buf.writeBytes(lightArray);
		}

		// Create final buffer with length prefix
		Buffer final;
		final.writeVarInt(buf.getData().size());
		final.writeBytes(buf.getData());

		// Set packet data
		packet.setPacketId(0x27);
		packet.getData() = final;
		packet.setPacketSize(final.getData().size());
		packet.setReturnPacket(PACKET_SEND);

		g_logger->logGameInfo(INFO,
							  "Sent levelChunkWithLight packet for chunk (" + std::to_string(chunkData.chunkX) + ", " +
									  std::to_string(chunkData.chunkZ) + ")",
							  "PACKET");

	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Error in levelChunkWithLight: " + std::string(e.what()), "PACKET");

		// Send empty chunk as fallback
		Buffer buf;
		buf.writeByte(0x27);	 // Packet ID
		buf.writeVarInt(chunkX); // chunkX parameter
		buf.writeVarInt(chunkZ); // chunkZ parameter
		buf.writeVarInt(0);		 // empty heightmaps
		buf.writeVarInt(0);		 // empty chunk data
		buf.writeVarInt(0);		 // no block entities
		buf.writeVarLong(0);	 // no sky light
		buf.writeVarLong(0);	 // no block light
		buf.writeVarLong(0);	 // empty sky light mask
		buf.writeVarLong(0);	 // empty block light mask

		// Create final buffer with length prefix
		Buffer final;
		final.writeVarInt(buf.getData().size());
		final.writeBytes(buf.getData());

		packet.setPacketId(0x27);
		packet.getData() = final;
		packet.setPacketSize(final.getData().size());
		packet.setReturnPacket(PACKET_SEND);
	}
}
