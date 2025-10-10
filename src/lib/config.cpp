#include "config.hpp"
#include "lib/filesystem.hpp"
#include "lib/json.hpp"
#include "logger.hpp"

#include <fstream>
#include <string>

using json = nlohmann::json;

Config::Config()
	: _execPath(getPath()), _gameVersion("1.21.5"), _protocolVersion(770),
	  _serverMotd("A Minecraft Server"), _serverAddress("127.0.0.1"), _serverPort(25565),
	  _serverSize(20), _worldName("world"), _gamemode("survival"), _difficulty("normal") {}

bool Config::loadConfig() {
	std::ifstream inputFile(_execPath.parent_path() /
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
		Config::setWorldName(config["world"]["name"]);
		Config::setGamemode(config["world"]["gamemode"]);
		Config::setDifficulty(config["world"]["difficulty"]);
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
		inputFile >> config;
		// Reload server settings that can be changed at runtime
		Config::setServerSize(config["server"]["max-players"]);
		Config::setServerMotd(config["server"]["motd"]);

		// Reload world settings that can be changed at runtime
		Config::setGamemode(config["world"]["gamemode"]);
		Config::setDifficulty(config["world"]["difficulty"]);
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

std::string Config::getWorldName() { return _worldName; }

std::string Config::getGamemode() { return _gamemode; }

std::string Config::getDifficulty() { return _difficulty; }

// Setter methods
void Config::setProtocolVersion(int ProtoVersion) { _protocolVersion = ProtoVersion; }

void Config::setServerSize(int ServerSize) { _serverSize = ServerSize; }

void Config::setServerPort(int ServerPort) { _serverPort = ServerPort; }

void Config::setServerMotd(std::string ServerMotd) { _serverMotd = ServerMotd; }

void Config::setServerVersion(std::string ServerVersion) { _gameVersion = ServerVersion; }

void Config::setServerAddress(std::string ServerAddress) { _serverAddress = ServerAddress; }

void Config::setWorldName(std::string WorldName) { _worldName = WorldName; }

void Config::setGamemode(std::string Gamemode) { _gamemode = Gamemode; }

void Config::setDifficulty(std::string Difficulty) { _difficulty = Difficulty; }
