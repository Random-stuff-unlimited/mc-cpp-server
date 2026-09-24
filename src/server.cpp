#include "config.hpp"
#include "lib/filesystem.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/World.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Combat.hpp"
#include "PacketIds.hpp"
#include "network/packet.hpp"
#include "network/TextComponent.hpp"
#include <algorithm>

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;

static volatile std::sig_atomic_t g_stopRequested = 0;

static void handleStopSignal(int) { g_stopRequested = 1; }

Server::Server() : _playerLst(), _config(), _networkManager(nullptr), _playerTracker(*this) {}

Server::~Server() {
	// No more packets first, then save the world, then drop the players (they release their chunks)
	if (_networkManager) _networkManager->stopThreads();
	if (_world) _world->shutdown();
	// Player destructors use _idManager, which is destroyed before the player maps
	clearPlayers();
	delete _networkManager;
}

std::vector<std::shared_ptr<Player>> Server::playersInGame() {
	std::vector<std::shared_ptr<Player>> players;
	std::lock_guard<std::mutex>			 lock(_playerLock);
	for (const auto& [socket, player] : _playerLst) {
		if (player->getPlayerState() == PlayerState::Play && !player->isDisconnected()) players.push_back(player);
	}
	return players;
}

void Server::kick(Player* player, const std::string& translationKey) {
	if (!player || player->isDisconnected()) return;
	std::shared_ptr<Player> target = player->shared_from_this();
	Buffer					reason;
	switch (player->getPlayerState()) {
	case PlayerState::Login:
		// The login state still uses JSON text
		reason.writeString("{\"translate\":\"" + translationKey + "\"}");
		Packet::send(target, PacketId::Login::Clientbound::LOGIN_DISCONNECT, reason, *this);
		break;
	case PlayerState::Configuration:
		TextComponent::writeTranslatable(reason, translationKey, {});
		Packet::send(target, PacketId::Configuration::Clientbound::DISCONNECT, reason, *this);
		break;
	case PlayerState::Play:
		TextComponent::writeTranslatable(reason, translationKey, {});
		Packet::send(target, PacketId::Play::Clientbound::DISCONNECT, reason, *this);
		break;
	default:
		break;
	}
	g_logger->logNetwork(INFO, player->getPlayerName() + " kicked: " + translationKey, "SERVER");
	_networkManager->requestDisconnect(player); // After the message is sent
}

std::vector<std::shared_ptr<Player>> Server::findPlayersByName(const std::string& name) {
	auto lower = [](std::string s) {
		for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	};
	std::string							 wanted = lower(name);
	std::vector<std::shared_ptr<Player>> found;
	std::lock_guard<std::mutex>			 lock(_playerLock);
	for (const auto& [socket, player] : _playerLst) {
		if (!player->isDisconnected() && lower(player->getPlayerName()) == wanted) found.push_back(player);
	}
	return found;
}

void Server::broadcastToChunk(int chunkX, int chunkZ, int packetId, Buffer& data, const Player* except) {
	for (const auto& player : playersInGame()) {
		if (player.get() == except) continue;
		ChunkStreamer* streamer = player->getChunkStreamer();
		if (streamer && streamer->hasChunk(chunkX, chunkZ)) Packet::send(player, packetId, data, *this);
	}
}

// Sends a Keep Alive every 10 s to players in game, and drops those who haven't answered the previous one in 30 s
void Server::tickKeepAlive() {
	std::vector<std::shared_ptr<Player>> players = playersInGame();

	int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	for (const auto& player : players) {
		int64_t sinceLast = now - player->getKeepAliveSentAt();
		if (player->getKeepAlivePending() != 0) {
			if (sinceLast > 30000) {
				g_logger->logNetwork(INFO, player->getPlayerName() + " timed out", "SERVER");
				_networkManager->requestDisconnect(player.get());
			}
			continue;
		}
		if (sinceLast < 10000) continue;
		player->setKeepAliveSent(now, now);
		Buffer keepAlive;
		keepAlive.writeLong(now);
		Packet::send(player, PacketId::Play::Clientbound::KEEP_ALIVE, keepAlive, *this);
	}
}

int Server::start_server() {
	try {
		initializeGlobalLogger();
		if (_config.loadConfig()) {
			g_logger->logGameInfo(ERROR, "Failed to load config", "SERVER");
			return 1;
		}

		try {
			_gameData.load(getPath().parent_path() / "gamedata");
		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR, "Failed to load game data: " + std::string(e.what()), "SERVER");
			return 1;
		}
		g_logger->logGameInfo(INFO,
							  "Game data loaded: Minecraft " + _gameData.getVersionName() + " (protocol " +
									  std::to_string(_gameData.getProtocolVersion()) + ")",
							  "SERVER");

		World::Settings worldSettings;
		worldSettings.directory		   = getPath().parent_path() / _config.getWorldName();
		worldSettings.autosaveInterval = std::chrono::seconds(_config.getAutosaveInterval());
		worldSettings.ioThreads		   = std::clamp<size_t>(std::thread::hardware_concurrency() / 4, 2, 8);
		try {
			_world = std::make_unique<World>(_gameData, worldSettings);
		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR, "Failed to load world: " + std::string(e.what()), "SERVER");
			return 1;
		}

		size_t workerCount = 4;
		if (workerCount == 0) workerCount = 4; // fallback

		// Create NetworkManager with BOTH required parameters
		_networkManager = new NetworkManager(workerCount, *this);
		_networkManager->startThreads();

		struct sigaction sa = {};
		sa.sa_handler		= handleStopSignal;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, nullptr);
		sigaction(SIGTERM, &sa, nullptr);

		g_logger->logGameInfo(INFO, "Server started, press Ctrl+C to stop", "SERVER");
		auto lastTick = std::chrono::steady_clock::now();
		while (!g_stopRequested) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			auto now = std::chrono::steady_clock::now();
			if (now - lastTick < std::chrono::seconds(1)) continue;
			lastTick = now;
			_world->tick();
			tickKeepAlive();
			for (const auto& player : playersInGame()) Combat::tick(*this, *player);
		}
		g_logger->logGameInfo(INFO, "Stopping server...", "SERVER");
	} catch (const std::exception& e) {
		std::cerr << "[Server] Fatal error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}

std::shared_ptr<Player> Server::addTempPlayer(const std::string& name, const PlayerState state, const int socket) {
	std::shared_ptr<Player> newPlayer = std::make_shared<Player>(name, state, socket, *this);

	std::lock_guard<std::mutex> lock(_tempPlayerLock);
	_tempPlayerLst[socket] = newPlayer;
	return (newPlayer);
}

std::shared_ptr<Player> Server::findPlayer(int socket) {
	{
		std::lock_guard<std::mutex> lock(_playerLock);
		auto						it = _playerLst.find(socket);
		if (it != _playerLst.end()) return it->second;
	}
	std::lock_guard<std::mutex> lock(_tempPlayerLock);
	auto						it = _tempPlayerLst.find(socket);
	if (it != _tempPlayerLst.end()) return it->second;
	return nullptr;
}

void Server::promoteTempPlayer(Player* player) {
	if (!player) return;
	int socket = player->getSocketFd();

	std::shared_ptr<Player> owned;
	{
		std::lock_guard<std::mutex> lock(_tempPlayerLock);
		auto						it = _tempPlayerLst.find(socket);
		if (it == _tempPlayerLst.end() || it->second.get() != player) return;
		owned = it->second;
		_tempPlayerLst.erase(it);
	}
	std::lock_guard<std::mutex> lock(_playerLock);
	_playerLst[socket] = owned;
}

// Only erases the entry if it still points to this player: the socket number may already belong to a new connection
void Server::removePlayerFromAnyList(Player* player) {
	if (!player) return;
	int socket = player->getSocketFd();

	{
		std::lock_guard<std::mutex> lock(_tempPlayerLock);
		auto						it = _tempPlayerLst.find(socket);
		if (it != _tempPlayerLst.end() && it->second.get() == player) _tempPlayerLst.erase(it);
	}
	std::lock_guard<std::mutex> lock(_playerLock);
	auto						it = _playerLst.find(socket);
	if (it != _playerLst.end() && it->second.get() == player) _playerLst.erase(it);
}

void Server::clearPlayers() {
	std::lock_guard<std::mutex> lockTemp(_tempPlayerLock);
	std::lock_guard<std::mutex> lockPlayer(_playerLock);
	_tempPlayerLst.clear();
	_playerLst.clear();
}

void Server::addPlayerToSample(const std::string& name) { _playerSample.push_back(name); }

void Server::removePlayerToSample(const std::string& name) {
	for (size_t i = 0; i < _playerLst.size(); i++)
		if (_playerSample[i] == name) {
			_playerSample.erase(_playerSample.begin() + i);
			break;
		}
}

int Server::getAmountOnline() {
	std::lock_guard<std::mutex> lock(_playerLock);
	return _playerLst.size();
}
json Server::getPlayerSample() { return _playerSample; }
