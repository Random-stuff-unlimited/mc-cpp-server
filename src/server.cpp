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

Server::Server() : _playerLst(), _config(), _networkManager(nullptr), _worldQuery(_worldManager) {}

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
			g_logger->logGameInfo(INFO, "World loaded successfully: " + _worldData.LevelName, "SERVER");
			g_logger->logGameInfo(INFO,
								  "Spawn Point: X=" + std::to_string(_worldData.SpawnX) + " Y=" + std::to_string(_worldData.SpawnY) +
										  " Z=" + std::to_string(_worldData.SpawnZ),
								  "SERVER");
			g_logger->logGameInfo(INFO, "Random Seed: " + std::to_string(_worldData.RandomSeed), "SERVER");
			g_logger->logGameInfo(INFO, "Game Type: " + std::to_string(_worldData.GameType), "SERVER");
			g_logger->logGameInfo(INFO, "Data Version: " + std::to_string(_worldData.DataVersion), "SERVER");
			g_logger->logGameInfo(INFO, "Difficulty: " + std::to_string(static_cast<int>(_worldData.Difficulty)), "SERVER");
			g_logger->logGameInfo(INFO, "Hardcore: " + std::string(_worldData.hardcore ? "true" : "false"), "SERVER");
			g_logger->logGameInfo(INFO, "Time: " + std::to_string(_worldData.Time), "SERVER");
			g_logger->logGameInfo(INFO, "Day Time: " + std::to_string(_worldData.DayTime), "SERVER");
			g_logger->logGameInfo(INFO, "Generator: " + _worldData.generatorName, "SERVER");
			g_logger->logGameInfo(INFO, "Raining: " + std::string(_worldData.raining ? "true" : "false"), "SERVER");
			g_logger->logGameInfo(INFO, "Thundering: " + std::string(_worldData.thundering ? "true" : "false"), "SERVER");

		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR, "Failed to load world: " + std::string(e.what()), "SERVER");
			return 1;
		}

		std::filesystem::path regionFile = _worldManager.locateRegionFileByChunkCoord(0, 0);
		g_logger->logGameInfo(INFO, "Region File: " + regionFile.string(), "SERVER");
		World::ChunkData chunk = _worldQuery.fetchChunk(0, 0);
		printChunkInfo(chunk);

		size_t workerCount = 4;
		if (workerCount == 0) workerCount = 4; // fallback

		// Create NetworkManager with BOTH required parameters
		_networkManager = new NetworkManager(workerCount, *this);
		_networkManager->startThreads();

		while (true) {
			// g_logger->logGameInfo(INFO, "Server is running...", "Server");
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

void Server::printChunkInfo(const World::ChunkData& chunk) {
	g_logger->logGameInfo(INFO, "========== CHUNK DATA INFO ==========", "SERVER");

	// Basic chunk information
	g_logger->logGameInfo(INFO, "Chunk Coordinates: (" + std::to_string(chunk.chunkX) + ", " + std::to_string(chunk.chunkZ) + ")", "SERVER");
	g_logger->logGameInfo(INFO, "Chunk Empty: " + std::string(chunk.isEmpty() ? "true" : "false"), "SERVER");

	// Data vector sizes
	g_logger->logGameInfo(INFO, "Block Data Size: " + std::to_string(chunk.blockData.size()) + " bytes", "SERVER");
	g_logger->logGameInfo(INFO, "Biome Data Size: " + std::to_string(chunk.biomeData.size()) + " bytes", "SERVER");
	g_logger->logGameInfo(INFO, "Heightmaps Size: " + std::to_string(chunk.heightmaps.size()) + " bytes", "SERVER");
	g_logger->logGameInfo(INFO, "Block Entities Size: " + std::to_string(chunk.blockEntities.size()) + " bytes", "SERVER");
	g_logger->logGameInfo(INFO, "Sky Light Size: " + std::to_string(chunk.skyLight.size()) + " bytes", "SERVER");
	g_logger->logGameInfo(INFO, "Block Light Size: " + std::to_string(chunk.blockLight.size()) + " bytes", "SERVER");

	// Calculate total data size
	size_t totalSize = chunk.blockData.size() + chunk.biomeData.size() + chunk.heightmaps.size() + chunk.blockEntities.size() +
					   chunk.skyLight.size() + chunk.blockLight.size();
	g_logger->logGameInfo(INFO, "Total Chunk Data Size: " + std::to_string(totalSize) + " bytes", "SERVER");

	// Analyze data content
	if (!chunk.blockData.empty()) {
		g_logger->logGameInfo(INFO, "Block Data: Contains " + std::to_string(chunk.blockData.size()) + " bytes of NBT block sections", "SERVER");

		// Try to parse some basic NBT info if possible
		try {
			if (chunk.blockData.size() > 10) {
				// Show first few bytes as hex for debugging
				std::string hexStr = "First 10 bytes (hex): ";
				for (size_t i = 0; i < std::min(static_cast<size_t>(10), chunk.blockData.size()); ++i) {
					char hexChar[4];
					snprintf(hexChar, sizeof(hexChar), "%02X ", static_cast<unsigned char>(chunk.blockData[i]));
					hexStr += hexChar;
				}
				g_logger->logGameInfo(DEBUG, hexStr, "SERVER");
			}
		} catch (const std::exception& e) {
			g_logger->logGameInfo(DEBUG, "Could not analyze block data: " + std::string(e.what()), "SERVER");
		}
	} else {
		g_logger->logGameInfo(INFO, "Block Data: Empty (chunk not generated or air-only)", "SERVER");
	}

	if (!chunk.biomeData.empty()) {
		g_logger->logGameInfo(INFO, "Biome Data: Contains biome information for 4x4x4 block cells", "SERVER");
	} else {
		g_logger->logGameInfo(INFO, "Biome Data: Empty (default biome used)", "SERVER");
	}

	if (!chunk.heightmaps.empty()) {
		g_logger->logGameInfo(INFO, "Heightmaps: Contains terrain height data for performance optimization", "SERVER");
	} else {
		g_logger->logGameInfo(INFO, "Heightmaps: Empty (not calculated)", "SERVER");
	}

	if (!chunk.blockEntities.empty()) {
		g_logger->logGameInfo(INFO,
							  "Block Entities: Contains " + std::to_string(chunk.blockEntities.size()) +
									  " bytes of special blocks (chests, furnaces, etc.)",
							  "SERVER");
	} else {
		g_logger->logGameInfo(INFO, "Block Entities: No special blocks found", "SERVER");
	}

	if (!chunk.skyLight.empty()) {
		g_logger->logGameInfo(INFO, "Sky Light: Contains natural lighting data (2048 bytes expected per 16x16x16 section)", "SERVER");
	} else {
		g_logger->logGameInfo(INFO, "Sky Light: No sky light data (underground sections or not calculated)", "SERVER");
	}

	if (!chunk.blockLight.empty()) {
		g_logger->logGameInfo(INFO, "Block Light: Contains artificial lighting data from torches, glowstone, etc.", "SERVER");
	} else {
		g_logger->logGameInfo(INFO, "Block Light: No block light sources found", "SERVER");
	}

	// Minecraft chunk format info
	g_logger->logGameInfo(INFO, "Note: Minecraft chunks are 16x384x16 blocks (Overworld) divided into 16x16x16 sections", "SERVER");
	g_logger->logGameInfo(INFO, "Note: Block data uses palletized format with NBT compression for efficiency", "SERVER");

	g_logger->logGameInfo(INFO, "====================================", "SERVER");
}
