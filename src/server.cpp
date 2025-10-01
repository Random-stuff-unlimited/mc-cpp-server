#include "server.hpp"
#include "include/json.hpp"
#include <sys/socket.h>
#include <iostream>
#include <fstream>

using json = nlohmann::json;

Server::Server() {}
Server::Server(const Server &src) {}
Server &Server::operator=(const Server &src) {}
Server::~Server() {}

int		Server::getServerPort() {return (this->_port);}
void	Server::setServerPort(int port) {this->_port = port;}

int Server::start_server(int port)
{

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
		Server::_gameVersion = j["version"]["name"];
		Server::_protocolVersion = j["version"]["protocol"];
		Server::_serverSize = j["maxPlayer"];
		Server::_serverMOTD = j["serverMotd"];

	} catch (json::parse_error& e) {
		std::cerr << "[Server]: Json parse error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}

std::string Server::getGameVersion() {return _gameVersion;}
std::string Server::getServerMOTD() {return _serverMOTD;}
int Server::getProtocolVersion() {return _protocolVersion;}
int Server::getServerSize() {return _serverSize;}
int Server::getAmountOnline() {return 69;}