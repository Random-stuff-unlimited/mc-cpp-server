#include "world/World.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"

#include <algorithm>
#include <fstream>
#include <random>
#include <stdexcept>

namespace {
	// Heightmap types the client uses (Heightmap.Types ids)
	constexpr int32_t HEIGHTMAP_WORLD_SURFACE			  = 1;
	constexpr int32_t HEIGHTMAP_MOTION_BLOCKING			  = 4;
	constexpr int32_t HEIGHTMAP_MOTION_BLOCKING_NO_LEAVES = 5;

	// Compression level of cached chunk packets: encoded once, sent many times, so worth a better ratio
	constexpr int CHUNK_PACKET_COMPRESSION_LEVEL = 6;

	nlohmann::json defaultGenerator() {
		return {{"type", "flat"},
				{"biome", "minecraft:plains"},
				{"layers",
				 {{{"block", "minecraft:bedrock"}, {"height", 1}},
				  {{"block", "minecraft:dirt"}, {"height", 2}},
				  {{"block", "minecraft:grass_block"}, {"height", 1}}}}};
	}

	// java.util.BitSet as sent by the client protocol: VarInt long count, then the longs
	void writeBitSet(Buffer& buf, const std::vector<bool>& bits) {
		std::vector<uint64_t> longs((bits.size() + 63) / 64, 0);
		for (size_t i = 0; i < bits.size(); i++) {
			if (bits[i]) longs[i / 64] |= 1ULL << (i % 64);
		}
		while (!longs.empty() && longs.back() == 0) longs.pop_back();
		buf.writeVarInt(static_cast<int32_t>(longs.size()));
		for (uint64_t value : longs) buf.writeLong(static_cast<int64_t>(value));
	}
} // namespace

// ===================== IoThreadPool =====================

IoThreadPool::IoThreadPool(size_t threadCount) {
	for (size_t i = 0; i < std::max<size_t>(1, threadCount); i++) _threads.emplace_back(&IoThreadPool::run, this);
}

IoThreadPool::~IoThreadPool() { stop(); }

void IoThreadPool::submitLoad(std::function<void()> job) {
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_loads.push_back(std::move(job));
	}
	_condition.notify_one();
}

void IoThreadPool::submitSave(std::function<void()> job) {
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_saves.push_back(std::move(job));
	}
	_condition.notify_one();
}

void IoThreadPool::stop() {
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_stopping = true;
	}
	_condition.notify_all();
	for (std::thread& thread : _threads) {
		if (thread.joinable()) thread.join();
	}
	_threads.clear();
}

void IoThreadPool::run() {
	while (true) {
		std::function<void()> job;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_condition.wait(lock, [this] { return _stopping || !_loads.empty() || !_saves.empty(); });
			if (!_loads.empty()) {
				job = std::move(_loads.front());
				_loads.pop_front();
			} else if (!_saves.empty()) {
				job = std::move(_saves.front());
				_saves.pop_front();
			} else {
				return; // Stopping and nothing left
			}
		}
		job();
	}
}

// ===================== World =====================

World::World(const GameData& gameData, const Settings& settings) : _gameData(gameData), _settings(settings), _io(settings.ioThreads) {
	loadLevel();
	_lastAutosave = std::chrono::steady_clock::now();
}

World::~World() { shutdown(); }

void World::loadLevel() {
	std::filesystem::create_directories(_settings.directory);
	std::filesystem::path levelFile = _settings.directory / "level.json";

	nlohmann::json level;
	bool		   levelChanged = false;
	if (std::filesystem::exists(levelFile)) {
		std::ifstream in(levelFile);
		level = nlohmann::json::parse(in);
	} else {
		std::random_device random;
		level = {{"format", 1},
				 {"dimension", "minecraft:overworld"},
				 {"seed", (static_cast<int64_t>(random()) << 32) | random()},
				 {"generator", defaultGenerator()}};
		levelChanged = true;
	}

	_dimensionName							= level.value("dimension", "minecraft:overworld");
	const GameData::Dimension* dimension	= _gameData.getDimension(_dimensionName);
	if (!dimension) throw std::runtime_error("Unknown dimension type " + _dimensionName + " in level.json");

	// Container formats: the direct bit count depends on the number of block states / biomes of this version
	const auto& biomes = _gameData.getSyncedRegistries();
	auto		biomeRegistry =
			std::find_if(biomes.begin(), biomes.end(), [](const GameData::Registry& r) { return r.name == "minecraft:worldgen/biome"; });
	if (biomeRegistry == biomes.end()) throw std::runtime_error("No biome registry in gamedata");
	_blockConfig = {4096, 4, 8, PalettedContainer::bitsFor(static_cast<uint32_t>(_gameData.getBlockStateCount()))};
	_biomeConfig = {64, 1, 3, PalettedContainer::bitsFor(static_cast<uint32_t>(biomeRegistry->entries.size()))};

	for (const char* name : {"minecraft:air", "minecraft:cave_air", "minecraft:void_air"}) {
		int id = _gameData.getDefaultBlockState(name);
		if (id >= 0) _airStates.push_back(static_cast<uint32_t>(id));
	}
	if (_airStates.empty()) throw std::runtime_error("No air block in gamedata");
	int plains = _gameData.getSyncedId("minecraft:worldgen/biome", "minecraft:plains");

	_layout = {dimension->minY, dimension->height / 16, &_blockConfig, &_biomeConfig, _airStates[0], plains < 0 ? 0u : static_cast<uint32_t>(plains)};

	const GameData& data = _gameData;
	_blockPalette		 = std::make_unique<DiskPalette>(
			   _settings.directory / "palettes" / "blocks.txt", _gameData.getBlockStateCount(),
			   [&data](uint32_t id) { return data.getBlockStateName(static_cast<int>(id)); },
			   [&data](const std::string& name) { return data.getBlockStateFromName(name); }, _layout.air);
	const GameData::Registry& biomeEntries = *biomeRegistry;
	_biomePalette						   = std::make_unique<DiskPalette>(
			  _settings.directory / "palettes" / "biomes.txt", biomeEntries.entries.size(),
			  [&biomeEntries](uint32_t id) { return biomeEntries.entries.at(id); },
			  [&biomeEntries](const std::string& name) {
				  auto it = biomeEntries.ids.find(name);
				  return it == biomeEntries.ids.end() ? -1 : it->second;
			  },
			  _layout.defaultBiome);
	if (_blockPalette->unknownCount() > 0) {
		g_logger->logGameInfo(WARN, std::to_string(_blockPalette->unknownCount()) + " block states of this world don't exist in this version, replaced by air",
							  "World");
	}

	_storage = std::make_unique<ChunkStorage>(_settings.directory, _layout, *_blockPalette, *_biomePalette);
	_anvil	 = std::make_unique<AnvilImporter>(_settings.directory, _gameData, _layout);

	_generatorOptions = level.value("generator", defaultGenerator());
	GeneratorSettings generatorSettings(_generatorOptions, _gameData, _layout.minY, dimension->height, level.value("seed", int64_t(0)));
	_generator = createGenerator(generatorSettings);

	if (level.contains("spawn")) {
		_spawn = {level["spawn"].value("x", 0.0), level["spawn"].value("y", 64.0), level["spawn"].value("z", 0.0)};
	} else {
		std::optional<AnvilImporter::Spawn> vanillaSpawn = AnvilImporter::readSpawn(_settings.directory);
		if (vanillaSpawn) {
			_spawn = {vanillaSpawn->x + 0.5, static_cast<double>(vanillaSpawn->y), vanillaSpawn->z + 0.5};
		} else {
			_spawn = {0.5, static_cast<double>(_generator->spawnHeight()), 0.5};
		}
		level["spawn"] = {{"x", _spawn.x}, {"y", _spawn.y}, {"z", _spawn.z}};
		levelChanged   = true;
	}

	if (levelChanged) {
		std::ofstream out(levelFile);
		out << level.dump(2) << "\n";
	}

	g_logger->logGameInfo(INFO,
						  "World " + _settings.directory.filename().string() + ": " + _dimensionName + ", generator " +
								  _generatorOptions.value("type", "?") + (_anvil->available() ? ", importing the vanilla world" : ""),
						  "World");
}

bool World::isAir(uint32_t state) const { return std::find(_airStates.begin(), _airStates.end(), state) != _airStates.end(); }

// ----- Loading -----

void World::acquireChunk(int x, int z, ChunkCallback onReady) {
	int64_t				   key = Chunk::key(x, z);
	std::shared_ptr<Chunk> ready;
	bool				   startLoad = false;
	{
		std::lock_guard<std::mutex> lock(_chunksMutex);
		if (_stopped) return;
		Entry& entry = _chunks[key];
		entry.tickets++;
		if (entry.chunk) {
			ready = entry.chunk;
		} else {
			entry.waiters.push_back(std::move(onReady));
			if (!entry.loading) {
				entry.loading = true;
				startLoad	  = true;
			}
		}
	}
	// Callbacks always run without the world lock: they may call back into the world
	if (ready) {
		onReady(ready);
		return;
	}
	if (startLoad) {
		_io.submitLoad([this, x, z] { finishLoad(x, z, loadOrGenerate(x, z)); });
	}
}

void World::releaseChunk(int x, int z) {
	std::lock_guard<std::mutex> lock(_chunksMutex);
	auto						it = _chunks.find(Chunk::key(x, z));
	if (it == _chunks.end() || it->second.tickets == 0) return;
	if (--it->second.tickets == 0) it->second.releasedAt = std::chrono::steady_clock::now();
}

std::shared_ptr<Chunk> World::loadOrGenerate(int x, int z) {
	std::string position = std::to_string(x) + "," + std::to_string(z);
	try {
		if (std::unique_ptr<Chunk> chunk = _storage->load(x, z)) return chunk;
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Chunk " + position + " is corrupted (" + e.what() + "), generating it again", "World");
	}
	try {
		if (std::unique_ptr<Chunk> chunk = _anvil->load(x, z)) return chunk;
	} catch (const std::exception& e) {
		g_logger->logGameInfo(WARN, "Cannot import vanilla chunk " + position + ": " + e.what(), "World");
	}

	auto		 chunk = std::make_shared<Chunk>(x, z, _layout.minY, _layout.sectionCount, _blockConfig, _biomeConfig, _layout.air, _layout.defaultBiome);
	ChunkBuilder builder(*chunk);
	_generator->generate(builder);
	// Generators are deterministic: an unmodified generated chunk isn't worth saving
	chunk->setDirty(false);
	return chunk;
}

void World::finishLoad(int x, int z, std::shared_ptr<Chunk> chunk) {
	std::vector<ChunkCallback> waiters;
	{
		std::lock_guard<std::mutex> lock(_chunksMutex);
		Entry&						entry = _chunks[Chunk::key(x, z)];
		entry.chunk						  = chunk;
		entry.loading					  = false;
		waiters.swap(entry.waiters);
		if (entry.tickets == 0) entry.releasedAt = std::chrono::steady_clock::now();
	}
	for (ChunkCallback& callback : waiters) callback(chunk);
}

size_t World::getLoadedChunkCount() {
	std::lock_guard<std::mutex> lock(_chunksMutex);
	size_t						count = 0;
	for (const auto& [key, entry] : _chunks) count += entry.chunk != nullptr;
	return count;
}

// ----- Block access -----

std::shared_ptr<Chunk> World::loadedChunk(int chunkX, int chunkZ) {
	std::lock_guard<std::mutex> lock(_chunksMutex);
	auto						it = _chunks.find(Chunk::key(chunkX, chunkZ));
	return it == _chunks.end() ? nullptr : it->second.chunk;
}

int World::getBlock(int x, int y, int z) {
	if (y < _layout.minY || y >= _layout.minY + _layout.sectionCount * 16) return -1;
	std::shared_ptr<Chunk> chunk = loadedChunk(x >> 4, z >> 4);
	if (!chunk) return -1;
	std::lock_guard<std::mutex> lock(chunk->mutex());
	return static_cast<int>(chunk->getBlock(x & 15, y, z & 15));
}

int World::setBlock(int x, int y, int z, uint32_t state) {
	if (y < _layout.minY || y >= _layout.minY + _layout.sectionCount * 16) return -1;
	std::shared_ptr<Chunk> chunk = loadedChunk(x >> 4, z >> 4);
	if (!chunk) return -1;
	std::lock_guard<std::mutex> lock(chunk->mutex());
	int							previous = static_cast<int>(chunk->getBlock(x & 15, y, z & 15));
	if (previous != static_cast<int>(state)) chunk->setBlock(x & 15, y, z & 15, state); // Marks it for saving
	return previous;
}

// ----- Saving and unloading -----

bool World::writeChunk(const std::shared_ptr<Chunk>& chunk) {
	try {
		std::vector<uint8_t> payload;
		{
			std::lock_guard<std::mutex> lock(chunk->mutex());
			if (!chunk->isDirty()) return true;
			chunk->setDirty(false);
			payload = _storage->encode(*chunk);
		}
		_storage->write(chunk->x(), chunk->z(), payload);
		return true;
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Cannot save chunk " + std::to_string(chunk->x()) + "," + std::to_string(chunk->z()) + ": " + e.what(), "World");
		chunk->setDirty(true);
		return false;
	}
}

void World::save(int64_t key, const std::shared_ptr<Chunk>& chunk, bool unloadAfter) {
	writeChunk(chunk);

	std::lock_guard<std::mutex> lock(_chunksMutex);
	auto						it = _chunks.find(key);
	if (it == _chunks.end() || it->second.chunk != chunk) return;
	it->second.saving = false;
	if (unloadAfter && it->second.tickets == 0 && !chunk->isDirty()) _chunks.erase(it);
}

void World::tick() {
	auto now	  = std::chrono::steady_clock::now();
	bool autosave = now - _lastAutosave >= _settings.autosaveInterval;
	if (autosave) _lastAutosave = now;

	std::vector<std::pair<int64_t, std::shared_ptr<Chunk>>> toUnload;
	std::vector<std::pair<int64_t, std::shared_ptr<Chunk>>> toAutosave;
	{
		std::lock_guard<std::mutex> lock(_chunksMutex);
		if (_stopped) return;
		for (auto it = _chunks.begin(); it != _chunks.end();) {
			Entry& entry = it->second;
			if (!entry.chunk || entry.saving) {
				++it;
				continue;
			}
			if (entry.tickets == 0 && now - entry.releasedAt >= _settings.unloadDelay) {
				if (!entry.chunk->isDirty()) {
					it = _chunks.erase(it);
					continue;
				}
				entry.saving = true;
				toUnload.emplace_back(it->first, entry.chunk);
			} else if (autosave && entry.chunk->isDirty()) {
				entry.saving = true;
				toAutosave.emplace_back(it->first, entry.chunk);
			}
			++it;
		}
	}

	for (auto& [key, chunk] : toUnload) _io.submitSave([this, key, chunk] { save(key, chunk, true); });
	for (auto& [key, chunk] : toAutosave) _io.submitSave([this, key, chunk] { save(key, chunk, false); });
	if (autosave && !toAutosave.empty()) {
		g_logger->logGameInfo(INFO, "Autosaving " + std::to_string(toAutosave.size()) + " chunks", "World");
	}
}

void World::shutdown() {
	{
		std::lock_guard<std::mutex> lock(_chunksMutex);
		if (_stopped) return;
		_stopped = true;
	}
	_io.stop(); // Finishes the queued loads and saves

	std::vector<std::shared_ptr<Chunk>> chunks;
	{
		std::lock_guard<std::mutex> lock(_chunksMutex);
		for (const auto& [key, entry] : _chunks) {
			if (entry.chunk) chunks.push_back(entry.chunk);
		}
	}
	size_t saved = 0;
	for (const auto& chunk : chunks) {
		if (chunk->isDirty() && writeChunk(chunk)) saved++;
	}
	_storage->flush();
	g_logger->logGameInfo(INFO, "World saved (" + std::to_string(saved) + " chunks written)", "World");
}

// ----- Network encoding -----

std::shared_ptr<const std::vector<uint8_t>> World::getChunkPacket(const std::shared_ptr<Chunk>& chunk, int compressionThreshold) {
	std::lock_guard<std::mutex> lock(chunk->mutex());
	if (auto cached = chunk->cachedPacket()) return cached;

	auto packet = std::make_shared<const std::vector<uint8_t>>(Packet::buildFrame(
			PacketId::Play::Clientbound::LEVEL_CHUNK_WITH_LIGHT, encodeChunkData(*chunk), compressionThreshold, CHUNK_PACKET_COMPRESSION_LEVEL));
	chunk->setCachedPacket(packet);
	return packet;
}

// Chunk Data and Update Light packet body. Light isn't simulated: sky light is 15 above the highest block of each
// column and 0 below, block light is 0 everywhere
std::vector<uint8_t> World::encodeChunkData(const Chunk& chunk) const {
	const auto& sections	 = chunk.sections();
	const int	sectionCount = static_cast<int>(sections.size());
	const int	height		 = sectionCount * 16;

	// Heightmap: 1 + y of the highest non-air block per column, relative to minY (0 = empty column)
	std::vector<uint16_t> heights(256, 0);
	int					  columnsLeft = 256;
	for (int s = sectionCount - 1; s >= 0 && columnsLeft > 0; s--) {
		const PalettedContainer& blocks = sections[s].blocks;
		if (blocks.isSingleValue() && isAir(blocks.singleValue())) continue;
		for (int column = 0; column < 256; column++) {
			if (heights[column] != 0) continue;
			for (int y = 15; y >= 0; y--) {
				if (!isAir(blocks.get((y << 8) | column))) {
					heights[column] = static_cast<uint16_t>(s * 16 + y + 1);
					columnsLeft--;
					break;
				}
			}
		}
	}

	Buffer buf;
	buf.writeInt(chunk.x());
	buf.writeInt(chunk.z());

	std::vector<uint32_t> heightValues(heights.begin(), heights.end());
	std::vector<uint64_t> packedHeights = PalettedContainer::pack(heightValues, PalettedContainer::bitsFor(height + 1));
	buf.writeVarInt(3);
	for (int32_t type : {HEIGHTMAP_WORLD_SURFACE, HEIGHTMAP_MOTION_BLOCKING, HEIGHTMAP_MOTION_BLOCKING_NO_LEAVES}) {
		buf.writeVarInt(type);
		buf.writeVarInt(static_cast<int32_t>(packedHeights.size()));
		for (uint64_t value : packedHeights) buf.writeLong(static_cast<int64_t>(value));
	}

	Buffer sectionData;
	for (const ChunkSection& section : sections) {
		int16_t nonAir = 0;
		if (section.blocks.isSingleValue()) {
			nonAir = isAir(section.blocks.singleValue()) ? 0 : 4096;
		} else {
			for (uint32_t state : section.blocks.values()) nonAir += !isAir(state);
		}
		sectionData.writeShort(nonAir);
		section.blocks.writeNetwork(sectionData);
		section.biomes.writeNetwork(sectionData);
	}
	buf.writeVarInt(static_cast<int32_t>(sectionData.getData().size()));
	buf.writeBytes(sectionData.getData());

	buf.writeVarInt(0); // Block entities

	// Light sections: one below the world, the world's sections, one above
	int lightSections = sectionCount + 2;
	int lowestTop	  = *std::min_element(heights.begin(), heights.end());
	int highestTop	  = *std::max_element(heights.begin(), heights.end());

	std::vector<bool>				  skyMask(lightSections, false);
	std::vector<bool>				  emptySkyMask(lightSections, false);
	std::vector<bool>				  emptyBlockMask(lightSections, true);
	std::vector<std::vector<uint8_t>> skyArrays;
	for (int i = 0; i < lightSections; i++) {
		int base = (i - 1) * 16; // Relative to minY
		if (base + 16 <= lowestTop) {
			emptySkyMask[i] = true; // Fully underground
			continue;
		}
		skyMask[i] = true;
		if (base >= highestTop) {
			skyArrays.emplace_back(2048, 0xFF); // Fully in the open
			continue;
		}
		std::vector<uint8_t> light(2048, 0);
		for (int y = 0; y < 16; y++) {
			for (int column = 0; column < 256; column++) {
				if (base + y < heights[column]) continue;
				int index = (y << 8) | column;
				light[index >> 1] |= (index & 1) ? 0xF0 : 0x0F;
			}
		}
		skyArrays.push_back(std::move(light));
	}

	writeBitSet(buf, skyMask);
	writeBitSet(buf, std::vector<bool>(lightSections, false)); // Block light mask
	writeBitSet(buf, emptySkyMask);
	writeBitSet(buf, emptyBlockMask);
	buf.writeVarInt(static_cast<int32_t>(skyArrays.size()));
	for (const auto& light : skyArrays) {
		buf.writeVarInt(2048);
		buf.writeBytes(light);
	}
	buf.writeVarInt(0); // Block light arrays

	return std::move(buf.getData());
}
