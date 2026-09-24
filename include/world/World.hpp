#ifndef WORLD_HPP
#define WORLD_HPP

#include "world/AnvilImporter.hpp"
#include "world/Chunk.hpp"
#include "world/ChunkStorage.hpp"
#include "world/WorldGenerator.hpp"

class Buffer;

#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class GameData;

// Runs load and save jobs on a few threads. Loads go first: players wait for them, saves can wait
class IoThreadPool {
  public:
	explicit IoThreadPool(size_t threadCount);
	~IoThreadPool();

	void submitLoad(std::function<void()> job);
	void submitSave(std::function<void()> job);
	// Runs every queued job, then stops the threads
	void stop();

  private:
	std::vector<std::thread>		  _threads;
	std::deque<std::function<void()>> _loads;
	std::deque<std::function<void()>> _saves;
	std::mutex						  _mutex;
	std::condition_variable			  _condition;
	bool							  _stopping = false;

	void run();
};

// A dimension: loaded chunks, their loading, generation and saving.
//
// Chunks are reference counted with tickets: acquireChunk() keeps a chunk loaded until the matching releaseChunk().
// A chunk without tickets is unloaded after `unloadDelay` (saved first if modified), so players walking back and
// forth don't reload it. A chunk is looked up in the server's format, then in the vanilla world if there is one,
// then generated. Generated chunks that are never modified aren't saved: they are generated again when needed.
class World {
  public:
	struct Settings {
		std::filesystem::path	  directory;
		std::chrono::seconds	  autosaveInterval{300};
		std::chrono::seconds	  unloadDelay{30};
		size_t					  ioThreads = 2;
	};
	struct Spawn {
		double x, y, z;
	};
	using ChunkCallback = std::function<void(const std::shared_ptr<Chunk>&)>;

	World(const GameData& gameData, const Settings& settings);
	~World();

	// onReady (optional) is called once the chunk is loaded: right away if it already is, otherwise on an I/O thread
	void acquireChunk(int x, int z, ChunkCallback onReady = nullptr);
	void releaseChunk(int x, int z);
	// Called once the chunk is lit, which needs its 8 neighbors loaded too: only lit chunks can be sent to players.
	// The caller must hold a ticket on the chunk
	void whenLit(int x, int z, ChunkCallback onLit);

	// Block state at a world position, -1 if its chunk isn't loaded or y is outside the world
	int getBlock(int x, int y, int z);
	// Light Update packet body for a chunk whose light changed
	struct LightUpdate {
		int					 chunkX, chunkZ;
		std::vector<uint8_t> packet;
	};
	// Returns the previous state, -1 (and changes nothing) if the chunk isn't loaded or y is outside the world.
	// The light around is updated; the chunks whose light changed are added to lightUpdates, to send to their viewers
	int setBlock(int x, int y, int z, uint32_t state, std::vector<LightUpdate>* lightUpdates = nullptr);
	bool	 isAir(uint32_t state) const;
	uint32_t airState() const { return _layout.air; }

	// Chunk Data packet ready to send (framed and compressed), encoded once and cached in the chunk
	std::shared_ptr<const std::vector<uint8_t>> getChunkPacket(const std::shared_ptr<Chunk>& chunk, int compressionThreshold);

	// Unloads idle chunks and autosaves. Call about once per second
	void tick();
	// Saves every modified chunk. The world can't load chunks anymore afterwards
	void shutdown();

	const Spawn&	   getSpawn() const { return _spawn; }
	const std::string& getDimensionName() const { return _dimensionName; }
	int				   getMinY() const { return _layout.minY; }
	int				   getSectionCount() const { return _layout.sectionCount; }
	size_t			   getLoadedChunkCount();

  private:
	struct Entry {
		std::shared_ptr<Chunk>				  chunk;
		int									  tickets = 0;
		bool								  loading = false;
		bool								  saving  = false;
		bool								  lit	  = false;
		bool								  lighting = false;
		std::vector<ChunkCallback>			  litWaiters;
		std::chrono::steady_clock::time_point releasedAt;
		std::vector<ChunkCallback>			  waiters;
	};

	const GameData& _gameData;
	Settings		_settings;
	std::string		_dimensionName;
	Spawn			_spawn{};

	PalettedContainer::Config		_blockConfig{};
	PalettedContainer::Config		_biomeConfig{};
	ChunkStorage::Layout			_layout{};
	std::vector<uint32_t>			_airStates;
	std::unique_ptr<LightTables>	_lightTables;
	std::unique_ptr<DiskPalette>	_blockPalette;
	std::unique_ptr<DiskPalette>	_biomePalette;
	std::unique_ptr<ChunkStorage>	_storage;
	std::unique_ptr<AnvilImporter>	_anvil;
	nlohmann::json					_generatorOptions;
	std::unique_ptr<WorldGenerator> _generator;
	IoThreadPool					_io;

	std::mutex								_chunksMutex;
	std::unordered_map<int64_t, Entry>		_chunks;
	bool									_stopped = false;
	std::chrono::steady_clock::time_point	_lastAutosave;

	void				   loadLevel();
	std::shared_ptr<Chunk> loadOrGenerate(int x, int z);
	void				   finishLoad(int x, int z, std::shared_ptr<Chunk> chunk);
	bool				   readyToLight(int64_t key); // Called with _chunksMutex held
	void				   light(int x, int z);
	void				   save(int64_t key, const std::shared_ptr<Chunk>& chunk, bool unloadAfter);
	bool				   writeChunk(const std::shared_ptr<Chunk>& chunk);
	std::vector<uint8_t>   encodeChunkData(const Chunk& chunk) const;
	std::shared_ptr<Chunk> loadedChunk(int chunkX, int chunkZ);
	void				   relight(int x, int y, int z, std::vector<LightUpdate>* lightUpdates);
	// Light data as in Chunk Data and Light Update: only the sections flagged in skySections / blockSections, or
	// all of them (with the ones below and above the world) when these are null
	void writeLightData(Buffer& buf, const ChunkLight& light, int sectionCount, const std::vector<bool>* skySections,
						const std::vector<bool>* blockSections) const;
};

#endif
