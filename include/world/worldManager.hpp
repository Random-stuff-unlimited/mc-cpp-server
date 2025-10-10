#ifndef WORLD_MANAGER_HPP
#define WORLD_MANAGER_HPP

#include "lib/nbt.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct Data { // see https://minecraft.wiki/w/Java_Edition_level_format#level.dat_format
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

class WorldManager {
  public:
	// Decompresses a Gzip file into a byte vector
	std::vector<uint8_t> decompressGzip(std::filesystem::path compressedFilePath);

	// Loads and parses the level.dat file
	Data loadLevelDat(std::filesystem::path levelDatPath);

  private:
	// NBT parsing helper functions
	nbt::Tag		 parseTag(const std::vector<uint8_t>& data, size_t& cursor, uint8_t type);
	std::string		 parseTagName(const std::vector<uint8_t>& data, size_t& cursor);
	nbt::TagCompound parseCompound(const std::vector<uint8_t>& data, size_t& cursor);
	nbt::TagList	 parseList(const std::vector<uint8_t>& data, size_t& cursor);

	template <typename T> T read(const std::vector<uint8_t>& data, size_t& cursor);
};

#endif
