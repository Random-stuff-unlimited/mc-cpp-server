#include "config.hpp"
#include "lib/filesystem.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/world.hpp"

#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

using json = nlohmann::json;

Server::Server() : _playerLst(), _config(), _networkManager(nullptr), _worldQuery(nullptr) {}

Server::~Server() {
	if (_networkManager) {
		_networkManager->stopThreads();
		delete _networkManager;
	}
}

int Server::start_server() {
	try {
		initializeGlobalLogger();
		if (_config.loadConfig()) {
			g_logger->logGameInfo(ERROR, "Failed to load config", "SERVER");
			return 1;
		}

		// Initialize world manager first
		g_logger->logGameInfo(INFO, "Initializing world manager...", "SERVER");
		// Add any world manager initialization here if needed

		// Load world data
		g_logger->logGameInfo(INFO, "Loading world...", "SERVER");
		try {
			std::string			  worldName	   = _config.getWorldName();
			std::filesystem::path levelDatPath = getPath().parent_path() / worldName / "level.dat";

			if (!std::filesystem::exists(levelDatPath)) {
				g_logger->logGameInfo(ERROR, "level.dat not found at: " + levelDatPath.string(), "SERVER");
				return 1;
			}
			_worldData = _worldManager.loadLevelDat(levelDatPath);

			// Log world information
			// g_logger->logGameInfo(INFO, "World loaded successfully: " + _worldData.LevelName, "SERVER");
			// g_logger->logGameInfo(INFO,
			// 					  "Spawn Point: X=" + std::to_string(_worldData.SpawnX) + " Y=" + std::to_string(_worldData.SpawnY) +
			// 							  " Z=" + std::to_string(_worldData.SpawnZ),
			// 					  "SERVER");
			// g_logger->logGameInfo(INFO, "Random Seed: " + std::to_string(_worldData.RandomSeed), "SERVER");
			// g_logger->logGameInfo(INFO, "Game Type: " + std::to_string(_worldData.GameType), "SERVER");
			// g_logger->logGameInfo(INFO, "Data Version: " + std::to_string(_worldData.DataVersion), "SERVER");
			// g_logger->logGameInfo(INFO, "Difficulty: " + std::to_string(static_cast<int>(_worldData.Difficulty)), "SERVER");
			// g_logger->logGameInfo(INFO, "Hardcore: " + std::string(_worldData.hardcore ? "true" : "false"), "SERVER");
			// g_logger->logGameInfo(INFO, "Time: " + std::to_string(_worldData.Time), "SERVER");
			// g_logger->logGameInfo(INFO, "Day Time: " + std::to_string(_worldData.DayTime), "SERVER");
			// g_logger->logGameInfo(INFO, "Generator: " + _worldData.generatorName, "SERVER");
			// g_logger->logGameInfo(INFO, "Raining: " + std::string(_worldData.raining ? "true" : "false"), "SERVER");
			// g_logger->logGameInfo(INFO, "Thundering: " + std::string(_worldData.thundering ? "true" : "false"), "SERVER");

		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR, "Failed to load world: " + std::string(e.what()), "SERVER");
			return 1;
		}

		// Initialize world query after world data is loaded
		g_logger->logGameInfo(INFO, "Initializing world query system...", "SERVER");
		_worldQuery = std::make_unique<World::OptimizedQuery>(_worldManager);

		// Configure the query based on world data
		World::OptimizedQuery::ParseConfig queryConfig;
		queryConfig.worldHeight		  = 384; // Could be derived from world data if needed
		queryConfig.minY			  = -64;
		queryConfig.cacheSize		  = 128; // Increase cache for server usage
		queryConfig.loadLighting	  = true;
		queryConfig.validateData	  = true;
		queryConfig.loadBlockEntities = true;
		_worldQuery->setParserConfig(queryConfig);

		g_logger->logGameInfo(INFO, "World query system initialized successfully", "SERVER");

		// Test the query system with spawn chunk
		try {
			int32_t spawnChunkX = _worldData.SpawnX >> 4;
			int32_t spawnChunkZ = _worldData.SpawnZ >> 4;
			g_logger->logGameInfo(
					INFO, "Loading spawn chunk (" + std::to_string(spawnChunkX) + ", " + std::to_string(spawnChunkZ) + ")...", "SERVER");

			auto spawnChunk = _worldQuery->fetchChunkCached(spawnChunkX, spawnChunkZ);
			g_logger->logGameInfo(INFO, "Spawn chunk loaded successfully", "SERVER");
		} catch (const std::exception& e) {
			g_logger->logGameInfo(WARN, "Failed to load spawn chunk: " + std::string(e.what()), "SERVER");
		}

		// Preload spawn area for better performance
		g_logger->logGameInfo(INFO, "Preloading spawn area...", "SERVER");
		preloadSpawnArea(3); // Load 7x7 chunks around spawn

		std::filesystem::path regionFile = _worldManager.locateRegionFileByChunkCoord(0, 0);
		g_logger->logGameInfo(INFO, "Region File: " + regionFile.string(), "SERVER");

		// Log initial world query stats
		logWorldQueryStats();

		size_t workerCount = 4;
		if (workerCount == 0) workerCount = 4; // fallback

		// Create NetworkManager with BOTH required parameters
		_networkManager = new NetworkManager(workerCount, *this);
		_networkManager->startThreads();

		while (true) {
			// g_logger->logGameInfo(INFO, "Server is running...", "Server");

			// Log stats every few iterations (in a real server, you'd do this with a timer)
			static int statCounter = 0;
			if (++statCounter % 1000 == 0) {
				logWorldQueryStats();
			}

			sleep(100);
			break;
		}
	} catch (const std::exception& e) {
		// g_logger->logGameInfo(ERROR, std::string("Error: ") + e.what(), "Server");
		return (1);
	}
	return (0);
}

Player* Server::addPlayer(const std::string& name, const PlayerState state, const int socket) {
	Player* newPlayer = nullptr;
	try {
		newPlayer = new Player(name, state, socket, *this);
	} catch (std::exception& e) {
		std::cerr << "[Server]: Error adding player: " << e.what() << std::endl;
		return (nullptr);
	}
	newPlayer->setPlayerName(name);
	newPlayer->setPlayerState(state);
	newPlayer->setSocketFd(socket);
	_playerLst[socket] = newPlayer;
	return (newPlayer);
}

void Server::removePlayer(Player* player) {
	if (!player) {
		return;
	}
	int socket = player->getSocketFd();
	delete player;
	_playerLst.erase(socket);
}

Player* Server::addTempPlayer(const std::string& name, const PlayerState state, const int socket) {
	Player* newPlayer = nullptr;
	try {
		newPlayer = new Player(name, state, socket, *this);
	} catch (std::exception& e) {
		std::cerr << "[Server]: Error adding temp player: " << e.what() << std::endl;
		return (nullptr);
	}
	newPlayer->setPlayerName(name);
	newPlayer->setPlayerState(state);
	newPlayer->setSocketFd(socket);

	std::lock_guard<std::mutex> lock(_tempPlayerLock);
	_tempPlayerLst[socket] = newPlayer;
	// g_logger->logGameInfo(INFO, "Added temp player on socket " + std::to_string(socket),
	// "Server");
	return (newPlayer);
}

void Server::removeTempPlayer(Player* player) {
	if (!player) {
		return;
	}
	int							socket = player->getSocketFd();
	std::lock_guard<std::mutex> lock(_tempPlayerLock);
	_tempPlayerLst.erase(socket);
	delete player;
	// g_logger->logGameInfo(INFO, "Removed temp player from socket " + std::to_string(socket),
	// "Server");
}

void Server::promoteTempPlayer(Player* player) {
	if (!player) return;
	int socket = player->getSocketFd();

	std::lock_guard<std::mutex> lockTemp(_tempPlayerLock);
	_tempPlayerLst.erase(socket);

	std::lock_guard<std::mutex> lockPlayer(_playerLock);
	_playerLst[socket] = player;

	// g_logger->logGameInfo(INFO, "Promoted temp player to main list on socket " +
	// std::to_string(socket), "Server");
}

void Server::removePlayerFromAnyList(Player* player) {
	if (!player) {
		return;
	}
	int socket = player->getSocketFd();

	{
		std::lock_guard<std::mutex> lock(_tempPlayerLock);
		auto						temp_it = _tempPlayerLst.find(socket);
		if (temp_it != _tempPlayerLst.end()) {
			_tempPlayerLst.erase(socket);
			delete player;
			// g_logger->logGameInfo(INFO, "Removed temp player from socket " +
			// std::to_string(socket), "Server");
			return;
		}
	}

	{
		std::lock_guard<std::mutex> lock(_playerLock);
		auto						main_it = _playerLst.find(socket);
		if (main_it != _playerLst.end()) {
			_playerLst.erase(socket);
			delete player;
			// g_logger->logGameInfo(INFO, "Removed main player from socket " +
			// std::to_string(socket), "Server");
			return;
		}
	}
	delete player;
	// g_logger->logGameInfo(INFO, "Deleted orphaned player from socket " + std::to_string(socket),
	// "Server");
}

void Server::addPlayerToSample(const std::string& name) { _playerSample.push_back(name); }

void Server::removePlayerToSample(const std::string& name) {
	for (size_t i = 0; i < _playerLst.size(); i++)
		if (_playerSample[i] == name) {
			_playerSample.erase(_playerSample.begin() + i);
			break;
		}
}

int	 Server::getAmountOnline() { return _playerLst.size(); }
json Server::getPlayerSample() { return _playerSample; }

// // World query utility methods implementation
// std::shared_ptr<World::ChunkData> Server::loadChunkForPlayer(int32_t chunkX, int32_t chunkZ) {
// 	if (!_worldQuery) {
// 		g_logger->logGameInfo(ERROR, "World query not initialized", "SERVER");
// 		return std::make_shared<World::ChunkData>(World::ChunkData::generateEmpty(chunkX, chunkZ, 384));
// 	}

// 	try {
// 		return _worldQuery->fetchChunkCached(chunkX, chunkZ);
// 	} catch (const std::exception& e) {
// 		g_logger->logGameInfo(ERROR, "Failed to load chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + "): " + e.what(), "SERVER");
// 		return std::make_shared<World::ChunkData>(World::ChunkData::generateEmpty(chunkX, chunkZ, 384));
// 	}
// }

// std::vector<std::shared_ptr<World::ChunkData>> Server::loadPlayerViewArea(int32_t centerChunkX, int32_t centerChunkZ, int radius) {
// 	std::vector<std::pair<int32_t, int32_t>> chunkCoords;

// 	// Generate chunk coordinates in a square around the center
// 	for (int x = -radius; x <= radius; ++x) {
// 		for (int z = -radius; z <= radius; ++z) {
// 			chunkCoords.emplace_back(centerChunkX + x, centerChunkZ + z);
// 		}
// 	}

// 	if (!_worldQuery) {
// 		g_logger->logGameInfo(ERROR, "World query not initialized", "SERVER");
// 		std::vector<std::shared_ptr<World::ChunkData>> emptyChunks;
// 		for (const auto& [chunkX, chunkZ] : chunkCoords) {
// 			emptyChunks.push_back(std::make_shared<World::ChunkData>(World::ChunkData::generateEmpty(chunkX, chunkZ, 384)));
// 		}
// 		return emptyChunks;
// 	}

// 	try {
// 		g_logger->logGameInfo(INFO, "Loading " + std::to_string(chunkCoords.size()) + " chunks for player view area", "SERVER");

// 		// Convert batch result to shared_ptr vector
// 		auto										   batchResult = _worldQuery->fetchChunkBatch(chunkCoords);
// 		std::vector<std::shared_ptr<World::ChunkData>> chunks;
// 		for (auto& chunk : batchResult) {
// 			chunks.push_back(std::make_shared<World::ChunkData>(std::move(chunk)));
// 		}
// 		return chunks;
// 	} catch (const std::exception& e) {
// 		g_logger->logGameInfo(ERROR, "Failed to load player view area: " + std::string(e.what()), "SERVER");

// 		// Fallback to individual loading
// 		std::vector<std::shared_ptr<World::ChunkData>> chunks;
// 		for (const auto& [chunkX, chunkZ] : chunkCoords) {
// 			chunks.push_back(loadChunkForPlayer(chunkX, chunkZ));
// 		}
// 		return chunks;
// 	}
// }

void Server::preloadSpawnArea(int radius) {
	if (!_worldQuery) {
		g_logger->logGameInfo(ERROR, "World query not initialized", "SERVER");
		return;
	}

	int32_t spawnChunkX = _worldData.SpawnX >> 4;
	int32_t spawnChunkZ = _worldData.SpawnZ >> 4;

	std::vector<std::pair<int32_t, int32_t>> spawnChunks;
	for (int x = -radius; x <= radius; ++x) {
		for (int z = -radius; z <= radius; ++z) {
			spawnChunks.emplace_back(spawnChunkX + x, spawnChunkZ + z);
		}
	}

	g_logger->logGameInfo(INFO, "Preloading spawn area with " + std::to_string(spawnChunks.size()) + " chunks", "SERVER");

	try {
		_worldQuery->preloadChunks(spawnChunks);
		g_logger->logGameInfo(INFO, "Spawn area preloaded successfully", "SERVER");
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Failed to preload spawn area: " + std::string(e.what()), "SERVER");
	}
}

void Server::logWorldQueryStats() {
	if (!_worldQuery) {
		g_logger->logGameInfo(WARN, "World query not initialized", "SERVER");
		return;
	}

	auto metrics = _worldQuery->getMetrics();
	g_logger->logGameInfo(INFO,
						  "World Query Stats - Queries: " + std::to_string(metrics.totalQueries) + ", Cache Hits: " +
								  std::to_string(metrics.cacheHits) + ", Cache Size: " + std::to_string(_worldQuery->getCacheSize()) +
								  ", Avg Load Time: " + std::to_string(metrics.averageLoadTime * 1000.0) + "ms",
						  "SERVER");
}

bool Server::isChunkAvailable(int32_t chunkX, int32_t chunkZ) {
	if (!_worldQuery) {
		return false;
	}

	// Check if chunk is cached first
	if (_worldQuery->isChunkCached(chunkX, chunkZ)) {
		return true;
	}

	// Check if region file exists
	try {
		std::filesystem::path regionPath = _worldManager.locateRegionFileByChunkCoord(chunkX, chunkZ);
		return std::filesystem::exists(regionPath);
	} catch (const std::exception&) {
		return false;
	}
}
