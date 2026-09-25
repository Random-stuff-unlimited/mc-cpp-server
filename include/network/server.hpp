#ifndef SERVER_HPP
#define SERVER_HPP

class NetworkManager;
class Packet;
class World;
#include "../TickLoop.hpp"
#include "../world/PlayerTracker.hpp"
#include "../config.hpp"
#include "../data/DeathMessages.hpp"
#include "../data/GameData.hpp"
#include "../player.hpp"
#include "id_manager.hpp"
#include "lib/json.hpp"

#include <chrono>
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
	DeathMessages					 _deathMessages;
	NetworkManager*					 _networkManager;
	IdManager						 _idManager;
	std::unique_ptr<World>			 _world;
	PlayerTracker					 _playerTracker;
	TickLoop						 _tickLoop;

	// Players in the Play state, game thread only
	std::vector<std::shared_ptr<Player>>  _gamePlayers;
	std::chrono::steady_clock::time_point _lastWorldMaintenance;

	void tickKeepAlive();
	void runGameHandler(Packet* packet, void (*handler)(Packet*, Server&));

  public:
	// Disconnects a player with a message (a translation key of the game, e.g. "multiplayer.disconnect.kicked")
	void kick(Player* player, const std::string& translationKey);
	// Logged-in players (login, configuration or play) with this name, ignoring case
	std::vector<std::shared_ptr<Player>> findPlayersByName(const std::string& name);

  private:

  public:
	Server();
	~Server();

	int start_server();

	int		getAmountOnline();
	Config&			getConfig() { return _config; }
	const GameData& getGameData() const { return _gameData; }
	const DeathMessages& getDeathMessages() const { return _deathMessages; }

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
	PlayerTracker&	getPlayerTracker() { return _playerTracker; }
	TickLoop&		getTickLoop() { return _tickLoop; }

	// ----- Game thread only -----

	// One tick of the game. worldRuns is false while the game is frozen (/tick freeze): players still tick
	void tick(bool worldRuns);
	// A Play packet posted by a network thread. Deletes it
	void handleGamePacket(Packet* packet);
	// Finish Configuration acknowledged: the player enters the world. Deletes the packet
	void enterGame(Packet* packet);
	// Called by enterPlay once the player is in the world
	void addGamePlayer(const std::shared_ptr<Player>& player);
	// After a disconnection: releases the player's chunks and removes it from the world
	void leaveGame(Player* player);
	const std::vector<std::shared_ptr<Player>>& getGamePlayers() const { return _gamePlayers; }

	// Sends a packet to every player that has this chunk, except `except`
	void broadcastToChunk(int chunkX, int chunkZ, int packetId, Buffer& data, const Player* except = nullptr);
	// Sends a packet to every player in game (encoded once)
	void broadcastToGame(int packetId, Buffer& data);
	// Tick rate and frozen state (Ticking State + Tick Step), to one player or to everyone when null
	void sendTickingState(const std::shared_ptr<Player>& to);
	// Game time and time of day (Set Time), to one player or to everyone when null
	void sendTime(const std::shared_ptr<Player>& to);
};

#endif
