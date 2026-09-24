#include "world/ChunkStorage.hpp"

#include "lib/compression.hpp"
#include "logger.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
	constexpr uint8_t PAYLOAD_VERSION  = 1;
	constexpr uint8_t COMPRESSION_ZLIB = 1;
	constexpr int	  ZLIB_LEVEL	   = 4;

	class ByteWriter {
	  public:
		std::vector<uint8_t> out;

		void u8(uint8_t v) { out.push_back(v); }
		void u32(uint32_t v) {
			for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
		}
		void u64(uint64_t v) {
			for (int i = 0; i < 8; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
		}
		void varint(uint32_t v) {
			while (v >= 0x80) {
				out.push_back(static_cast<uint8_t>(v | 0x80));
				v >>= 7;
			}
			out.push_back(static_cast<uint8_t>(v));
		}
	};

	class ByteReader {
	  public:
		ByteReader(const uint8_t* data, size_t size) : _data(data), _size(size) {}

		uint8_t u8() {
			need(1);
			return _data[_pos++];
		}
		uint32_t u32() {
			need(4);
			uint32_t v = 0;
			for (int i = 0; i < 4; i++) v |= static_cast<uint32_t>(_data[_pos++]) << (8 * i);
			return v;
		}
		uint64_t u64() {
			need(8);
			uint64_t v = 0;
			for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(_data[_pos++]) << (8 * i);
			return v;
		}
		uint32_t varint() {
			uint32_t v = 0;
			for (int shift = 0; shift < 35; shift += 7) {
				uint8_t b = u8();
				v |= static_cast<uint32_t>(b & 0x7F) << shift;
				if (!(b & 0x80)) return v;
			}
			throw std::runtime_error("Bad VarInt in chunk data");
		}

	  private:
		const uint8_t* _data;
		size_t		   _size;
		size_t		   _pos = 0;

		void need(size_t n) {
			if (_pos + n > _size) throw std::runtime_error("Truncated chunk data");
		}
	};

	void writeContainer(ByteWriter& w, const PalettedContainer& container, DiskPalette& diskPalette) {
		std::vector<uint32_t> palette;
		std::vector<uint64_t> data;
		uint8_t				  bits = 0;
		container.compactPalette(palette, bits, data);

		w.u8(bits);
		w.varint(static_cast<uint32_t>(palette.size()));
		for (uint32_t value : palette) w.varint(diskPalette.toDisk(value));
		for (uint64_t value : data) w.u64(value);
	}

	PalettedContainer readContainer(ByteReader& r, const PalettedContainer::Config& config, const DiskPalette& diskPalette) {
		uint8_t	 bits  = r.u8();
		uint32_t count = r.varint();
		if (count == 0 || count > config.entries || bits > 32) throw std::runtime_error("Bad palette in chunk data");

		std::vector<uint32_t> palette(count);
		for (uint32_t& value : palette) value = diskPalette.toRuntime(r.varint());
		if (bits == 0) return PalettedContainer(config, palette[0]);

		uint32_t			  perLong = 64 / bits;
		std::vector<uint64_t> data((config.entries + perLong - 1) / perLong);
		for (uint64_t& value : data) value = r.u64();
		return PalettedContainer::fromPalette(config, palette, bits, data);
	}

	void writeAll(int fd, const void* data, size_t size, off_t offset) {
		const uint8_t* bytes = static_cast<const uint8_t*>(data);
		while (size > 0) {
			ssize_t n = pwrite(fd, bytes, size, offset);
			if (n < 0) {
				if (errno == EINTR) continue;
				throw std::runtime_error(std::string("Region write failed: ") + std::strerror(errno));
			}
			bytes += n;
			size -= n;
			offset += n;
		}
	}

	void readAll(int fd, void* data, size_t size, off_t offset) {
		uint8_t* bytes = static_cast<uint8_t*>(data);
		while (size > 0) {
			ssize_t n = pread(fd, bytes, size, offset);
			if (n < 0 && errno == EINTR) continue;
			if (n <= 0) throw std::runtime_error("Region read failed");
			bytes += n;
			size -= n;
			offset += n;
		}
	}
} // namespace

// ===================== DiskPalette =====================

DiskPalette::DiskPalette(std::filesystem::path file, size_t runtimeCount, NameOf nameOf, IdOf idOf, uint32_t fallback)
	: _file(std::move(file)), _nameOf(std::move(nameOf)), _fallback(fallback), _runtimeToDisk(runtimeCount, -1) {
	std::filesystem::create_directories(_file.parent_path());

	std::ifstream in(_file);
	std::string	  name;
	while (std::getline(in, name)) {
		int runtimeId = idOf(name);
		if (runtimeId < 0) {
			_unknown++;
			_diskToRuntime.push_back(_fallback);
			continue;
		}
		if (_runtimeToDisk[runtimeId] < 0) _runtimeToDisk[runtimeId] = static_cast<int32_t>(_diskToRuntime.size());
		_diskToRuntime.push_back(static_cast<uint32_t>(runtimeId));
	}

	_append = std::fopen(_file.c_str(), "a");
	if (!_append) throw std::runtime_error("Cannot open " + _file.string());
}

DiskPalette::~DiskPalette() {
	if (_append) std::fclose(_append);
}

uint32_t DiskPalette::toDisk(uint32_t runtimeId) {
	std::lock_guard<std::mutex> lock(_mutex);
	int32_t						diskId = _runtimeToDisk.at(runtimeId);
	if (diskId >= 0) return static_cast<uint32_t>(diskId);

	// New value: append it before any chunk referencing it is written
	std::string line = _nameOf(runtimeId) + "\n";
	if (std::fputs(line.c_str(), _append) < 0 || std::fflush(_append) != 0) throw std::runtime_error("Cannot write " + _file.string());
	diskId					  = static_cast<int32_t>(_diskToRuntime.size());
	_runtimeToDisk[runtimeId] = diskId;
	_diskToRuntime.push_back(runtimeId);
	return static_cast<uint32_t>(diskId);
}

uint32_t DiskPalette::toRuntime(uint32_t diskId) const {
	std::lock_guard<std::mutex> lock(_mutex);
	if (diskId >= _diskToRuntime.size()) throw std::runtime_error("Unknown palette id " + std::to_string(diskId) + " in " + _file.string());
	return _diskToRuntime[diskId];
}

// ===================== RegionFile =====================

RegionFile::RegionFile(const std::filesystem::path& path) {
	_fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
	if (_fd < 0) throw std::runtime_error("Cannot open region " + path.string() + ": " + std::strerror(errno));

	struct stat st{};
	fstat(_fd, &st);
	std::memset(_offsets, 0, sizeof(_offsets));
	std::memset(_sizes, 0, sizeof(_sizes));

	if (st.st_size == 0) {
		std::vector<uint8_t> header(HEADER_SECTORS * SECTOR_SIZE, 0);
		std::memcpy(header.data(), "MCCR", 4);
		header[4] = 1; // Format version, little-endian u32
		writeAll(_fd, header.data(), header.size(), 0);
		markSectors(0, HEADER_SECTORS, true);
		return;
	}

	std::vector<uint8_t> header(ENTRIES_OFFSET + 1024 * 8);
	readAll(_fd, header.data(), header.size(), 0);
	if (std::memcmp(header.data(), "MCCR", 4) != 0) {
		close(_fd);
		throw std::runtime_error("Not a region file: " + path.string());
	}

	uint32_t fileSectors = static_cast<uint32_t>((st.st_size + SECTOR_SIZE - 1) / SECTOR_SIZE);
	markSectors(0, HEADER_SECTORS, true);
	for (int i = 0; i < 1024; i++) {
		const uint8_t* entry  = header.data() + ENTRIES_OFFSET + i * 8;
		uint32_t	   offset = entry[0] | (entry[1] << 8) | (entry[2] << 16) | (static_cast<uint32_t>(entry[3]) << 24);
		uint32_t	   size	  = entry[4] | (entry[5] << 8) | (entry[6] << 16) | (static_cast<uint32_t>(entry[7]) << 24);
		uint32_t	   count  = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
		if (size == 0) continue;
		if (offset < HEADER_SECTORS || offset + count > fileSectors) {
			g_logger->logGameInfo(WARN, "Dropping invalid chunk entry " + std::to_string(i) + " in " + path.string(), "World");
			continue;
		}
		_offsets[i] = offset;
		_sizes[i]	= size;
		markSectors(offset, count, true);
	}
}

RegionFile::~RegionFile() {
	if (_fd >= 0) {
		fdatasync(_fd);
		close(_fd);
	}
}

void RegionFile::markSectors(uint32_t first, uint32_t count, bool used) {
	if (_usedSectors.size() < first + count) _usedSectors.resize(first + count, false);
	for (uint32_t i = 0; i < count; i++) _usedSectors[first + i] = used;
}

uint32_t RegionFile::allocate(uint32_t count) {
	uint32_t run = 0;
	for (uint32_t i = HEADER_SECTORS; i < _usedSectors.size(); i++) {
		run = _usedSectors[i] ? 0 : run + 1;
		if (run == count) return i - count + 1;
	}
	// Extend the file, reusing the free sectors at its end
	return static_cast<uint32_t>(_usedSectors.size()) - run;
}

bool RegionFile::read(int index, std::vector<uint8_t>& blob) {
	std::lock_guard<std::mutex> lock(_mutex);
	if (_sizes[index] == 0) return false;
	blob.resize(_sizes[index]);
	readAll(_fd, blob.data(), blob.size(), static_cast<off_t>(_offsets[index]) * SECTOR_SIZE);
	return true;
}

void RegionFile::write(int index, const std::vector<uint8_t>& blob) {
	std::lock_guard<std::mutex> lock(_mutex);
	uint32_t					count  = static_cast<uint32_t>((blob.size() + SECTOR_SIZE - 1) / SECTOR_SIZE);
	uint32_t					offset = allocate(count);

	// New data first, then the header entry: the old version stays valid until the entry points to the new one
	writeAll(_fd, blob.data(), blob.size(), static_cast<off_t>(offset) * SECTOR_SIZE);
	uint8_t	 entry[8];
	uint32_t size = static_cast<uint32_t>(blob.size());
	for (int i = 0; i < 4; i++) {
		entry[i]	 = static_cast<uint8_t>(offset >> (8 * i));
		entry[4 + i] = static_cast<uint8_t>(size >> (8 * i));
	}
	writeAll(_fd, entry, sizeof(entry), ENTRIES_OFFSET + index * 8);

	if (_sizes[index] != 0) markSectors(_offsets[index], (_sizes[index] + SECTOR_SIZE - 1) / SECTOR_SIZE, false);
	markSectors(offset, count, true);
	_offsets[index] = offset;
	_sizes[index]	= size;
}

void RegionFile::sync() {
	std::lock_guard<std::mutex> lock(_mutex);
	fdatasync(_fd);
}

// ===================== ChunkStorage =====================

ChunkStorage::ChunkStorage(const std::filesystem::path& worldDirectory, const Layout& layout, DiskPalette& blocks, DiskPalette& biomes)
	: _regionDirectory(worldDirectory / "regions"), _layout(layout), _blocks(blocks), _biomes(biomes) {
	std::filesystem::create_directories(_regionDirectory);
}

std::shared_ptr<RegionFile> ChunkStorage::region(int chunkX, int chunkZ, bool create) {
	int		regionX = chunkX >> 5;
	int		regionZ = chunkZ >> 5;
	int64_t key		= Chunk::key(regionX, regionZ);

	std::lock_guard<std::mutex> lock(_regionsMutex);
	_regionLastUse[key] = ++_useCounter;
	auto it				= _regions.find(key);
	if (it != _regions.end()) return it->second;

	std::filesystem::path path = _regionDirectory / ("r." + std::to_string(regionX) + "." + std::to_string(regionZ) + ".ccr");
	if (!create && !std::filesystem::exists(path)) {
		_regionLastUse.erase(key);
		return nullptr;
	}

	if (_regions.size() >= MAX_OPEN_REGIONS) {
		// Close the least recently used region (it stays open while another thread still uses it)
		auto oldest = _regions.begin();
		for (auto r = _regions.begin(); r != _regions.end(); ++r) {
			if (_regionLastUse[r->first] < _regionLastUse[oldest->first]) oldest = r;
		}
		_regionLastUse.erase(oldest->first);
		_regions.erase(oldest);
	}

	auto file	  = std::make_shared<RegionFile>(path);
	_regions[key] = file;
	return file;
}

std::unique_ptr<Chunk> ChunkStorage::load(int x, int z) {
	std::shared_ptr<RegionFile> file = region(x, z, false);
	if (!file) return nullptr;

	std::vector<uint8_t> blob;
	if (!file->read((z & 31) * 32 + (x & 31), blob)) return nullptr;

	ByteReader header(blob.data(), blob.size());
	uint8_t	   compression = header.u8();
	uint32_t   rawSize	   = header.u32();
	if (compression != COMPRESSION_ZLIB || rawSize > 64 * 1024 * 1024) throw std::runtime_error("Unsupported chunk compression");
	std::vector<uint8_t> payload = compression::zlibDecompress(blob.data() + 5, blob.size() - 5, rawSize);

	ByteReader r(payload.data(), payload.size());
	if (r.u8() != PAYLOAD_VERSION) throw std::runtime_error("Unsupported chunk format version");
	int sectionCount = r.u8();

	auto chunk = std::make_unique<Chunk>(x, z, _layout.minY, _layout.sectionCount, *_layout.blockConfig, *_layout.biomeConfig, _layout.air,
										 _layout.defaultBiome);
	for (int i = 0; i < sectionCount; i++) {
		PalettedContainer blocks = readContainer(r, *_layout.blockConfig, _blocks);
		PalettedContainer biomes = readContainer(r, *_layout.biomeConfig, _biomes);
		// Sections beyond the current world height (dimension got smaller) are dropped
		if (i < _layout.sectionCount) chunk->sections()[i] = {std::move(blocks), std::move(biomes)};
	}
	return chunk;
}

std::vector<uint8_t> ChunkStorage::encode(const Chunk& chunk) {
	ByteWriter w;
	w.u8(PAYLOAD_VERSION);
	w.u8(static_cast<uint8_t>(chunk.sections().size()));
	for (const ChunkSection& section : chunk.sections()) {
		writeContainer(w, section.blocks, _blocks);
		writeContainer(w, section.biomes, _biomes);
	}
	return std::move(w.out);
}

void ChunkStorage::write(int x, int z, const std::vector<uint8_t>& payload) {
	std::vector<uint8_t> compressed = compression::zlibCompress(payload.data(), payload.size(), ZLIB_LEVEL);

	ByteWriter blob;
	blob.u8(COMPRESSION_ZLIB);
	blob.u32(static_cast<uint32_t>(payload.size()));
	blob.out.insert(blob.out.end(), compressed.begin(), compressed.end());

	region(x, z, true)->write((z & 31) * 32 + (x & 31), blob.out);
}

void ChunkStorage::flush() {
	std::lock_guard<std::mutex> lock(_regionsMutex);
	for (auto& [key, file] : _regions) file->sync();
}
