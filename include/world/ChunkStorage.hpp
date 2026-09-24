#ifndef CHUNK_STORAGE_HPP
#define CHUNK_STORAGE_HPP

// On-disk world format
//
//   <world>/palettes/blocks.txt   one block state per line ("minecraft:oak_log[axis=y]"), line number = disk id
//   <world>/palettes/biomes.txt   same for biomes
//   <world>/regions/r.<x>.<z>.ccr 32x32 chunks per file
//
// Chunks store disk ids, never the game's numeric ids: those change between Minecraft versions, the names don't.
// Palette files only grow; mapping them to the current version's ids is done once when the world loads.
//
// Region file: a header ("MCCR", version, then 1024 entries {u32 first sector, u32 byte size}) followed by
// 512-byte sectors (small, so nearly empty chunks don't waste space). A chunk is always written to free sectors
// before its header entry is updated, so a crash while saving leaves the previous version readable.
// Chunk blob: u8 compression (1 = zlib), u32 uncompressed size, compressed payload.
// Payload: u8 format version, u8 section count, then per section the block and biome containers:
//   u8 bits (0 = single value), VarInt palette size, VarInt disk ids, then the packed longs (none if bits == 0).
// All integers are little-endian.

#include "world/Chunk.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Maps runtime ids to the ids used on disk, through their names
class DiskPalette {
  public:
	using NameOf = std::function<std::string(uint32_t)>;
	using IdOf	 = std::function<int(const std::string&)>;

	// Unknown names on load (removed in this version) map to fallback
	DiskPalette(std::filesystem::path file, size_t runtimeCount, NameOf nameOf, IdOf idOf, uint32_t fallback);
	~DiskPalette();

	uint32_t toDisk(uint32_t runtimeId);
	uint32_t toRuntime(uint32_t diskId) const;
	size_t	 unknownCount() const { return _unknown; }

  private:
	std::filesystem::path _file;
	NameOf				  _nameOf;
	uint32_t			  _fallback;
	std::vector<int32_t>  _runtimeToDisk; // -1 = not in the file yet
	std::vector<uint32_t> _diskToRuntime;
	size_t				  _unknown = 0;
	FILE*				  _append  = nullptr;
	mutable std::mutex	  _mutex;
};

class RegionFile {
  public:
	explicit RegionFile(const std::filesystem::path& path);
	~RegionFile();

	// index = (z & 31) * 32 + (x & 31)
	bool read(int index, std::vector<uint8_t>& blob);
	void write(int index, const std::vector<uint8_t>& blob);
	void sync();

  private:
	static constexpr uint32_t SECTOR_SIZE	 = 512;
	static constexpr uint32_t ENTRIES_OFFSET = 16;
	static constexpr uint32_t HEADER_SECTORS = (ENTRIES_OFFSET + 1024 * 8 + SECTOR_SIZE - 1) / SECTOR_SIZE;

	int				  _fd;
	std::mutex		  _mutex;
	uint32_t		  _offsets[1024];
	uint32_t		  _sizes[1024];
	std::vector<bool> _usedSectors;

	void	 markSectors(uint32_t first, uint32_t count, bool used);
	uint32_t allocate(uint32_t count);
};

class ChunkStorage {
  public:
	struct Layout {
		int								minY;
		int								sectionCount;
		const PalettedContainer::Config* blockConfig;
		const PalettedContainer::Config* biomeConfig;
		uint32_t						air;
		uint32_t						defaultBiome;
	};

	ChunkStorage(const std::filesystem::path& worldDirectory, const Layout& layout, DiskPalette& blocks, DiskPalette& biomes);

	// nullptr if the chunk was never saved. Throws on corrupted data
	std::unique_ptr<Chunk> load(int x, int z);

	// Two steps so the chunk is only locked while encoding: encode() under the chunk's mutex, then write()
	std::vector<uint8_t> encode(const Chunk& chunk);
	void				 write(int x, int z, const std::vector<uint8_t>& payload);

	void flush();

  private:
	static constexpr size_t MAX_OPEN_REGIONS = 256;

	std::filesystem::path _regionDirectory;
	Layout				  _layout;
	DiskPalette&		  _blocks;
	DiskPalette&		  _biomes;

	std::mutex												  _regionsMutex;
	std::unordered_map<int64_t, std::shared_ptr<RegionFile>> _regions;
	std::unordered_map<int64_t, uint64_t>					  _regionLastUse;
	uint64_t												  _useCounter = 0;

	std::shared_ptr<RegionFile> region(int chunkX, int chunkZ, bool create);
};

#endif
