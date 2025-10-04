#ifndef SERVER_HPP
#define SERVER_HPP

class NetworkManager;
#include "id_manager.hpp"
#include "json.hpp"
#include "player.hpp"

#include <mutex>
#include <netinet/in.h>
#include <string>
#include <unordered_map>
#define ConfigFileName "config.json"

using json = nlohmann::json;

class Server {
  private:
	std::unordered_map<int, Player*> _playerLst;
	std::unordered_map<int, Player*> _tempPlayerLst;
	json _playerSample;
	std::mutex _playerLock;
	std::mutex _tempPlayerLock;
	int _protocolVersion;
	int _serverSize;
	int loadConfig();
	std::string _gameVersion;
	std::string _serverMOTD;
	int _serverPort;
	std::string _serverAddr;
	NetworkManager* _networkManager;
	IdManager _idManager;

  public:
	Server();
	~Server();

	int start_server(int port);

	int getProtocolVersion();
	int getServerSize();
	int getAmountOnline();
	std::string getGameVersion();
	std::string getServerMOTD();
	int getServerPort() {
		return _serverPort;
	}
	std::string getServerAddr() {
		return _serverAddr;
	}
	std::unordered_map<int, Player*>& getPlayerLst() {
		return _playerLst;
	}
	std::unordered_map<int, Player*>& getTempPlayerLst() {
		return _tempPlayerLst;
	}

	void addPlayerToSample(const std::string& name);
	void removePlayerToSample(const std::string& name);
	Player* addPlayer(const std::string& name, const PlayerState state, const int socket);
	void removePlayer(Player* player);
	Player* addTempPlayer(const std::string& name, const PlayerState state, const int socket);
	void removeTempPlayer(Player* player);
	void promoteTempPlayer(Player* player);
	void removePlayerFromAnyList(Player* player);
	json getPlayerSample();
	IdManager& getIdManager() {
		return (_idManager);
	}

	NetworkManager& getNetworkManager() {
		return *_networkManager;
	}
};

#endif
