#ifndef SERVER_HPP
#define SERVER_HPP

class Level;
class NetworkManager;
class Packet;
class PlayerDataStorage;
class World;
#include "../TickLoop.hpp"
#include "../world/PlayerTracker.hpp"
#include "../config.hpp"
#include "../data/DeathMessages.hpp"
#include "../data/GameData.hpp"
#include "../player.hpp"
#include "../world/World.hpp"
#include "id_manager.hpp"
#include "lib/json.hpp"

#include <chrono>
#include <filesystem>
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
	// The dimensions: overworld, nether, end (MinecraftServer.levels), each its own World and Level. The overworld
	// comes first: it ticks first and owns the clock and the weather
	std::vector<std::unique_ptr<World>> _worlds;
	std::vector<std::unique_ptr<Level>> _levels;
	World*							 _world = nullptr; // The overworld
	Level*							 _level = nullptr;
	std::unique_ptr<PlayerDataStorage> _playerData;
	PlayerTracker					 _playerTracker;
	TickLoop						 _tickLoop;

	// Players in the Play state, game thread only
	std::vector<std::shared_ptr<Player>>  _gamePlayers;
	std::chrono::steady_clock::time_point _lastWorldMaintenance;

	// One level's part of the tick (ServerLevel.tick): weather, sleeping, time, scheduled ticks, random ticks, block
	// events, entities, block entities
	void tickLevel(Level& level, bool worldRuns);

	void tickKeepAlive();
	void runGameHandler(Packet* packet, void (*handler)(Packet*, Server&));

  public:
	// Creates a dimension's World and Level (World directory: the overworld's, DIM-1 for the nether, DIM1 for the end).
	// The overworld first. start_server loads the three; tests load them with loadGameData
	void loadDimension(const World::Settings& overworldSettings, const std::string& dimension);
	void loadGameData(const std::filesystem::path& directory) { _gameData.load(directory); }
	// Disconnects a player with a message (a translation key of the game, e.g. "multiplayer.disconnect.kicked")
	void kick(Player* player, const std::string& translationKey);
	// A translated message in the player's own chat (System Chat, in the chat): its language's wording
	void sendSystemMessage(Player& player, const std::string& translationKey);
	// The same above the hotbar (Player.displayClientMessage(..., true))
	void sendActionBar(Player& player, const std::string& translationKey);
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
	// The overworld
	World&			getWorld() { return *_world; }
	// The overworld for the game logic (game thread)
	Level&			getLevel() { return *_level; }
	// The dimension a player is in (the overworld before it entered the game)
	Level&			levelOf(const Player& player) { return player.level() ? *player.level() : *_level; }
	// A dimension by name ("minecraft:the_nether"), nullptr if the server doesn't have it
	Level*			getLevel(const std::string& dimension);
	const std::vector<std::unique_ptr<Level>>& getLevels() const { return _levels; }
	PlayerTracker&	getPlayerTracker() { return _playerTracker; }
	// <world>/playerdata
	PlayerDataStorage& getPlayerData() { return *_playerData; }
	TickLoop&		getTickLoop() { return _tickLoop; }

	// ----- Game thread only -----

	// One tick of the game. worldRuns is false while the game is frozen (/tick freeze): players still tick
	void tick(bool worldRuns);
	// The sleeping players of a level: their timers, and the night skipped once everyone sleeps long enough
	// (ServerLevel.tick's SleepStatus)
	void tickSleeping(Level& level);
	// A sleeping player wakes up: the bed is free, the pose and the wake animation are sent
	void wakeUp(Player& player);
	// PlayerList.sendLevelInfo: the world border, the time, the default spawn position, the rain, and the "waiting for
	// chunks" event, for a player entering a level
	void sendLevelInfo(const std::shared_ptr<Player>& player, Level& level);
	// The world spawn (the overworld's), to one player or to everyone when null (Set Default Spawn Position)
	void sendDefaultSpawn(const std::shared_ptr<Player>& to);
	// ServerPlayer.teleport(TeleportTransition) to another dimension: the player leaves its level, gets a Respawn
	// packet (keeping its attributes and entity data), the new level's info, its position, and the chunks and
	// entities of the new level. keepAllData: what Respawn keeps (1 attributes, 2 entity data)
	void changeDimension(Player& player, Level& destination, double x, double y, double z, float yaw, float pitch, uint8_t keptData = 3);
	// ServerPlayer.showEndCredits: the player leaves the end (its level) and watches the credits; it respawns in the
	// overworld, keeping everything, when its client closes them (CLIENT_COMMAND)
	void showEndCredits(Player& player);
	// A Play packet posted by a network thread. Deletes it
	void handleGamePacket(Packet* packet);
	// Finish Configuration acknowledged: the player enters the world. Deletes the packet
	void enterGame(Packet* packet);
	// Called by enterPlay once the player is in the world
	void addGamePlayer(const std::shared_ptr<Player>& player);
	// After a disconnection: releases the player's chunks and removes it from the world
	void leaveGame(Player* player);
	// PlayerList.save / saveAll: written on the I/O threads
	void savePlayer(const Player& player);
	void savePlayers();
	const std::vector<std::shared_ptr<Player>>& getGamePlayers() const { return _gamePlayers; }

	// Sends a packet to every player of the overworld that has this chunk, except `except` (Level::broadcastToChunk
	// for the other dimensions)
	void broadcastToChunk(int chunkX, int chunkZ, int packetId, Buffer& data, const Player* except = nullptr);
	// Sends a packet to every player in game (encoded once)
	void broadcastToGame(int packetId, Buffer& data);
	// Tick rate and frozen state (Ticking State + Tick Step), to one player or to everyone when null
	void sendTickingState(const std::shared_ptr<Player>& to);
	// Game time and time of day (Set Time), to one player or to everyone when null
	void sendTime(const std::shared_ptr<Player>& to);
};

#endif
