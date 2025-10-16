#ifndef WORLD_MANAGER_HPP
#define WORLD_MANAGER_HPP

#include "heightMap.hpp"
#include "lib/nbt.hpp"
#include "logger.hpp"
#include "palettedContainer.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

// Forward declarations moved to namespace

namespace World {
	// Forward declarations
	class HeightMap;
	class PalettedContainer;

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

	struct ChunkSection {
		uint16_t						   blockCount = 0;
		std::unique_ptr<PalettedContainer> blockStates; // 4096 entries
		std::unique_ptr<PalettedContainer> biomes;		// 64 entries

		std::vector<uint8_t> skyLight;	 // 2048 bytes
		std::vector<uint8_t> blockLight; // 2048 bytes

		bool hasSkyLight   = false;
		bool hasBlockLight = false;
		bool isEmpty	   = true;

		ChunkSection();

		void				 calculateBlockCount(const std::vector<uint32_t>& blockStateIds);
		void				 setBlocks(const std::vector<uint32_t>& blockStateIds);
		void				 setBiomes(const std::vector<uint32_t>& biomeIds);
		void				 initializeLighting(bool withSkyLight = true, bool withBlockLight = false);
		std::vector<uint8_t> serialize() const;
	};

	struct ChunkData {
		int32_t chunkX, chunkZ;

		HeightMap				  heightmaps;
		std::vector<ChunkSection> sections;

		struct BlockEntity {
			uint8_t				 packedXZ; // (x & 15) << 4 | (z & 15)
			int16_t				 y;
			uint32_t			 type;
			std::vector<uint8_t> nbtData;

			BlockEntity(uint8_t x, uint8_t z, int16_t yPos, uint32_t typeId);
		};
		std::vector<BlockEntity> blockEntities;

		bool	isFullyGenerated = false;
		int64_t inhabitedTime	 = 0;
		int64_t lastUpdate		 = 0;

		ChunkData(int32_t x, int32_t z, int worldHeight = 384, int minY = -64);

		ChunkSection* getSectionByY(int worldY, int minY = -64);
		bool		  isEmpty() const;

		static ChunkData generateEmpty(int32_t x, int32_t z, int worldHeight = 384);
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

} // namespace World

#endif
