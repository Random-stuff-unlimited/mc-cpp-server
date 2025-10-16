#include "world/heightMap.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

World::HeightMap::HeightMap(int worldHeight, int minY) : _worldHeight(worldHeight), _minY(minY) {
	if (worldHeight <= 0) {
		throw std::invalid_argument("World height must be positive");
	}
	_bitsPerEntry = calculateBitsPerEntry(worldHeight);
}

uint8_t World::HeightMap::calculateBitsPerEntry(int worldHeight) const {
	// Minecraft uses ceil(log2(world_height + 1)) bits per entry
	// +1 because height can range from 0 to world_height (inclusive)
	return static_cast<uint8_t>(std::ceil(std::log2(worldHeight + 1)));
}

void World::HeightMap::setHeightMapDirect(Type type, const std::vector<uint16_t>& heights) {
	if (heights.size() != HEIGHTMAP_SIZE) {
		throw std::invalid_argument("Height array must contain exactly 256 entries");
	}

	// Validate height values
	for (uint16_t height : heights) {
		if (height > _worldHeight) {
			throw std::invalid_argument("Height value exceeds world height");
		}
	}

	_heightmaps[type] = heights;
}

void World::HeightMap::setHeightMap(Type type, const std::vector<int64_t>& packedData) { unpackHeightMap(type, packedData); }

void World::HeightMap::unpackHeightMap(Type type, const std::vector<int64_t>& packedData) {
	std::vector<uint16_t> heights(HEIGHTMAP_SIZE);
	unpackBits(packedData, heights);
	_heightmaps[type] = std::move(heights);
}

void World::HeightMap::unpackBits(const std::vector<int64_t>& packed, std::vector<uint16_t>& heights) const {
	if (heights.size() != HEIGHTMAP_SIZE) {
		heights.resize(HEIGHTMAP_SIZE);
	}

	size_t	 valuesPerLong = 64 / _bitsPerEntry;
	uint64_t mask		   = (1ULL << _bitsPerEntry) - 1;

	for (size_t i = 0; i < HEIGHTMAP_SIZE; ++i) {
		size_t longIndex = i / valuesPerLong;
		size_t bitOffset = (i % valuesPerLong) * _bitsPerEntry;

		if (longIndex >= packed.size()) {
			heights[i] = 0; // Default to 0 if data is insufficient
			continue;
		}

		uint64_t longValue = static_cast<uint64_t>(packed[longIndex]);
		heights[i]		   = static_cast<uint16_t>((longValue >> bitOffset) & mask);
	}
}

std::vector<int64_t> World::HeightMap::packHeightMap(Type type) const {
	auto it = _heightmaps.find(type);
	if (it == _heightmaps.end()) {
		throw std::runtime_error("Heightmap type not found");
	}

	std::vector<int64_t> packed;
	packBits(it->second, packed);
	return packed;
}

void World::HeightMap::packBits(const std::vector<uint16_t>& heights, std::vector<int64_t>& packed) const {
	if (heights.size() != HEIGHTMAP_SIZE) {
		throw std::invalid_argument("Heights array must be exactly 256 entries");
	}

	size_t valuesPerLong = 64 / _bitsPerEntry;
	size_t longCount	 = (HEIGHTMAP_SIZE + valuesPerLong - 1) / valuesPerLong;

	packed.clear();
	packed.resize(longCount, 0);

	uint64_t mask = (1ULL << _bitsPerEntry) - 1;

	for (size_t i = 0; i < HEIGHTMAP_SIZE; ++i) {
		size_t longIndex = i / valuesPerLong;
		size_t bitOffset = (i % valuesPerLong) * _bitsPerEntry;

		uint64_t value = static_cast<uint64_t>(heights[i] & mask);
		packed[longIndex] |= (value << bitOffset);
	}
}

void World::HeightMap::setHeight(Type type, uint8_t x, uint8_t z, uint16_t height) {
	validateCoordinates(x, z);

	if (height > _worldHeight) {
		throw std::invalid_argument("Height exceeds world height");
	}

	ensureHeightMapExists(type);
	size_t index			 = coordToIndex(x, z);
	_heightmaps[type][index] = height;
}

uint16_t World::HeightMap::getHeight(Type type, uint8_t x, uint8_t z) const {
	validateCoordinates(x, z);

	auto it = _heightmaps.find(type);
	if (it == _heightmaps.end()) {
		return 0; // Default height if heightmap doesn't exist
	}

	size_t index = coordToIndex(x, z);
	return it->second[index];
}

size_t World::HeightMap::coordToIndex(uint8_t x, uint8_t z) const {
	// Minecraft uses x as the fastest-increasing coordinate
	return static_cast<size_t>(z) * CHUNK_SIZE + static_cast<size_t>(x);
}

void World::HeightMap::indexToCoord(size_t index, uint8_t& x, uint8_t& z) const {
	x = static_cast<uint8_t>(index % CHUNK_SIZE);
	z = static_cast<uint8_t>(index / CHUNK_SIZE);
}

void World::HeightMap::validateCoordinates(uint8_t x, uint8_t z) const {
	if (x >= CHUNK_SIZE || z >= CHUNK_SIZE) {
		throw std::out_of_range("Coordinates must be within 0-15 range");
	}
}

void World::HeightMap::ensureHeightMapExists(Type type) {
	if (_heightmaps.find(type) == _heightmaps.end()) {
		_heightmaps[type] = std::vector<uint16_t>(HEIGHTMAP_SIZE, 0);
	}
}

void World::HeightMap::generateEmpty() {
	// Generate common heightmaps with minimum values
	std::vector<Type> commonTypes = {Type::MOTION_BLOCKING, Type::WORLD_SURFACE};

	for (Type type : commonTypes) {
		_heightmaps[type] = std::vector<uint16_t>(HEIGHTMAP_SIZE, static_cast<uint16_t>(_minY));
	}
}

void World::HeightMap::generateFlat(uint16_t height) {
	if (height < _minY || height > _worldHeight) {
		throw std::invalid_argument("Height out of valid range");
	}

	std::vector<Type> commonTypes = {Type::MOTION_BLOCKING, Type::MOTION_BLOCKING_NO_LEAVES, Type::WORLD_SURFACE, Type::OCEAN_FLOOR};

	for (Type type : commonTypes) {
		_heightmaps[type] = std::vector<uint16_t>(HEIGHTMAP_SIZE, height);
	}
}

bool World::HeightMap::hasHeightMap(Type type) const { return _heightmaps.find(type) != _heightmaps.end(); }

std::vector<World::Type> World::HeightMap::getAvailableTypes() const {
	std::vector<Type> types;
	for (const auto& pair : _heightmaps) {
		types.push_back(pair.first);
	}
	return types;
}

std::string World::HeightMap::getTypeName(Type type) {
	switch (type) {
	case Type::MOTION_BLOCKING:
		return "MOTION_BLOCKING";
	case Type::MOTION_BLOCKING_NO_LEAVES:
		return "MOTION_BLOCKING_NO_LEAVES";
	case Type::OCEAN_FLOOR:
		return "OCEAN_FLOOR";
	case Type::WORLD_SURFACE:
		return "WORLD_SURFACE";
	case Type::WORLD_SURFACE_WG:
		return "WORLD_SURFACE_WG";
	default:
		return "UNKNOWN";
	}
}

World::Type World::HeightMap::getTypeFromName(const std::string& name) {
	if (name == "MOTION_BLOCKING") return Type::MOTION_BLOCKING;
	if (name == "MOTION_BLOCKING_NO_LEAVES") return Type::MOTION_BLOCKING_NO_LEAVES;
	if (name == "OCEAN_FLOOR") return Type::OCEAN_FLOOR;
	if (name == "WORLD_SURFACE") return Type::WORLD_SURFACE;
	if (name == "WORLD_SURFACE_WG") return Type::WORLD_SURFACE_WG;
	throw std::invalid_argument("Unknown heightmap type: " + name);
}

std::vector<uint8_t> World::HeightMap::serializeToNBT() const {
	std::vector<uint8_t> buffer;

	// NBT Compound tag for heightmaps
	buffer.push_back(0x0A);	 // TAG_Compound
	writeString(buffer, ""); // Empty name for root compound

	for (const auto& pair : _heightmaps) {
		// TAG_Long_Array for each heightmap
		buffer.push_back(0x0C); // TAG_Long_Array
		writeString(buffer, getTypeName(pair.first));

		std::vector<int64_t> packed = packHeightMap(pair.first);
		writeLongArray(buffer, packed);
	}

	buffer.push_back(0x00); // TAG_End
	return buffer;
}

void World::HeightMap::writeString(std::vector<uint8_t>& buffer, const std::string& str) const {
	uint16_t length = static_cast<uint16_t>(str.length());
	buffer.push_back(static_cast<uint8_t>(length >> 8));
	buffer.push_back(static_cast<uint8_t>(length & 0xFF));
	buffer.insert(buffer.end(), str.begin(), str.end());
}

void World::HeightMap::writeLongArray(std::vector<uint8_t>& buffer, const std::vector<int64_t>& data) const {
	// Write array length
	uint32_t length = static_cast<uint32_t>(data.size());
	buffer.push_back(static_cast<uint8_t>(length >> 24));
	buffer.push_back(static_cast<uint8_t>(length >> 16));
	buffer.push_back(static_cast<uint8_t>(length >> 8));
	buffer.push_back(static_cast<uint8_t>(length & 0xFF));

	// Write longs in big-endian format
	for (int64_t value : data) {
		for (int i = 7; i >= 0; --i) {
			buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
		}
	}
}

void World::HeightMap::copyFrom(const World::HeightMap& other) {
	if (_worldHeight != other._worldHeight || _minY != other._minY) {
		throw std::invalid_argument("Cannot copy from heightmap with different world dimensions");
	}

	_heightmaps = other._heightmaps;
}
