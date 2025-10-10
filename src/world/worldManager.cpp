#include "world/worldManager.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <zconf.h>
#include <zlib.h>

// --- NBT Tag Type IDs ---
const uint8_t TAG_END		 = 0;
const uint8_t TAG_BYTE		 = 1;
const uint8_t TAG_SHORT		 = 2;
const uint8_t TAG_INT		 = 3;
const uint8_t TAG_LONG		 = 4;
const uint8_t TAG_FLOAT		 = 5;
const uint8_t TAG_DOUBLE	 = 6;
const uint8_t TAG_BYTE_ARRAY = 7;
const uint8_t TAG_STRING	 = 8;
const uint8_t TAG_LIST		 = 9;
const uint8_t TAG_COMPOUND	 = 10;
const uint8_t TAG_INT_ARRAY	 = 11;
const uint8_t TAG_LONG_ARRAY = 12;

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

	stream.avail_in	 = compressed.size();
	stream.next_in	 = compressed.data();
	stream.avail_out = decompressed.size();
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

	size_t cursor = 0;

	uint8_t rootType = read<uint8_t>(decompressedData, cursor);
	if (rootType != TAG_COMPOUND) {
		throw std::runtime_error("level.dat must start with a TAG_Compound.");
	}

	std::string		 rootName	  = parseTagName(decompressedData, cursor);
	nbt::TagCompound rootCompound = parseCompound(decompressedData, cursor);

	Data dataStruct;
	dataStruct.nbtData = nbt::NBT(rootName, rootCompound);

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

// --- NBT Parsing Implementation ---

template <typename T> T WorldManager::read(const std::vector<uint8_t>& data, size_t& cursor) {
	if (cursor + sizeof(T) > data.size()) {
		throw std::runtime_error("NBT parsing error: unexpected end of data.");
	}
	T value;
	std::memcpy(&value, &data[cursor], sizeof(T));
	cursor += sizeof(T);

	if constexpr (sizeof(T) > 1) {
		if (std::endian::native == std::endian::little) {
			return std::byteswap(value);
		}
	}
	return value;
}

std::string WorldManager::parseTagName(const std::vector<uint8_t>& data, size_t& cursor) {
	uint16_t length = read<uint16_t>(data, cursor);
	if (length == 0) {
		return "";
	}
	if (cursor + length > data.size()) {
		throw std::runtime_error("NBT parsing error: invalid tag name length.");
	}
	std::string name(data.begin() + cursor, data.begin() + cursor + length);
	cursor += length;
	return name;
}

nbt::TagCompound WorldManager::parseCompound(const std::vector<uint8_t>& data, size_t& cursor) {
	nbt::TagCompound compound;
	while (cursor < data.size()) {
		uint8_t tagType = read<uint8_t>(data, cursor);
		if (tagType == TAG_END) {
			break;
		}

		std::string tagName = parseTagName(data, cursor);
		compound[tagName]	= parseTag(data, cursor, tagType);
	}
	return compound;
}

nbt::TagList WorldManager::parseList(const std::vector<uint8_t>& data, size_t& cursor) {
	nbt::TagList list;
	uint8_t		 listType = read<uint8_t>(data, cursor);
	int32_t		 length	  = read<int32_t>(data, cursor);

	for (int32_t i = 0; i < length; ++i) {
		list.push_back(parseTag(data, cursor, listType));
	}
	return list;
}

nbt::Tag WorldManager::parseTag(const std::vector<uint8_t>& data, size_t& cursor, uint8_t tagType) {
	switch (tagType) {
	case TAG_BYTE:
		return nbt::Tag(read<nbt::TagByte>(data, cursor));
	case TAG_SHORT:
		return nbt::Tag(read<nbt::TagShort>(data, cursor));
	case TAG_INT:
		return nbt::Tag(read<nbt::TagInt>(data, cursor));
	case TAG_LONG:
		return nbt::Tag(read<nbt::TagLong>(data, cursor));
	case TAG_FLOAT:
		return nbt::Tag(std::bit_cast<nbt::TagFloat>(read<uint32_t>(data, cursor)));
	case TAG_DOUBLE:
		return nbt::Tag(std::bit_cast<nbt::TagDouble>(read<uint64_t>(data, cursor)));
	case TAG_STRING: {
		uint16_t length = read<uint16_t>(data, cursor);
		if (cursor + length > data.size()) {
			throw std::runtime_error("NBT parsing error: invalid string length.");
		}
		std::string str(data.begin() + cursor, data.begin() + cursor + length);
		cursor += length;
		return nbt::Tag(str);
	}
	case TAG_BYTE_ARRAY: {
		int32_t			  length = read<int32_t>(data, cursor);
		nbt::TagByteArray array;
		if (length > 0) {
			array.resize(length);
			std::memcpy(array.data(), &data[cursor], length);
			cursor += length;
		}
		return nbt::Tag(array);
	}
	case TAG_INT_ARRAY: {
		int32_t			 length = read<int32_t>(data, cursor);
		nbt::TagIntArray array;
		array.reserve(length);
		for (int32_t i = 0; i < length; ++i) {
			array.push_back(read<nbt::TagInt>(data, cursor));
		}
		return nbt::Tag(array);
	}
	case TAG_LONG_ARRAY: {
		int32_t			  length = read<int32_t>(data, cursor);
		nbt::TagLongArray array;
		array.reserve(length);
		for (int32_t i = 0; i < length; ++i) {
			array.push_back(read<nbt::TagLong>(data, cursor));
		}
		return nbt::Tag(array);
	}
	case TAG_LIST:
		return nbt::Tag(std::make_shared<nbt::TagList>(parseList(data, cursor)));
	case TAG_COMPOUND:
		return nbt::Tag(std::make_shared<nbt::TagCompound>(parseCompound(data, cursor)));
	default:
		throw std::runtime_error("Unsupported NBT tag type: " + std::to_string(tagType));
	}
}
