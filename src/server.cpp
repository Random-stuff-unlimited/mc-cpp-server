#include "server.hpp"
#include "json.hpp"
#include "networking.hpp"
#include "player.hpp"
#include "enums.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <exception>
#include <cstddef>

using json = nlohmann::json;

Server::Server() : _playerLst(), _protocolVersion(770), _serverSize(-100000000), _gameVersion("1.12.5"), _serverMOTD(), _networkManager(nullptr) {}

Server::~Server() {
   	if (_networkManager) {
		_networkManager->stopThreads();
		delete _networkManager;
	}
}

int Server::start_server(int port) {
	try {
		Server::loadConfig();
		size_t workerCount = 4;
		if (workerCount == 0) workerCount = 4; // fallback

		// Create NetworkManager with BOTH required parameters
		_networkManager = new NetworkManager(workerCount, *this);
		_networkManager->startThreads();

		while (true) {
			std::cout << "Server is running..." << std::endl;
			sleep(10);
			break ;
		}

	} catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
		return (1);
	}
	(void)port;
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

		std::cout << "[Server]: Successfully parsed " << ConfigFileName << "!" << std::endl;
		_gameVersion = j["version"]["name"];
		std::cout << "[Server]: Game version: " << _gameVersion << std::endl;
		_protocolVersion = j["version"]["protocol"];
		std::cout << "[Server]: Protocol version: " << _protocolVersion << std::endl;
		_serverSize = j["server"]["max-players"];
		std::cout << "[Server]: Server size: " << _serverSize << std::endl;
		_serverMOTD = j["server"]["motd"];
		std::cout << "[Server]: Server MOTD: " << _serverMOTD << std::endl;
		std::string temp = j["server"]["ip-address"];
		std::cout << "[Server]: Server IP address before conversion: " << temp << std::endl;
		_serverAddr = const_cast<char*>(temp.c_str());
		std::cout << "[Server]: Server IP address after conversion: " << temp << std::endl;
		_serverPort = j["server"]["port"];
		std::cout << "[Server]: Server port: " << _serverPort << std::endl;


	} catch (json::parse_error& e) {
		std::cerr << "[Server]: Json parse error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}

Player* Server::addPlayer(const std::string &name, const PlayerState state, const int socket) {
	Player* newPlayer = nullptr;
	try
	{
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

void Server::removePlayer(Player *player) {
    if (!player) {
        return;
    }
    int socket = player->getSocketFd();
    delete player;
    _playerLst.erase(socket);
}

void	Server::addPlayerToSample(const std::string &name)
{
	_playerSample.push_back(name);
}

void	Server::removePlayerToSample(const std::string &name)
{
	for (size_t i = 0; i < _playerLst.size(); i++)
		if(_playerSample[i] == name)
		{
			_playerSample.erase(_playerSample.begin() + i);
			break ;
		}
}

std::string	Server::getGameVersion() {return _gameVersion;}
std::string	Server::getServerMOTD() {return _serverMOTD;}
int			Server::getProtocolVersion() {return _protocolVersion;}
int			Server::getServerSize() {return _serverSize;}
int			Server::getAmountOnline() {return _playerLst.size();}
json		Server::getPlayerSample() {return _playerSample;}
