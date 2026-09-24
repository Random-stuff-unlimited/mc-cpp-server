#include "world/PalettedContainer.hpp"

#include "network/buffer.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace {
	inline uint32_t valuesPerLong(uint8_t bits) { return 64 / bits; }

	inline uint64_t readPacked(const std::vector<uint64_t>& data, uint8_t bits, uint32_t index) {
		uint32_t perLong = valuesPerLong(bits);
		uint32_t shift	 = (index % perLong) * bits;
		return (data[index / perLong] >> shift) & ((1ULL << bits) - 1);
	}

	inline void writePacked(std::vector<uint64_t>& data, uint8_t bits, uint32_t index, uint64_t value) {
		uint32_t perLong = valuesPerLong(bits);
		uint32_t shift	 = (index % perLong) * bits;
		uint64_t mask	 = ((1ULL << bits) - 1) << shift;
		uint64_t& slot	 = data[index / perLong];
		slot			 = (slot & ~mask) | (value << shift);
	}
} // namespace

PalettedContainer::PalettedContainer(const Config& config, uint32_t value) : _config(&config), _bits(0), _single(value) {}

uint8_t PalettedContainer::bitsFor(uint32_t count) {
	uint8_t bits = 0;
	while ((1u << bits) < count) bits++;
	return bits;
}

std::vector<uint64_t> PalettedContainer::pack(const std::vector<uint32_t>& indices, uint8_t bits) {
	uint32_t			  perLong = valuesPerLong(bits);
	std::vector<uint64_t> data((indices.size() + perLong - 1) / perLong, 0);
	for (uint32_t i = 0; i < indices.size(); i++) {
		writePacked(data, bits, i, indices[i]);
	}
	return data;
}

std::vector<uint32_t> PalettedContainer::unpack(const std::vector<uint64_t>& data, uint8_t bits, uint32_t entries) {
	uint32_t perLong = valuesPerLong(bits);
	if (data.size() < (entries + perLong - 1) / perLong) throw std::runtime_error("Paletted data too short");
	std::vector<uint32_t> out(entries);
	for (uint32_t i = 0; i < entries; i++) {
		out[i] = static_cast<uint32_t>(readPacked(data, bits, i));
	}
	return out;
}

uint8_t PalettedContainer::indirectBitsFor(size_t paletteSize) const {
	return std::max(_config->minIndirectBits, bitsFor(static_cast<uint32_t>(paletteSize)));
}

PalettedContainer PalettedContainer::fromPalette(const Config& config, const std::vector<uint32_t>& palette, uint8_t bits,
												 const std::vector<uint64_t>& data) {
	if (palette.empty()) throw std::runtime_error("Empty palette");
	PalettedContainer container(config, palette[0]);
	if (palette.size() == 1) return container;

	std::vector<uint32_t> values = unpack(data, bits, config.entries);
	for (uint32_t& value : values) {
		if (value >= palette.size()) throw std::runtime_error("Palette index out of range");
		value = palette[value];
	}
	container.rebuild(values);
	return container;
}

// Picks the smallest representation the client accepts for these values
void PalettedContainer::rebuild(const std::vector<uint32_t>& values) {
	std::vector<uint32_t>				   palette;
	std::unordered_map<uint32_t, uint32_t> indexOf;
	std::vector<uint32_t>				   indices(values.size());
	for (size_t i = 0; i < values.size(); i++) {
		auto [it, inserted] = indexOf.try_emplace(values[i], static_cast<uint32_t>(palette.size()));
		if (inserted) palette.push_back(values[i]);
		indices[i] = it->second;
	}

	_data.clear();
	_palette.clear();
	if (palette.size() == 1) {
		_bits	= 0;
		_single = palette[0];
	} else if (bitsFor(static_cast<uint32_t>(palette.size())) <= _config->maxIndirectBits) {
		_bits	 = indirectBitsFor(palette.size());
		_palette = std::move(palette);
		_data	 = pack(indices, _bits);
	} else {
		_bits = _config->directBits;
		_data = pack(values, _bits);
	}
}

uint32_t PalettedContainer::get(uint32_t index) const {
	if (_bits == 0) return _single;
	uint32_t raw = static_cast<uint32_t>(readPacked(_data, _bits, index));
	return isDirect() ? raw : _palette[raw];
}

void PalettedContainer::set(uint32_t index, uint32_t value) {
	if (_bits == 0) {
		if (value == _single) return;
		// Switch to a 2-entry palette: every index points to the old value (0)
		_palette = {_single, value};
		_bits	 = indirectBitsFor(2);
		_data.assign((_config->entries + valuesPerLong(_bits) - 1) / valuesPerLong(_bits), 0);
		writePacked(_data, _bits, index, 1);
		return;
	}
	if (isDirect()) {
		writePacked(_data, _bits, index, value);
		return;
	}

	auto	 it			  = std::find(_palette.begin(), _palette.end(), value);
	uint32_t paletteIndex = static_cast<uint32_t>(it - _palette.begin());
	if (it == _palette.end()) {
		if (_palette.size() == (1u << _bits)) {
			// Palette full: grow the indices (or go direct), then set
			std::vector<uint32_t> all = values();
			all[index]				  = value;
			rebuild(all);
			return;
		}
		_palette.push_back(value);
	}
	writePacked(_data, _bits, index, paletteIndex);
}

void PalettedContainer::fill(uint32_t value) {
	_bits	= 0;
	_single = value;
	_palette.clear();
	_palette.shrink_to_fit();
	_data.clear();
	_data.shrink_to_fit();
}

std::vector<uint32_t> PalettedContainer::values() const {
	if (_bits == 0) return std::vector<uint32_t>(_config->entries, _single);
	std::vector<uint32_t> out = unpack(_data, _bits, _config->entries);
	if (!isDirect()) {
		for (uint32_t& v : out) v = _palette[v];
	}
	return out;
}

void PalettedContainer::compactPalette(std::vector<uint32_t>& palette, uint8_t& bits, std::vector<uint64_t>& data) const {
	palette.clear();
	data.clear();
	if (_bits == 0) {
		palette.push_back(_single);
		bits = 0;
		return;
	}
	std::vector<uint32_t>				   all = values();
	std::unordered_map<uint32_t, uint32_t> indexOf;
	std::vector<uint32_t>				   indices(all.size());
	for (size_t i = 0; i < all.size(); i++) {
		auto [it, inserted] = indexOf.try_emplace(all[i], static_cast<uint32_t>(palette.size()));
		if (inserted) palette.push_back(all[i]);
		indices[i] = it->second;
	}
	if (palette.size() == 1) {
		bits = 0;
		return;
	}
	bits = bitsFor(static_cast<uint32_t>(palette.size()));
	data = pack(indices, bits);
}

void PalettedContainer::writeNetwork(Buffer& buf) const {
	buf.writeUByte(_bits);
	if (_bits == 0) {
		buf.writeVarInt(static_cast<int32_t>(_single));
		return; // No data array
	}
	if (!isDirect()) {
		buf.writeVarInt(static_cast<int32_t>(_palette.size()));
		for (uint32_t value : _palette) buf.writeVarInt(static_cast<int32_t>(value));
	}
	// Fixed-size long array: the client derives the length from the bit count
	for (uint64_t value : _data) buf.writeLong(static_cast<int64_t>(value));
}

size_t PalettedContainer::memoryUsage() const {
	return sizeof(*this) + _palette.capacity() * sizeof(uint32_t) + _data.capacity() * sizeof(uint64_t);
}
