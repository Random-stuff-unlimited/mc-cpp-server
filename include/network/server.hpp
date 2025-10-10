#ifndef SERVER_HPP
#define SERVER_HPP

class NetworkManager;
#include "../config.hpp"
#include "../player.hpp"
#include "../world/worldManager.hpp"
#include "id_manager.hpp"
#include "lib/json.hpp"

#include <mutex>
#include <netinet/in.h>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

class Server {
  private:
	std::unordered_map<int, Player*> _playerLst;
	std::unordered_map<int, Player*> _tempPlayerLst;
	json							 _playerSample;
	std::mutex						 _playerLock;
	std::mutex						 _tempPlayerLock;
	Config							 _config;
	NetworkManager*					 _networkManager;
	IdManager						 _idManager;
	WorldManager					 _worldManager;
	Data							 _worldData;

  public:
	Server();
	~Server();

	int start_server();

	int								  getAmountOnline();
	Config&							  getConfig() { return _config; }
	std::unordered_map<int, Player*>& getPlayerLst() { return _playerLst; }
	std::unordered_map<int, Player*>& getTempPlayerLst() { return _tempPlayerLst; }

	void	   addPlayerToSample(const std::string& name);
	void	   removePlayerToSample(const std::string& name);
	Player*	   addPlayer(const std::string& name, const PlayerState state, const int socket);
	void	   removePlayer(Player* player);
	Player*	   addTempPlayer(const std::string& name, const PlayerState state, const int socket);
	void	   removeTempPlayer(Player* player);
	void	   promoteTempPlayer(Player* player);
	void	   removePlayerFromAnyList(Player* player);
	json	   getPlayerSample();
	IdManager& getIdManager() { return (_idManager); }

	NetworkManager& getNetworkManager() { return *_networkManager; }
	WorldManager&	getWorldManager() { return _worldManager; }
	Data&			getWorldData() { return _worldData; }
};

#endif
