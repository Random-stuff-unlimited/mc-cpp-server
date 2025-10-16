
#ifndef WORLD_CHUNKPARSER_HPP
#define WORLD_CHUNKPARSER_HPP

#include "fstream"
#include "nbt.hpp"
#include "world.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace World {

	class DirectChunkParser {
	  public:
		struct ParseConfig {
			int	 worldHeight	   = 384;
			int	 minY			   = -64;
			bool loadLighting	   = true;
			bool optimizePalettes  = true;
			bool validateData	   = true;
			bool loadBlockEntities = true;

			// Performance options
			bool   useMemoryMapping = true;
			size_t cacheSize		= 64; // Number of chunks to cache
		};

	  private:
		ParseConfig _config;

		// Region file handling
		struct RegionFile {
			std::filesystem::path		   path;
			std::unique_ptr<std::ifstream> stream;
			std::vector<uint32_t>		   chunkOffsets;
			std::vector<uint32_t>		   chunkTimestamps;
			bool						   isLoaded = false;
		};

		mutable std::map<std::pair<int32_t, int32_t>, std::unique_ptr<RegionFile>> _regionCache;

	  public:
		explicit DirectChunkParser(const ParseConfig& config);
		~DirectChunkParser();

		// Main parsing methods
		World::ChunkData parseChunkFromRegion(const std::filesystem::path& regionPath, int32_t chunkX, int32_t chunkZ);

		World::ChunkData parseChunkFromNBT(const nbt::NBT& chunkNBT, int32_t chunkX, int32_t chunkZ);

		// Batch operations
		std::vector<World::ChunkData> parseChunkBatch(const std::filesystem::path&					  regionPath,
													  const std::vector<std::pair<int32_t, int32_t>>& coordinates);

		// Utility methods
		bool									 chunkExistsInRegion(const std::filesystem::path& regionPath, int32_t chunkX, int32_t chunkZ);
		std::vector<std::pair<int32_t, int32_t>> getAvailableChunks(const std::filesystem::path& regionPath);

		// Configuration
		void			   setConfig(const ParseConfig& config) { _config = config; }
		const ParseConfig& getConfig() const { return _config; }

	  private:
		// Region file operations
		RegionFile*			 loadRegionFile(const std::filesystem::path& regionPath) const;
		std::vector<uint8_t> readChunkData(RegionFile* region, int32_t chunkX, int32_t chunkZ) const;
		void				 parseRegionHeader(RegionFile* region) const;
		uint32_t			 getBlockIdFromName(const std::string& blockName);

		// NBT parsing methods
		void parseHeightMaps(const nbt::TagCompound& heightmaps, World::ChunkData& chunk);
		void parseSections(const nbt::TagList& sections, World::ChunkData& chunk);
		void parseBlockEntities(const nbt::TagList& blockEntities, World::ChunkData& chunk);
		void parseChunkMetadata(const nbt::TagCompound& level, World::ChunkData& chunk);

		// Section parsing
		void parseBlockStates(const nbt::TagCompound& blockStates, ChunkSection& section);
		void parseBiomes(const nbt::TagCompound& biomes, ChunkSection& section);
		void parseLighting(const nbt::TagCompound& section, ChunkSection& chunkSection);

		// Paletted data unpacking
		std::vector<uint32_t>
		unpackPalettedData(const nbt::TagLongArray& data, const nbt::TagList* palette, size_t expectedSize, uint32_t defaultValue = 0);

		std::vector<uint32_t> unpackDirectPalettedData(const nbt::TagLongArray& data, uint8_t bitsPerEntry, size_t expectedSize);

		// Heightmap processing
		std::vector<uint16_t> unpackHeightmapData(const nbt::TagLongArray& data, int worldHeight);

		// Validation methods
		void validateChunkData(const World::ChunkData& chunk) const;
		void validateSection(const ChunkSection& section, int sectionY) const;

		// Utility methods
		std::pair<int32_t, int32_t> getRegionCoords(int32_t chunkX, int32_t chunkZ) const;
		size_t						getChunkIndex(int32_t chunkX, int32_t chunkZ) const;
		uint32_t					readBigEndianInt(const uint8_t* data) const;

		// NBT helper methods
		template <typename T> T getTagValue(const nbt::TagCompound& compound, const std::string& key, T defaultValue = T{}) const;

		bool hasTag(const nbt::TagCompound& compound, const std::string& key) const;
	};

}; // namespace World

#endif // WORLD_CHUNKPARSER_HPP
