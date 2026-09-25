#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "world/Light.hpp"
#include "world/PalettedContainer.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

struct ChunkSection {
	PalettedContainer blocks; // 16x16x16 block states, index (y << 8) | (z << 4) | x
	PalettedContainer biomes; // 4x4x4 biomes, index (y << 4) | (z << 2) | x
};

// A 16-block-wide column of sections. Methods don't lock: once a chunk is shared (after loading),
// hold mutex() to read or modify it.
class Chunk {
  public:
	Chunk(int x, int z, int minY, int sectionCount, const PalettedContainer::Config& blockConfig, const PalettedContainer::Config& biomeConfig,
		  uint32_t fillBlock, uint32_t fillBiome);

	int x() const { return _x; }
	int z() const { return _z; }
	int minY() const { return _minY; }
	int maxY() const { return _minY + static_cast<int>(_sections.size()) * 16; } // Exclusive

	// x and z are local (0-15), y is the world height
	uint32_t getBlock(int x, int y, int z) const;
	void	 setBlock(int x, int y, int z, uint32_t state);
	uint32_t getBiome(int x, int y, int z) const;
	void	 setBiome(int x, int y, int z, uint32_t biome);

	std::vector<ChunkSection>&		 sections() { return _sections; }
	const std::vector<ChunkSection>& sections() const { return _sections; }

	std::mutex& mutex() const { return _mutex; }

	// Computed when the chunk is lit (see World), then kept up to date as blocks change
	const ChunkLight& light() const { return _light; }
	ChunkLight&		  light() { return _light; }
	void			  setLight(ChunkLight light) { _light = std::move(light); }

	// Incremented on every block change: lets a long computation notice the blocks changed meanwhile
	uint64_t version() const { return _version.load(); }

	// Modified since the last save
	bool isDirty() const { return _dirty.load(); }
	void setDirty(bool dirty) { _dirty.store(dirty); }

	// Encoded Chunk Data packet, shared by every player that receives this chunk. Reset on modification
	std::shared_ptr<const std::vector<uint8_t>> cachedPacket() const { return _cachedPacket; }
	void setCachedPacket(std::shared_ptr<const std::vector<uint8_t>> packet) { _cachedPacket = std::move(packet); }
	// Drops the cached packet after a change (blocks or light)
	void invalidatePacket() {
		_cachedPacket.reset();
		_packetGeneration++;
	}
	// Incremented by invalidatePacket(): a packet encoded meanwhile is outdated
	uint64_t packetGeneration() const { return _packetGeneration; }
	// Called after modifying sections() directly
	void markModified();

	size_t memoryUsage() const;

	static int64_t key(int x, int z) { return (static_cast<int64_t>(z) << 32) | static_cast<uint32_t>(x); }

  private:
	int								 _x;
	int								 _z;
	int								 _minY;
	std::vector<ChunkSection>		 _sections;
	ChunkLight						 _light;
	mutable std::mutex				 _mutex;
	std::atomic<bool>				 _dirty;
	std::atomic<uint64_t>			 _version{0};
	std::shared_ptr<const std::vector<uint8_t>> _cachedPacket;
	uint64_t						 _packetGeneration = 0;

	ChunkSection& sectionAt(int y) { return _sections[(y - _minY) >> 4]; }
	const ChunkSection& sectionAt(int y) const { return _sections[(y - _minY) >> 4]; }
};

#endif
