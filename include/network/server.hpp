#ifndef SERVER_HPP
#define SERVER_HPP

class NetworkManager;
class World;
#include "../config.hpp"
#include "../data/GameData.hpp"
#include "../player.hpp"
#include "id_manager.hpp"
#include "lib/json.hpp"

#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <unordered_map>
#include <vector>

class Buffer;

using json = nlohmann::json;

class Server {
  private:
	std::unordered_map<int, std::shared_ptr<Player>> _playerLst;
	std::unordered_map<int, std::shared_ptr<Player>> _tempPlayerLst;
	json											 _playerSample;
	std::mutex						 _playerLock;
	std::mutex						 _tempPlayerLock;
	Config							 _config;
	GameData						 _gameData;
	NetworkManager*					 _networkManager;
	IdManager						 _idManager;
	std::unique_ptr<World>			 _world;

	void tickKeepAlive();
	std::vector<std::shared_ptr<Player>> playersInGame();

  public:
	Server();
	~Server();

	int start_server();

	int		getAmountOnline();
	Config&			getConfig() { return _config; }
	const GameData& getGameData() const { return _gameData; }

	void					addPlayerToSample(const std::string& name);
	void					removePlayerToSample(const std::string& name);
	std::shared_ptr<Player> addTempPlayer(const std::string& name, const PlayerState state, const int socket);
	std::shared_ptr<Player> findPlayer(int socket);
	void					promoteTempPlayer(Player* player);
	void					removePlayerFromAnyList(Player* player);
	void					clearPlayers();
	json	   getPlayerSample();
	IdManager& getIdManager() { return (_idManager); }

	NetworkManager& getNetworkManager() { return *_networkManager; }
	World&			getWorld() { return *_world; }

	// Sends a packet to every player that has this chunk, except `except`
	void broadcastToChunk(int chunkX, int chunkZ, int packetId, Buffer& data, const Player* except = nullptr);
};

#endif
