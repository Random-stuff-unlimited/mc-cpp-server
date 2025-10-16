#include "world/palettedContainer.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

World::PalettedContainer::PalettedContainer(size_t size, bool isBlockContainer)
	: _containerType(World::PalettedType::SINGLE_VALUED), _bitsPerEntry(0), _size(size), _isBlockContainer(isBlockContainer) {
	if (size == 0) {
		throw std::invalid_argument("Container size must be greater than 0");
	}
}

void World::PalettedContainer::setSingleValue(uint32_t value) {
	_containerType = World::PalettedType::SINGLE_VALUED;
	_bitsPerEntry  = 0;
	_palette.clear();
	_palette.push_back(value);
	_data.clear();
}

void World::PalettedContainer::setFromArray(const std::vector<uint32_t>& values) {
	if (values.size() != _size) {
		throw std::invalid_argument("Array size must match container size");
	}

	optimizeEncoding(values);
}

void World::PalettedContainer::optimizeEncoding(const std::vector<uint32_t>& values) {
	// Count unique values
	std::unordered_map<uint32_t, uint32_t> uniqueValues;
	for (uint32_t value : values) {
		uniqueValues[value]++;
	}

	// Single value optimization
	if (uniqueValues.size() == 1) {
		setSingleValue(values[0]);
		return;
	}

	// Determine encoding strategy
	size_t uniqueCount = uniqueValues.size();

	if (shouldUseDirect(uniqueCount)) {
		_containerType = World::PalettedType::DIRECT;
		_bitsPerEntry  = _isBlockContainer ? MAX_BITS_BLOCK : MAX_BITS_BIOME;
		_palette.clear();
	} else {
		_containerType = World::PalettedType::INDIRECT;
		_bitsPerEntry  = calculateOptimalBits(uniqueCount);

		// Build palette
		_palette.clear();
		_palette.reserve(uniqueCount);
		for (const auto& pair : uniqueValues) {
			_palette.push_back(pair.first);
		}
	}

	packData(values);
}

bool World::PalettedContainer::shouldUseDirect(size_t uniqueValues) const {
	if (_isBlockContainer) {
		// For blocks: use direct if palette would be too large
		return uniqueValues > (1u << 8); // More than 256 unique values
	} else {
		// For biomes: use direct if palette would use more than 3 bits
		return uniqueValues > (1u << 3); // More than 8 unique values
	}
}

uint8_t World::PalettedContainer::calculateOptimalBits(size_t uniqueValues) const {
	if (uniqueValues <= 1) return 0;

	uint8_t minBits = _isBlockContainer ? MIN_BITS_BLOCK : MIN_BITS_BIOME;
	uint8_t maxBits = _isBlockContainer ? MAX_BITS_BLOCK : MAX_BITS_BIOME;

	uint8_t neededBits = static_cast<uint8_t>(std::ceil(std::log2(uniqueValues)));
	return std::max(minBits, std::min(maxBits, neededBits));
}

void World::PalettedContainer::packData(const std::vector<uint32_t>& values) {
	if (_bitsPerEntry == 0) return;

	size_t valuesPerLong = 64 / _bitsPerEntry;
	size_t longCount	 = (_size + valuesPerLong - 1) / valuesPerLong;

	_data.clear();
	_data.resize(longCount, 0);

	for (size_t i = 0; i < _size; ++i) {
		uint32_t value = values[i];

		// Convert to palette index if using indirect encoding
		if (_containerType == World::PalettedType::INDIRECT) {
			value = findInPalette(value);
		}

		setBits(i, value);
	}
}

void World::PalettedContainer::setBits(size_t index, uint32_t value) {
	if (_bitsPerEntry == 0) return;

	size_t valuesPerLong = 64 / _bitsPerEntry;
	size_t longIndex	 = index / valuesPerLong;
	size_t bitOffset	 = (index % valuesPerLong) * _bitsPerEntry;

	uint64_t mask = (1ULL << _bitsPerEntry) - 1;
	_data[longIndex] &= ~(mask << bitOffset);
	_data[longIndex] |= (static_cast<uint64_t>(value & mask) << bitOffset);
}

uint32_t World::PalettedContainer::getBits(size_t index) const {
	if (_bitsPerEntry == 0) return 0;

	size_t valuesPerLong = 64 / _bitsPerEntry;
	size_t longIndex	 = index / valuesPerLong;
	size_t bitOffset	 = (index % valuesPerLong) * _bitsPerEntry;

	uint64_t mask = (1ULL << _bitsPerEntry) - 1;
	return static_cast<uint32_t>((_data[longIndex] >> bitOffset) & mask);
}

uint32_t World::PalettedContainer::getValue(size_t index) const {
	if (index >= _size) {
		throw std::out_of_range("Index out of bounds");
	}

	switch (_containerType) {
	case World::PalettedType::SINGLE_VALUED:
		return _palette[0];

	case World::PalettedType::INDIRECT: {
		uint32_t paletteIndex = getBits(index);
		if (paletteIndex >= _palette.size()) {
			throw std::runtime_error("Invalid palette index");
		}
		return _palette[paletteIndex];
	}

	case World::PalettedType::DIRECT:
		return getBits(index);

	default:
		throw std::runtime_error("Invalid container type");
	}
}

uint32_t World::PalettedContainer::findInPalette(uint32_t value) const {
	for (size_t i = 0; i < _palette.size(); ++i) {
		if (_palette[i] == value) {
			return static_cast<uint32_t>(i);
		}
	}
	throw std::runtime_error("Value not found in palette");
}

std::vector<uint8_t> World::PalettedContainer::serialize() const {
	std::vector<uint8_t> buffer;

	// Write bits per entry
	buffer.push_back(_bitsPerEntry);

	// Write palette (for indirect encoding)
	if (_containerType == World::PalettedType::INDIRECT) {
		writeVarInt(buffer, static_cast<int32_t>(_palette.size()));
		for (uint32_t value : _palette) {
			writeVarInt(buffer, static_cast<int32_t>(value));
		}
	}

	// Write data array length
	writeVarInt(buffer, static_cast<int32_t>(_data.size()));

	// Write data array
	for (uint64_t value : _data) {
		writeVarLong(buffer, static_cast<int64_t>(value));
	}

	return buffer;
}

void World::PalettedContainer::writeVarInt(std::vector<uint8_t>& buffer, int32_t value) const {
	uint32_t uvalue = static_cast<uint32_t>(value);
	while (uvalue >= 0x80) {
		buffer.push_back(static_cast<uint8_t>((uvalue & 0x7F) | 0x80));
		uvalue >>= 7;
	}
	buffer.push_back(static_cast<uint8_t>(uvalue & 0x7F));
}

void World::PalettedContainer::writeVarLong(std::vector<uint8_t>& buffer, int64_t value) const {
	for (int i = 0; i < 8; ++i) {
		buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
	}
}
