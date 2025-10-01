#include "server.hpp"
#include "json.hpp"
#include "enums.hpp"
#include "network.hpp"
#include "player.hpp"
#include <sys/socket.h>
#include <iostream>
#include <fstream>

using json = nlohmann::json;

Server::Server() : _playerLst(), _gameVersion("1.12.5"), _protocolVersion(770), _serverSize(-100000000), _serverMOTD() {}

Server::~Server() {}

int Server::start_server(int port) {
	try {
		Server::loadConfig();
		Network net(port, *this);
		std::this_thread::sleep_for(std::chrono::seconds(20));
		std::cout << "stop server try\n";
		net.stopThreads();
	} catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
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

		std::cout << "[Server]: Successfully parsed " << ConfigFileName << "!" << std::endl;
		_gameVersion = j["version"]["name"];
		_protocolVersion = j["version"]["protocol"];
		_serverSize = j["maxPlayer"];
		_serverMOTD = j["serverMotd"];

	} catch (json::parse_error& e) {
		std::cerr << "[Server]: Json parse error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
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