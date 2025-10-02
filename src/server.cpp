#include "server.hpp"
#include "json.hpp"
#include "networking.hpp"
#include "player.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <exception>

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

		T& self = *this;
		// Create NetworkManager with BOTH required parameters
		_networkManager = new NetworkManager(workerCount, self);
		_networkManager->startThreads();

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
		_protocolVersion = j["version"]["protocol"];
		_serverSize = j["server"]["max-players"];
		_serverMOTD = j["motd"];
		std::string temp = j["server"]["ip-address"];
		_serverAddr = (char*)temp.c_str();
		_serverPort = j["server"]["port"];

	} catch (json::parse_error& e) {
		std::cerr << "[Server]: Json parse error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}

void Server::addPlayer(Player *player) {
	_playerLst.push_back(*player);
}

void Server::removePlayer(Player *player) {
    auto it = std::remove_if(_playerLst.begin(), _playerLst.end(),
        [player](const Player &p) {
            return &p == player;
        });

    if (it != _playerLst.end()) {
        _playerLst.erase(it, _playerLst.end());
    }
}

Player& Server::getLastPlayer() {
    return _playerLst.back();
}

void	Server::addPlayerToSample(const std::string &name)
{
	_playerSample.push_back(name);
}

void	Server::removePlayerToSample(const std::string &name)
{
	for (size_t i = 0; i < _playerLst.size(); i++)
		if(_playerLst[i].getPlayerName() == name)
		{
			_playerLst.erase(_playerLst.begin() + i);
			break ;
		}
}

std::string	Server::getGameVersion() {return _gameVersion;}
std::string	Server::getServerMOTD() {return _serverMOTD;}
int	Server::getProtocolVersion() {return _protocolVersion;}
int	Server::getServerSize() {return _serverSize;}
int	Server::getAmountOnline() {return 69;}
json	Server::getPlayerSample() {return _playerSample;}
