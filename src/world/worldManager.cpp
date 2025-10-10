#include "lib/nbtParser.hpp"
#include "world/worldManager.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include <zconf.h>
#include <zlib.h>

std::vector<uint8_t> WorldManager::decompressGzip(std::filesystem::path compressedFilePath) {
	// Read file into memory
	std::ifstream file(compressedFilePath, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Could not open file: " + compressedFilePath.string());
	}

	std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(file)),
									std::istreambuf_iterator<char>());
	file.close();

	// Initialize zlib stream
	z_stream stream;
	std::memset(&stream, 0, sizeof(stream));

	// 16 + MAX_WBITS tells zlib to decode gzip format
	if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
		throw std::runtime_error("Failed to initialize gzip decompression");
	}

	// Allocate output buffer (level.dat is usually < 10MB)
	std::vector<uint8_t> decompressed(10 * 1024 * 1024);

	stream.avail_in	 = static_cast<uInt>(compressed.size());
	stream.next_in	 = compressed.data();
	stream.avail_out = static_cast<uInt>(decompressed.size());
	stream.next_out	 = decompressed.data();

	int ret = inflate(&stream, Z_FINISH);

	if (ret != Z_STREAM_END) {
		inflateEnd(&stream);
		throw std::runtime_error("Decompression failed: " + std::to_string(ret));
	}

	// Resize to actual decompressed size
	decompressed.resize(stream.total_out);
	inflateEnd(&stream);

	return decompressed;
}

Data WorldManager::loadLevelDat(std::filesystem::path levelDatPath) {
	std::vector<uint8_t> decompressedData = decompressGzip(levelDatPath);
	if (decompressedData.empty()) {
		throw std::runtime_error("Decompressed level.dat data is empty.");
	}

	// Parse NBT using the dedicated parser
	nbt::Parser parser;
	nbt::NBT	n = parser.parse(decompressedData);

	Data dataStruct;
	dataStruct.nbtData = std::move(n);

	// --- Populate the Data struct for easy access ---
	const auto& dataCompound =
			dataStruct.nbtData.getRoot().at("Data").get<std::shared_ptr<nbt::TagCompound>>();

	// Helper to safely get a value from a compound tag
	auto getTagValue = [&](const std::string& key, auto& member) {
		using MemberType = std::remove_reference_t<decltype(member)>;
		if (dataCompound->contains(key)) {
			try {
				if constexpr (std::is_same_v<MemberType, bool>) {
					// NBT uses TagByte for booleans
					member = (dataCompound->at(key).get<nbt::TagByte>() != 0);
				} else if constexpr (std::is_same_v<MemberType, std::byte>) {
					member = std::byte(dataCompound->at(key).get<nbt::TagByte>());
				} else {
					member = dataCompound->at(key).get<MemberType>();
				}
			} catch (const std::bad_variant_access&) {
				// Type mismatch, leave member as default
			}
		}
	};

	getTagValue("allowCommands", dataStruct.allowCommands);
	getTagValue("BorderCenterX", dataStruct.BorderCenterX);
	getTagValue("BorderCenterY", dataStruct.BorderCenterY);
	getTagValue("BorderDamagePerBlock", dataStruct.BorderDamagePerBlock);
	getTagValue("BorderSafeZone", dataStruct.BorderSafeZone);
	getTagValue("BorderSizeLerpTarget", dataStruct.BorderSizeLerpTarget);
	getTagValue("BorderWarningBlocks", dataStruct.BorderWarningBlocks);
	getTagValue("BorderWarningTime", dataStruct.BorderWarningTime);
	getTagValue("clearWeatherTime", dataStruct.clearWeatherTime);
	getTagValue("DataVersion", dataStruct.DataVersion);
	getTagValue("DayTime", dataStruct.DayTime);
	getTagValue("Difficulty", dataStruct.Difficulty);
	getTagValue("DifficultyLocked", dataStruct.DifficultyLocked);
	getTagValue("GameType", dataStruct.GameType);
	getTagValue("generatorName", dataStruct.generatorName);
	getTagValue("generatorVersion", dataStruct.generatorVersiona);
	getTagValue("hardcore", dataStruct.hardcore);
	getTagValue("initialized", dataStruct.initialized);
	getTagValue("LastPlayed", dataStruct.LastPlayed);
	getTagValue("LevelName", dataStruct.LevelName);
	getTagValue("MapFeatures", dataStruct.MapFeatures);
	getTagValue("raining", dataStruct.raining);
	getTagValue("rainTime", dataStruct.rainTime);
	getTagValue("RandomSeed", dataStruct.RandomSeed);
	getTagValue("SizeOnDisk", dataStruct.SizeOnDisk);
	getTagValue("SpawnX", dataStruct.SpawnX);
	getTagValue("SpawnY", dataStruct.SpawnY);
	getTagValue("SpawnZ", dataStruct.SpawnZ);
	getTagValue("thundering", dataStruct.thundering);
	getTagValue("thunderTime", dataStruct.thunderTime);
	getTagValue("Time", dataStruct.Time);
	getTagValue("version", dataStruct.version);
	getTagValue("WanderingTraderSpawnChance", dataStruct.WanderingTraderSpawnChange);
	getTagValue("WanderingTraderSpawnDelay", dataStruct.WanderingTraderSpawnDelay);
	getTagValue("WasModded", dataStruct.WasModded);

	return dataStruct;
}
