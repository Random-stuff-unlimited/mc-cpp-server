#include "lib/nbt.hpp"
#include "lib/nbtParser.hpp"
#include "logger.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

World::ChunkData World::Query::loadChunkFromRegion(const std::filesystem::path& regionPath, int chunkX, int chunkZ) {

	// Get the byte index inside the location table
	int localX = chunkX & 31, localZ = chunkZ & 31;
	int chunkIndex = (localZ << 5) + localX;

	std::ifstream file(regionPath, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open region file");
	}

	file.seekg(chunkIndex * 4);
	uint32_t locationData;
	file.read(reinterpret_cast<char*>(&locationData), 4);
	locationData = ntohl(locationData);

	uint32_t offset		 = (locationData >> 8) & 0xFFFFFF;
	uint8_t	 sectorCount = locationData & 0xFF;

	if (offset == 0 || sectorCount == 0) {
		// Chunk doesn't exist in this region
		return ChunkData(chunkX, chunkZ);
	}

	file.seekg(offset * 4096);

	uint32_t chunkLength;
	file.read(reinterpret_cast<char*>(&chunkLength), 4);
	chunkLength = ntohl(chunkLength);

	uint8_t compressionType;
	file.read(reinterpret_cast<char*>(&compressionType), 1);

	std::vector<uint8_t> compressedData(chunkLength - 1);
	file.read(reinterpret_cast<char*>(compressedData.data()), chunkLength - 1);

	std::vector<uint8_t> decompressedData;
	switch (compressionType) {
	case 1: // GZip
		decompressedData = World::Manager::decompressGzip(compressedData);
		break;
	case 2: // Zlib
		decompressedData = World::Manager::decompressZlib(compressedData);
		break;
	case 3: // Uncompressed
		decompressedData = compressedData;
		break;
	default:
		throw std::runtime_error("Unknown compression type: " + std::to_string(compressionType));
	};

	nbt::Parser parser;
	nbt::NBT	chunkNBT = parser.parse(decompressedData);

	ChunkData chunk(chunkX, chunkZ);
	extractChunkDataFromNBT(chunkNBT, chunk);

	return chunk;
}

void World::Query::extractChunkDataFromNBT(const nbt::NBT& chunkNBT, World::ChunkData& chunk) {
	try {
		const auto& root = chunkNBT.getRoot();

		// Modern format (1.18+) - data directly in root

		// Extract sections (block data)
		if (root.contains("sections")) {
			const auto& sectionsTag = root.at("sections").get<std::shared_ptr<nbt::TagList>>();
			extractSectionsData(*sectionsTag, chunk);
		}

		// Extract heightmaps
		if (root.contains("Heightmaps")) {
			const auto& heightmapsTag = root.at("Heightmaps").get<std::shared_ptr<nbt::TagCompound>>();
			extractHeightmaps(*heightmapsTag, chunk);
		}

		// Extract biomes from sections
		extractBiomesFromSections(root, chunk);

		// Extract block entities
		if (root.contains("block_entities")) {
			const auto& blockEntitiesTag = root.at("block_entities").get<std::shared_ptr<nbt::TagList>>();
			extractBlockEntities(*blockEntitiesTag, chunk);
		}

	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Failed to extract chunk data from NBT: " + std::string(e.what()), "World::Query");
		// Leave chunk data empty - it will be treated as empty chunk
	}
}

void World::Query::extractSectionsData(const nbt::TagList& sections, World::ChunkData& chunk) {
	for (size_t i = 0; i < sections.size(); i++) {
		const auto& sectionTag = sections[i].get<std::shared_ptr<nbt::TagCompound>>();

		// Get section Y coordinate
		int8_t sectionY = 0;
		if (sectionTag->contains("Y")) {
			sectionY = sectionTag->at("Y").get<nbt::TagByte>();
		}

		// Extract block states
		if (sectionTag->contains("block_states")) {
			const auto& blockStatesTag = sectionTag->at("block_states").get<std::shared_ptr<nbt::TagCompound>>();
			extractBlockStatesFromSection(*blockStatesTag, chunk, sectionY);
		}

		// Extract light data if present
		if (sectionTag->contains("SkyLight")) {
			const auto& skyLightTag = sectionTag->at("SkyLight").get<nbt::TagByteArray>();
			// Append to chunk.skyLight
			size_t offset = chunk.skyLight.size();
			chunk.skyLight.resize(offset + skyLightTag.size());
			std::memcpy(chunk.skyLight.data() + offset, skyLightTag.data(), skyLightTag.size());
		}

		if (sectionTag->contains("BlockLight")) {
			const auto& blockLightTag = sectionTag->at("BlockLight").get<nbt::TagByteArray>();
			// Append to chunk.blockLight
			size_t offset = chunk.blockLight.size();
			chunk.blockLight.resize(offset + blockLightTag.size());
			std::memcpy(chunk.blockLight.data() + offset, blockLightTag.data(), blockLightTag.size());
		}
	}
}

void World::Query::extractBlockStatesFromSection(const nbt::TagCompound& blockStates, World::ChunkData& chunk, int8_t sectionY) {
	// Block states contain palette and packed data array

	if (blockStates.contains("data")) {
		const auto& dataTag = blockStates.at("data").get<nbt::TagLongArray>();

		// For now, just append raw packed data
		// In a full implementation, you'd unpack the bits based on palette size
		size_t offset = chunk.blockData.size();
		chunk.blockData.resize(offset + dataTag.size() * sizeof(int64_t));
		std::memcpy(chunk.blockData.data() + offset, dataTag.data(), dataTag.size() * sizeof(int64_t));
	}

	// Note: To properly decode blocks, you'd also need to:
	// 1. Read the "palette" array to get block state mappings
	// 2. Calculate bits per entry based on palette size
	// 3. Unpack the long array using the calculated bits per entry
	// 4. Map indices back to actual block states using the palette
}

void World::Query::extractHeightmaps(const nbt::TagCompound& heightmaps, World::ChunkData& chunk) {
	// Modern chunks typically have multiple heightmap types
	if (heightmaps.contains("MOTION_BLOCKING")) {
		const auto& motionBlockingTag = heightmaps.at("MOTION_BLOCKING").get<nbt::TagLongArray>();

		size_t dataSize = motionBlockingTag.size() * sizeof(int64_t);
		chunk.heightmaps.resize(dataSize);
		std::memcpy(chunk.heightmaps.data(), motionBlockingTag.data(), dataSize);
	}
}

void World::Query::extractBiomesFromSections(const nbt::TagCompound& root, World::ChunkData& chunk) {
	// In modern format, biomes are stored in sections
	if (root.contains("sections")) {
		const auto& sectionsTag = root.at("sections").get<std::shared_ptr<nbt::TagList>>();

		for (size_t i = 0; i < sectionsTag->size(); i++) {
			const auto& sectionTag = (*sectionsTag)[i].get<std::shared_ptr<nbt::TagCompound>>();

			if (sectionTag->contains("biomes")) {
				const auto& biomesTag = sectionTag->at("biomes").get<std::shared_ptr<nbt::TagCompound>>();

				if (biomesTag->contains("data")) {
					const auto& biomeDataTag = biomesTag->at("data").get<nbt::TagLongArray>();

					// Append biome data
					size_t offset = chunk.biomeData.size();
					chunk.biomeData.resize(offset + biomeDataTag.size() * sizeof(int64_t));
					std::memcpy(chunk.biomeData.data() + offset, biomeDataTag.data(), biomeDataTag.size() * sizeof(int64_t));
				}
			}
		}
	}
}

void World::Query::extractBlockEntities(const nbt::TagList& blockEntities, World::ChunkData& chunk) {
	// For now, just reserve some space
	// In a full implementation, you'd serialize each block entity properly
	chunk.blockEntities.reserve(blockEntities.size() * 100); // Rough estimate

	// You could iterate through and extract specific block entity data if needed
	// For basic functionality, leaving empty is fine
}

World::ChunkData World::Query::generateEmptyChunk(int chunkX, int chunkZ) {
	World::ChunkData emptyChunk(chunkX, chunkZ);

	// Leave all vectors empty - this represents an empty chunk
	// The isEmpty() function will return true for this chunk

	g_logger->logGameInfo(DEBUG, "Generated empty chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ")", "World::Query");

	return emptyChunk;
}
