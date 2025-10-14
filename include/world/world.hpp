#ifndef WORLD_MANAGER_HPP
#define WORLD_MANAGER_HPP

#include "lib/nbt.hpp"
#include "logger.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace World {

	struct LevelDat { // see https://minecraft.wiki/w/Java_Edition_level_format#level.dat_format
		// Existing fields
		bool		allowCommands;
		double		BorderCenterX;
		double		BorderCenterY;
		double		BorderDamagePerBlock;
		double		BorderSafeZone;
		long		BorderSizeLerpTarget;
		double		BorderWarningBlocks;
		double		BorderWarningTime;
		int			clearWeatherTime;
		int			DataVersion;
		long		DayTime;
		std::byte	Difficulty;
		bool		DifficultyLocked;
		int			GameType;
		std::string generatorName;
		int			generatorVersiona;
		bool		hardcore;
		bool		initialized;
		long		LastPlayed;
		std::string LevelName;
		bool		MapFeatures;
		bool		raining;
		int			rainTime;
		long		RandomSeed;
		long		SizeOnDisk;
		int			SpawnX;
		int			SpawnY;
		int			SpawnZ;
		bool		thundering;
		int			thunderTime;
		long		Time;
		int			version;
		int			WanderingTraderSpawnChange;
		int			WanderingTraderSpawnDelay;
		bool		WasModded;

		// The full NBT data, for accessing less common or custom tags
		nbt::NBT nbtData;
	};

	struct ChunkData {
		int chunkX;
		int chunkZ;

		std::vector<uint8_t> blockData;
		std::vector<uint8_t> biomeData;
		std::vector<uint8_t> heightmaps;
		std::vector<uint8_t> blockEntities;

		std::vector<uint8_t> skyLight;
		std::vector<uint8_t> blockLight;

		ChunkData(int x, int z) : chunkX(x), chunkZ(z) {}

		bool isEmpty() const { return blockData.empty(); }
	};

	class Manager {
	  public:
		static std::vector<uint8_t> decompressGzip(std::filesystem::path compressedFilePath);
		static std::vector<uint8_t> decompressGzip(const std::vector<uint8_t>& compressedData);
		static std::vector<uint8_t> decompressZlib(const std::vector<uint8_t>& compressedData);

		std::filesystem::path locateRegionFileByChunkCoord(int ChunkX, int ChunkZ);
		LevelDat			  loadLevelDat(std::filesystem::path levelDatPath);

		const LevelDat& getLevelDat() const { return _LevelDat; }

		void						 setWorldPath(const std::filesystem::path& worldPath) { _worldPath = worldPath; }
		const std::filesystem::path& getWorldPath() const { return _worldPath; }

	  private:
		LevelDat			  _LevelDat;
		std::filesystem::path _worldPath;
	};

	class Query {
	  public:
		explicit Query(Manager& manager) : _worldManager(manager) {}
		ChunkData fetchChunk(int chunkX, int chunkZ) {
			ChunkData chunk(chunkX, chunkZ);

			try {
				auto regionPath = _worldManager.locateRegionFileByChunkCoord(chunkX, chunkZ);
				chunk			= loadChunkFromRegion(regionPath, chunkX, chunkZ);
			} catch (const std::exception& e) {
				g_logger->logGameInfo(DEBUG,
									  "Chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) +
											  ") not found, sending empty chunk: " + e.what(),
									  "World::Query::fetchChunk");
				chunk = generateEmptyChunk(chunkX, chunkZ);
			}

			return chunk;
		}
		const LevelDat& getWorldData() const { return _worldManager.getLevelDat(); }

	  private:
		Manager& _worldManager;

	  private:
		void	  extractChunkDataFromNBT(const nbt::NBT& chunkNBT, ChunkData& chunk);
		void	  extractSectionsData(const nbt::TagList& sections, ChunkData& chunk);
		void	  extractBlockStatesFromSection(const nbt::TagCompound& blockStates, ChunkData& chunk, int8_t sectionY);
		void	  extractHeightmaps(const nbt::TagCompound& heightmaps, ChunkData& chunk);
		void	  extractBlockEntities(const nbt::TagList& blockEntities, ChunkData& chunk);
		void	  extractBiomesFromSections(const nbt::TagCompound& root, ChunkData& chunk);
		ChunkData generateEmptyChunk(int chunkX, int chunkZ);
		ChunkData loadChunkFromRegion(const std::filesystem::path& regionPath, int chunkX, int chunkZ);
	};

} // namespace World

#endif
