#ifndef WORLD_HPP
#define WORLD_HPP

#include "world/AnvilImporter.hpp"
#include "world/Chunk.hpp"
#include "world/ChunkStorage.hpp"
#include "world/WorldGenerator.hpp"
#include "lib/JavaRandom.hpp"

class Buffer;

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
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
		int						  compressionThreshold = 256; // Of the network, for the cached Chunk Data packets
		// A new world (no level.json yet): its dimension, generator (the default superflat if null) and seed (random if
		// not set)
		std::string				  dimension = "minecraft:overworld";
		nlohmann::json			  generator;
		std::optional<int64_t>	  seed;
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
	// Its Chunk Data packet is encoded by then (on an I/O thread). The caller must hold a ticket on the chunk
	void whenLit(int x, int z, ChunkCallback onLit);

	// The chunk if it is loaded, null otherwise (still loading or unloaded). Blocks are changed through Level
	std::shared_ptr<Chunk> loadedChunk(int chunkX, int chunkZ);
	// Game thread: the chunk, loaded (or generated) right now on this thread if it isn't (vanilla's getChunk, which
	// waits too): portals looking for their exit. Without a ticket it unloads after the delay like any other.
	// null once the world is shut down
	std::shared_ptr<Chunk> loadChunkNow(int chunkX, int chunkZ);
	const std::filesystem::path& directory() const { return _settings.directory; }
	// Called on an I/O thread each time a chunk finishes loading
	void setChunkLoadListener(ChunkCallback listener) { _loadListener = std::move(listener); }

	// Light sections changed in each chunk, accumulated over several relights (sent once per tick)
	struct LightChange {
		std::vector<bool> sky, block;	 // Per section of the world
		bool			  full = false; // Recomputed entirely: every section
	};
	using LightChanges = std::unordered_map<int64_t, LightChange>; // By Chunk::key
	// Updates the light around a block that changed (its chunk must be loaded), adding the changed sections to changes
	void relight(int x, int y, int z, LightChanges& changes);
	// Light Update packet body for these sections of the chunk
	std::vector<uint8_t> lightUpdatePacket(const Chunk& chunk, const LightChange& change) const;
	bool				 isAir(uint32_t state) const;
	uint32_t airState() const { return _layout.air; }

	// Chunk Data packet ready to send (framed and compressed), encoded once and cached in the chunk. Encoding is slow:
	// done on the I/O threads before whenLit's callback, so the game thread normally only gets the cached one
	std::shared_ptr<const std::vector<uint8_t>> getChunkPacket(const std::shared_ptr<Chunk>& chunk);

	// Unloads idle chunks and autosaves. Call about once per second. True when it autosaved (players are saved too)
	bool tick();
	// The other dimensions share the overworld's clock and weather flags (vanilla's DerivedLevelData): their time
	// and weather cycle are the overworld's, only their rain and thunder levels are their own
	void	setPrimary(World* overworld) { _primary = overworld; }
	bool	isPrimary() const { return _primary == nullptr; }
	// Game thread, once per tick unless the game is frozen: advances the game time and the time of day (the
	// overworld's only: ServerLevel.tickTime is off in the other dimensions)
	void	tickTime();
	// One tick of the rain and thunder (ServerLevel.advanceWeatherCycle): in a dimension with a sky, the overworld's
	// flags toggle when their countdown ends and the levels the clients see ramp toward them. The game events are
	// sent by Server
	void tickWeather(bool hasSkyLight);
	// ServerLevel.resetWeatherCycle: no rain or thunder (after a night skipped in bed), the levels ramp down
	void resetWeatherCycle();
	// ServerLevel.setWeatherParameters, for /weather clear, rain or thunder: sets the flags (and for clear, a long
	// clear time so the weather can't start again before it ends). The clients are told by Server's next tick
	void setWeather(bool raining, bool thundering);
	// The weather the clients see (ServerLevel.isRaining / isThundering): the levels, ramping 0..1
	bool  isRaining() const { return _weather.rainLevel > 0.2F; }
	// Level.getThunderLevel is the thunder level times the rain level
	bool  isThundering() const { return _weather.thunderLevel * _weather.rainLevel > 0.9F; }
	float rainLevel() const { return _weather.rainLevel; }
	float thunderLevel() const { return _weather.thunderLevel; }
	// LevelData.isRaining / isThundering: the flags (the levels follow them)
	bool  rainingFlag() const { return _primary ? _primary->rainingFlag() : _weather.raining; }
	bool  thunderingFlag() const { return _primary ? _primary->thunderingFlag() : _weather.thundering; }
	int64_t getGameTime() const { return _primary ? _primary->getGameTime() : _gameTime.load(std::memory_order_relaxed); }
	int64_t getDayTime() const { return _primary ? _primary->getDayTime() : _dayTime; }
	void	setDayTime(int64_t time) {
		   if (_primary) _primary->setDayTime(time);
		   else _dayTime = time;
	}
	int64_t getSeed() const { return _seed; }
	// The generator's sea level (63 without one)
	int		getSeaLevel() const;
	// Saves every modified chunk. The world can't load chunks anymore afterwards
	void shutdown();
	// Runs a save job on the I/O threads (at once after shutdown). The jobs queued before shutdown are all run
	void submitSave(std::function<void()> job);

	const Spawn&	   getSpawn() const { return _spawn; }
	// The world's spawn point (from level.json, the vanilla world's level.dat, or the generator): where players
	// without a bed appear. Saved to level.json
	void			   setSpawn(double x, double y, double z);
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
	ChunkCallback							_loadListener;
	std::chrono::steady_clock::time_point	_lastAutosave;
	std::atomic<int64_t>					_gameTime{0}; // Ticks since the world was created (read when saving)
	int64_t									_dayTime  = 0; // Time of day: 0 = sunrise, 24000 ticks a day
	World*									_primary  = nullptr; // The overworld, for the other dimensions
	int64_t									_seed	  = 0;
	// Rain and thunder (ServerLevelData + ServerLevel.updateWeather): the raw flags, their countdowns, and the
	// previous and current levels the clients see (oRainLevel / rainLevel)
	struct Weather {
		bool  raining = false, thundering = false;
		int	  clearWeatherTime = 0, rainTime = 0, thunderTime = 0;
		float oRainLevel = 0, rainLevel = 0, oThunderLevel = 0, thunderLevel = 0;
	};
	Weather		_weather;
	JavaRandom	_random{0}; // The world's random (seeded with the level's seed): the weather

	void				   loadLevel();
	// Writes the game time to level.json
	void				   saveLevel();
	std::shared_ptr<Chunk> loadOrGenerate(int x, int z);
	std::shared_ptr<Chunk> loadOrGenerateBlocks(int x, int z);
	// The block entities its blocks need and don't have yet (generated or imported chunks): created at once, so the
	// chunk's packet has them
	void				   addMissingBlockEntities(Chunk& chunk) const;
	void				   finishLoad(int x, int z, std::shared_ptr<Chunk> chunk);
	bool				   readyToLight(int64_t key); // Called with _chunksMutex held
	void				   light(int x, int z);
	void				   save(int64_t key, const std::shared_ptr<Chunk>& chunk, bool unloadAfter);
	bool				   writeChunk(const std::shared_ptr<Chunk>& chunk);
	std::vector<uint8_t>   encodeChunkData(const Chunk& chunk) const;
	// Chunk is ticking when lit and kept by a player. Called with _chunksMutex held
	static void			   updateTicking(Entry& entry);
	// Light data as in Chunk Data and Light Update: only the sections flagged in skySections / blockSections, or
	// all of them (with the ones below and above the world) when these are null
	void writeLightData(Buffer& buf, const ChunkLight& light, int sectionCount, const std::vector<bool>* skySections,
						const std::vector<bool>* blockSections) const;
};

#endif
