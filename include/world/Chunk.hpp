#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "world/LevelTicks.hpp"
#include "world/Light.hpp"
#include "world/blockentity/BlockEntity.hpp"
#include "world/PalettedContainer.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
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

	// Scheduled ticks of the blocks and fluids of this chunk (see Level), saved with it
	ChunkTicks& blockTicks() { return _blockTicks; }
	ChunkTicks& fluidTicks() { return _fluidTicks; }
	const ChunkTicks& blockTicks() const { return _blockTicks; }
	const ChunkTicks& fluidTicks() const { return _fluidTicks; }

	// Set by World: loaded with its neighbors and kept by a player, so its blocks tick (vanilla's block ticking)
	bool isTicking() const { return _ticking.load(std::memory_order_relaxed); }
	void setTicking(bool ticking) { _ticking.store(ticking, std::memory_order_relaxed); }
	// Set by World when it drops the chunk: whoever still holds it must forget it
	bool isUnloaded() const { return _unloaded.load(std::memory_order_relaxed); }
	void setUnloaded() { _unloaded.store(true, std::memory_order_relaxed); }

	// Block entities (comparators, containers, moving pistons...), by index ((y - minY) << 8 | z << 4 | x). Saved with
	// the chunk: the game thread holds mutex() while changing the map, the I/O threads while saving it
	using BlockEntities = std::unordered_map<uint32_t, std::shared_ptr<BlockEntity>>;
	BlockEntities&		 blockEntities() { return _blockEntities; }
	const BlockEntities& blockEntities() const { return _blockEntities; }
	uint32_t			 indexOf(int x, int y, int z) const { return static_cast<uint32_t>(y - _minY) << 8 | (z & 15) << 4 | (x & 15); }

	// The entities saved with the chunk (mobs), encoded by EntityManager::encodeChunk: written by the game thread
	// (Level::saveEntities) and read when the chunk joins the level, under mutex()
	std::vector<uint8_t>&		savedEntities() { return _savedEntities; }
	const std::vector<uint8_t>& savedEntities() const { return _savedEntities; }

	// Game thread only (see Level): per section, how many of its blocks tick randomly
	std::vector<uint16_t>& randomTickingCounts() { return _randomTicking; }

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
	ChunkTicks						 _blockTicks;
	ChunkTicks						 _fluidTicks;
	std::atomic<bool>				 _ticking{false};
	std::atomic<bool>				 _unloaded{false};
	std::vector<uint16_t>			 _randomTicking;
	BlockEntities					 _blockEntities;
	std::vector<uint8_t>			 _savedEntities;

	ChunkSection& sectionAt(int y) { return _sections[(y - _minY) >> 4]; }
	const ChunkSection& sectionAt(int y) const { return _sections[(y - _minY) >> 4]; }
};

#endif
