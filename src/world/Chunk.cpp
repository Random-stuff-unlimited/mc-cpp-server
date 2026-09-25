#include "world/Chunk.hpp"

namespace {
	inline uint32_t blockIndex(int x, int y, int z) { return ((y & 15) << 8) | (z << 4) | x; }
	inline uint32_t biomeIndex(int x, int y, int z) { return (((y & 15) >> 2) << 4) | ((z >> 2) << 2) | (x >> 2); }
} // namespace

Chunk::Chunk(int x, int z, int minY, int sectionCount, const PalettedContainer::Config& blockConfig, const PalettedContainer::Config& biomeConfig,
			 uint32_t fillBlock, uint32_t fillBiome)
	: _x(x), _z(z), _minY(minY), _dirty(false) {
	_sections.reserve(sectionCount);
	for (int i = 0; i < sectionCount; i++) {
		_sections.push_back({PalettedContainer(blockConfig, fillBlock), PalettedContainer(biomeConfig, fillBiome)});
	}
}

uint32_t Chunk::getBlock(int x, int y, int z) const { return sectionAt(y).blocks.get(blockIndex(x, y, z)); }

void Chunk::setBlock(int x, int y, int z, uint32_t state) {
	sectionAt(y).blocks.set(blockIndex(x, y, z), state);
	markModified();
}

uint32_t Chunk::getBiome(int x, int y, int z) const { return sectionAt(y).biomes.get(biomeIndex(x, y, z)); }

void Chunk::setBiome(int x, int y, int z, uint32_t biome) {
	sectionAt(y).biomes.set(biomeIndex(x, y, z), biome);
	markModified();
}

void Chunk::markModified() {
	_dirty.store(true);
	_version.fetch_add(1);
	invalidatePacket();
}

size_t Chunk::memoryUsage() const {
	size_t total = sizeof(*this) + _sections.capacity() * sizeof(ChunkSection);
	for (const ChunkSection& section : _sections) {
		total += section.blocks.memoryUsage() + section.biomes.memoryUsage() - 2 * sizeof(PalettedContainer);
	}
	if (_cachedPacket) total += _cachedPacket->capacity();
	total += _light.memoryUsage() - sizeof(ChunkLight);
	return total;
}
