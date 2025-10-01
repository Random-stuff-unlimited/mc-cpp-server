#include "server.hpp"
#include "json.hpp"
#include "network.hpp"
#include <sys/socket.h>
#include <iostream>
#include <fstream>

using json = nlohmann::json;

Server::Server() : _player_lst(nullptr) {}
Server::~Server() {}

int Server::start_server(int port) {
    try {
        Network net(port, *this); // création directe
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "stop server try\n";
        net.stopThreads();
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
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