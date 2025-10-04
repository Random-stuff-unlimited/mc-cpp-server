#include "json.hpp"
#include "logger.hpp"
#include "networking.hpp"
#include "player.hpp"
#include "server.hpp"

#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using json = nlohmann::json;

Server::Server()
    : _playerLst(), _protocolVersion(770), _serverSize(-100000000), _gameVersion("1.12.5"),
      _serverMOTD(), _serverPort(25565), _serverAddr("0.0.0.0"), _networkManager(nullptr) {}

Server::~Server() {
	if (_networkManager) {
		_networkManager->stopThreads();
		delete _networkManager;
	}
}

int Server::start_server() {
	try {
	    initializeGlobalLogger();
		Server::loadConfig();
		size_t workerCount = 4;
		if (workerCount == 0)
			workerCount = 4; // fallback

		// Create NetworkManager with BOTH required parameters
		_networkManager = new NetworkManager(workerCount, *this);
		_networkManager->startThreads();

		while (true) {
			g_logger->logGameInfo(INFO, "Server is running...", "Server");
			sleep(10);
			break;
		}
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, std::string("Error: ") + e.what(), "Server");
		return (1);
	}
	return (0);
}

int Server::loadConfig() {
	std::ifstream inputFile(ConfigFileName);

	if (!inputFile.is_open()) {
		std::cerr << "[Server] Error: Could not open " << ConfigFileName << std::endl;
		return 1;
	}

	json j;

	try {
		inputFile >> j;

		g_logger->logGameInfo(INFO, "Successfully parsed " + std::string(ConfigFileName) + "!", "Server");
		_gameVersion = j["version"]["name"];
		g_logger->logGameInfo(INFO, "Game version: " + _gameVersion, "Server");
		_protocolVersion = j["version"]["protocol"];
		g_logger->logGameInfo(INFO, "Protocol version: " + std::to_string(_protocolVersion), "Server");
		_serverSize = j["server"]["max-players"];
		g_logger->logGameInfo(INFO, "Server size: " + std::to_string(_serverSize), "Server");
		_serverMOTD = j["server"]["motd"];
		g_logger->logGameInfo(INFO, "Server MOTD: " + _serverMOTD, "Server");
		_serverAddr = j["server"]["ip-address"];
		g_logger->logGameInfo(INFO, "Server IP address : " + _serverAddr, "Server");
		_serverPort = j["server"]["port"];
		g_logger->logGameInfo(INFO, "Server port: " + std::to_string(_serverPort), "Server");
	} catch (json::parse_error& e) {
		std::cerr << "[Server]: Json parse error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}

Player* Server::addPlayer(const std::string& name, const PlayerState state, const int socket) {
	Player* newPlayer = nullptr;
	try {
		newPlayer = new Player(name, state, socket);
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
		newPlayer = new Player(name, state, socket);
	} catch (std::exception& e) {
		std::cerr << "[Server]: Error adding temp player: " << e.what() << std::endl;
		return (nullptr);
	}
	newPlayer->setPlayerName(name);
	newPlayer->setPlayerState(state);
	newPlayer->setSocketFd(socket);

	std::lock_guard<std::mutex> lock(_tempPlayerLock);
	_tempPlayerLst[socket] = newPlayer;
	g_logger->logGameInfo(INFO, "Added temp player on socket " + std::to_string(socket), "Server");
	return (newPlayer);
}

void Server::removeTempPlayer(Player* player) {
	if (!player) {
		return;
	}
	int socket = player->getSocketFd();
	std::lock_guard<std::mutex> lock(_tempPlayerLock);
	_tempPlayerLst.erase(socket);
	delete player;
	g_logger->logGameInfo(INFO, "Removed temp player from socket " + std::to_string(socket), "Server");
}

void Server::promoteTempPlayer(Player* player) {
	if (!player)
		return;
	int socket = player->getSocketFd();

	std::lock_guard<std::mutex> lockTemp(_tempPlayerLock);
	_tempPlayerLst.erase(socket);

	std::lock_guard<std::mutex> lockPlayer(_playerLock);
	_playerLst[socket] = player;

	g_logger->logGameInfo(INFO, "Promoted temp player to main list on socket " + std::to_string(socket), "Server");
}

void Server::removePlayerFromAnyList(Player* player) {
	if (!player) {
		return;
	}
	int socket = player->getSocketFd();

	{
		std::lock_guard<std::mutex> lock(_tempPlayerLock);
		auto temp_it = _tempPlayerLst.find(socket);
		if (temp_it != _tempPlayerLst.end()) {
			_tempPlayerLst.erase(socket);
			delete player;
			g_logger->logGameInfo(INFO, "Removed temp player from socket " + std::to_string(socket), "Server");
			return;
		}
	}

	{
		std::lock_guard<std::mutex> lock(_playerLock);
		auto main_it = _playerLst.find(socket);
		if (main_it != _playerLst.end()) {
			_playerLst.erase(socket);
			delete player;
			g_logger->logGameInfo(INFO, "Removed main player from socket " + std::to_string(socket), "Server");
			return;
		}
	}
	delete player;
	g_logger->logGameInfo(INFO, "Deleted orphaned player from socket " + std::to_string(socket), "Server");
}

void Server::addPlayerToSample(const std::string& name) {
	_playerSample.push_back(name);
}

void Server::removePlayerToSample(const std::string& name) {
	for (size_t i = 0; i < _playerLst.size(); i++)
		if (_playerSample[i] == name) {
			_playerSample.erase(_playerSample.begin() + i);
			break;
		}
}

std::string Server::getGameVersion() {
	return _gameVersion;
}
std::string Server::getServerMOTD() {
	return _serverMOTD;
}
int Server::getProtocolVersion() {
	return _protocolVersion;
}
int Server::getServerSize() {
	return _serverSize;
}
int Server::getAmountOnline() {
	return _playerLst.size();
}
json Server::getPlayerSample() {
	return _playerSample;
}
