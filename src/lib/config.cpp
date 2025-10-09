#include "config.hpp"
#include "lib/filesystem.hpp"
#include "lib/json.hpp"
#include "logger.hpp"

#include <fstream>
#include <string>

using json = nlohmann::json;

Config::Config() { _execPath = getPath(); }

bool Config::loadConfig() {
	std::ifstream inputFile(_execPath.root_directory() /
							"config.json"); // Should change the config path later if needed

	if (!inputFile.is_open()) {
		g_logger->logGameInfo(DEBUG, "Error: Could not open: config.json", "SERVER");
		return true;
	}

	json config;

	try {
		inputFile >> config;
		Config::setServerVersion(config["version"]["name"]);
		Config::setProtocolVersion(config["version"]["protocol"]);
		Config::setServerSize(config["server"]["max-players"]);
		Config::setServerMotd(config["server"]["motd"]);
		Config::setServerAddress(config["server"]["ip-address"]);
		Config::setServerPort(config["server"]["port"]);
	} catch (json::parse_error& e) {
		g_logger->logGameInfo(
				ERROR, "Error parsing config.json: " + std::string(e.what()), "SERVER");
		inputFile.close();
		return true;
	}
	inputFile.close();
	return false;
}

bool Config::reloadConfig() {
	std::ifstream inputFile(_execPath.root_directory() /
							"config.json"); // Should change the config path later if needed

	if (!inputFile.is_open()) {
		g_logger->logGameInfo(DEBUG, "Error: Could not open: config.json", "SERVER");
		return true;
	}

	json config;

	try {
		Config::setServerSize(config["server"]["max-players"]);
		Config::setServerMotd(config["server"]["motd"]);
	} catch (json::parse_error& e) {
		g_logger->logGameInfo(
				ERROR, "Error parsing config.json: " + std::string(e.what()), "SERVER");
		inputFile.close();
		return true;
	}
	return false;
}

Config::~Config() {}

// Getter methods
int Config::getServerPort() { return _serverPort; }

int Config::getProtocolVersion() { return _protocolVersion; }

int Config::getServerSize() { return _serverSize; }

std::string Config::getVersion() { return _gameVersion; }

std::string Config::getServerMotd() { return _serverMotd; }

std::string Config::getServerAddress() { return _serverAddress; }

// Setter methods
void Config::setProtocolVersion(int ProtoVersion) { _protocolVersion = ProtoVersion; }

void Config::setServerSize(int ServerSize) { _serverSize = ServerSize; }

void Config::setServerPort(int ServerPort) { _serverPort = ServerPort; }

void Config::setServerMotd(std::string ServerMotd) { _serverMotd = ServerMotd; }

void Config::setServerVersion(std::string ServerVersion) { _gameVersion = ServerVersion; }

void Config::setServerAddress(std::string ServerAddress) { _serverAddress = ServerAddress; }
