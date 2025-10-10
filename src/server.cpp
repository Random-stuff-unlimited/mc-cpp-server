#include "config.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <unistd.h>

using json = nlohmann::json;

Server::Server() : _playerLst(), _config(), _networkManager(nullptr) {}

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
