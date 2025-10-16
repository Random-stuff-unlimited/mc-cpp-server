#include "logger.hpp"
#include "world/palettedContainer.hpp"
#include "world/world.hpp"

namespace World {

	// ChunkSection implementation
	ChunkSection::ChunkSection() {
		blockStates = std::make_unique<PalettedContainer>(4096, true); // 16x16x16 blocks
		biomes		= std::make_unique<PalettedContainer>(64, false);  // 4x4x4 biomes

		// Initialize with air blocks and plains biome
		blockStates->setSingleValue(0); // Air block
		biomes->setSingleValue(1);		// Plains biome

		skyLight.resize(2048, 0xFF);   // Full sky light
		blockLight.resize(2048, 0x00); // No block light

		hasSkyLight	  = true;
		hasBlockLight = false;
		isEmpty		  = true;
		blockCount	  = 0;
	}

	void ChunkSection::calculateBlockCount(const std::vector<uint32_t>& blockStateIds) {
		blockCount = 0;
		for (uint32_t blockId : blockStateIds) {
			if (blockId != 0) { // Count non-air blocks
				blockCount++;
			}
		}
		isEmpty = (blockCount == 0);
	}

	void ChunkSection::setBlocks(const std::vector<uint32_t>& blockStateIds) {
		if (blockStateIds.size() != 4096) {
			throw std::invalid_argument("Block state array must contain exactly 4096 entries");
		}

		blockStates->setFromArray(blockStateIds);
		calculateBlockCount(blockStateIds);
	}

	void ChunkSection::setBiomes(const std::vector<uint32_t>& biomeIds) {
		if (biomeIds.size() != 64) {
			throw std::invalid_argument("Biome array must contain exactly 64 entries");
		}

		biomes->setFromArray(biomeIds);
	}

	void ChunkSection::initializeLighting(bool withSkyLight, bool withBlockLight) {
		if (withSkyLight) {
			skyLight.resize(2048, 0xFF); // Full sky light
			hasSkyLight = true;
		} else {
			skyLight.clear();
			hasSkyLight = false;
		}

		if (withBlockLight) {
			blockLight.resize(2048, 0x00); // No block light initially
			hasBlockLight = true;
		} else {
			blockLight.clear();
			hasBlockLight = false;
		}
	}

	std::vector<uint8_t> ChunkSection::serialize() const {
		std::vector<uint8_t> data;

		// Serialize block count (2 bytes)
		data.push_back(blockCount & 0xFF);
		data.push_back((blockCount >> 8) & 0xFF);

		// Serialize block states
		auto blockData = blockStates->serialize();
		data.insert(data.end(), blockData.begin(), blockData.end());

		// Serialize biomes
		auto biomeData = biomes->serialize();
		data.insert(data.end(), biomeData.begin(), biomeData.end());

		return data;
	}

	// BlockEntity implementation
	ChunkData::BlockEntity::BlockEntity(uint8_t x, uint8_t z, int16_t yPos, uint32_t typeId)
		: packedXZ((x & 15) << 4 | (z & 15)), y(yPos), type(typeId) {
		// Initialize with empty NBT data
		nbtData.clear();
	}

	// ChunkData implementation
	ChunkData::ChunkData(int32_t x, int32_t z, int worldHeight, int minY) : chunkX(x), chunkZ(z), heightmaps(worldHeight, minY) {

		// Calculate number of sections needed
		int sectionsCount = (worldHeight + 15) / 16; // Round up to nearest 16
		sections.resize(sectionsCount);

		// Initialize all sections as empty
		for (auto& section : sections) {
			section = ChunkSection();
		}

		isFullyGenerated = false;
		inhabitedTime	 = 0;
		lastUpdate		 = 0;
	}

	ChunkSection* ChunkData::getSectionByY(int worldY, int minY) {
		int sectionIndex = (worldY - minY) / 16;
		if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size())) {
			return nullptr;
		}
		return &sections[sectionIndex];
	}

	bool ChunkData::isEmpty() const {
		for (const auto& section : sections) {
			if (!section.isEmpty) {
				return false;
			}
		}
		return true;
	}

	ChunkData ChunkData::generateEmpty(int32_t x, int32_t z, int worldHeight) {
		ChunkData chunk(x, z, worldHeight, -64);

		// Initialize heightmaps with minimum values
		chunk.heightmaps.generateEmpty();

		// All sections are already initialized as empty in constructor
		chunk.isFullyGenerated = true; // Empty chunk is considered "generated"

		return chunk;
	}

} // namespace World
