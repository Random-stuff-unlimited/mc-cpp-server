#include "config.hpp"
#include "Commands.hpp"
#include "lib/filesystem.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/inventory/Menu.hpp"
#include "world/PlayerDataStorage.hpp"
#include "world/World.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Combat.hpp"
#include "world/Survival.hpp"
#include "PacketIds.hpp"
#include "network/packet.hpp"
#include "network/TextComponent.hpp"
#include "network/packetRouter.hpp"
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

Server::Server() : _playerLst(), _config(), _networkManager(nullptr), _playerTracker(*this), _tickLoop(*this) {}

Server::~Server() {
	// No more packets first, then save the players and the world, then drop the players (they release their chunks)
	if (_networkManager) _networkManager->stopThreads();
	if (_world && _level) _level->saveEntities(); // Mobs go into their chunks first
	if (_world && _playerData) savePlayers(); // PlayerList.saveAll, before the I/O threads stop
	if (_world) _world->shutdown();
	_gamePlayers.clear();
	_level.reset();
	// Player destructors use _idManager, which is destroyed before the player maps
	clearPlayers();
	delete _networkManager;
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

void Server::sendSystemMessage(Player& player, const std::string& translationKey) {
	Buffer message;
	TextComponent::writeTranslatable(message, translationKey, {});
	message.writeBool(false); // In the chat, not above the hotbar
	Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SYSTEM_CHAT, message, *this);
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
	std::vector<uint8_t> frame;
	for (const auto& player : _gamePlayers) {
		if (player.get() == except || player->isDisconnected()) continue;
		ChunkStreamer* streamer = player->getChunkStreamer();
		if (!streamer || !streamer->hasChunk(chunkX, chunkZ)) continue;
		if (frame.empty()) frame = Packet::buildFrame(packetId, data.getData(), _config.getCompressionThreshold());
		Packet::sendFrame(player, frame, *this);
	}
}

void Server::broadcastToGame(int packetId, Buffer& data) {
	if (_gamePlayers.empty()) return;
	std::vector<uint8_t> frame = Packet::buildFrame(packetId, data.getData(), _config.getCompressionThreshold());
	for (const auto& player : _gamePlayers) Packet::sendFrame(player, frame, *this);
}

void Server::sendTickingState(const std::shared_ptr<Player>& to) {
	Buffer state;
	state.writeFloat(_tickLoop.getTickRate());
	state.writeBool(_tickLoop.isFrozen());
	Buffer step;
	step.writeVarInt(_tickLoop.getStepsLeft());
	if (to) {
		Packet::send(to, PacketId::Play::Clientbound::TICKING_STATE, state, *this);
		Packet::send(to, PacketId::Play::Clientbound::TICKING_STEP, step, *this);
	} else {
		broadcastToGame(PacketId::Play::Clientbound::TICKING_STATE, state);
		broadcastToGame(PacketId::Play::Clientbound::TICKING_STEP, step);
	}
}

void Server::sendTime(const std::shared_ptr<Player>& to) {
	if (!_world) return; // A server without its world (tests)
	Buffer time;
	time.writeLong(_world->getGameTime());
	time.writeLong(_world->getDayTime());
	time.writeBool(true); // The client advances the time of day by itself (daylight cycle)
	if (to) {
		Packet::send(to, PacketId::Play::Clientbound::SET_TIME, time, *this);
	} else {
		broadcastToGame(PacketId::Play::Clientbound::SET_TIME, time);
	}
}

// Sends a Keep Alive every 10 s to players in game, and drops those who haven't answered the previous one in 30 s.
// Real time, not ticks: it's about the connection, whatever the tick rate
void Server::tickKeepAlive() {
	int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	for (const auto& player : _gamePlayers) {
		if (player->isDisconnected()) continue;
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

// The phases of vanilla's ServerLevel.tick, in its order: this order is what redstone timings rely on
void Server::tick(bool worldRuns) {
	_level->setHandlingTick(true);
	if (worldRuns) {
		_level->updateSkyBrightness();
		_world->tickTime();
		_level->tickScheduled(); // Block ticks, then fluid ticks
		_level->tickChunks();	 // Random ticks
	}
	// The clients advance the time themselves: resynchronized every second, like vanilla
	if (_tickLoop.getTickCount() % 20 == 0) sendTime(nullptr);
	if (worldRuns) _level->runBlockEvents();
	_level->setHandlingTick(false);

	// Entities: players, then the others (items...), which stop while the game is frozen
	for (const auto& player : _gamePlayers) {
		if (!player->isDisconnected()) {
			Combat::tick(*this, *player);
			Survival::tick(*_level, *player);
		}
	}
	if (worldRuns) {
		_level->tickEntities();
		_level->tickBlockEntities(); // Moving pistons
	}
	// The movements of this tick to the players that see them
	_playerTracker.tick(_tickLoop.getTickCount());
	// Their entity data that changed (air, item in use)
	for (const auto& player : _gamePlayers) {
		if (!player->isDisconnected()) Survival::sendDirtyData(*this, *player);
	}
	// The block changes, then the actions they answer (Block Changed Ack), the entities and the inventories
	_level->sendChanges();
	_level->sendEntityChanges();
	// ServerPlayer.tick: the open menu's changes (the inventory's by default)
	for (const auto& player : _gamePlayers) {
		if (!player->isDisconnected()) Menus::tick(*player, *_level);
	}
	for (const auto& player : _gamePlayers) {
		int sequence = player->takeBlockChangesAck();
		if (sequence < 0) continue;
		Buffer ack;
		ack.writeVarInt(sequence);
		Packet::send(player, PacketId::Play::Clientbound::BLOCK_CHANGED_ACK, ack, *this);
	}
	tickKeepAlive();

	// Chunk unloading and autosave count in real time
	auto now = std::chrono::steady_clock::now();
	if (now - _lastWorldMaintenance >= std::chrono::seconds(1)) {
		_lastWorldMaintenance = now;
		_level->saveEntities(); // Before chunks are saved or unloaded
		if (_world->tick()) savePlayers(); // MinecraftServer.saveEverything: the players with the chunks
		_level->dropUnloadedChunks();
	}
}

void Server::runGameHandler(Packet* packet, void (*handler)(Packet*, Server&)) {
	std::unique_ptr<Packet> owned(packet);
	Player*					player = packet->getPlayer();
	// Packets still queued for a player that is being disconnected are dropped
	if (!player || player->isDisconnected()) return;
	try {
		handler(packet, *this);
		if (packet->getReturnPacket() == PACKET_DISCONNECT) _networkManager->requestDisconnect(player);
	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Error processing packet: " + std::string(e.what()), "SERVER");
		_networkManager->requestDisconnect(player);
	}
}

void Server::handleGamePacket(Packet* packet) { runGameHandler(packet, playPacketRouter); }

void Server::enterGame(Packet* packet) { runGameHandler(packet, enterPlay); }

void Server::addGamePlayer(const std::shared_ptr<Player>& player) {
	_gamePlayers.push_back(player);
	sendTickingState(player);
	sendTime(player);
}

// Also cleans up after an enterPlay that failed halfway
void Server::leaveGame(Player* player) {
	// PlayerList.remove: saved first, then the menus close. Only a player that entered the game: one whose
	// enterPlay failed may not have its data
	auto inGame = std::find_if(_gamePlayers.begin(), _gamePlayers.end(), [player](const auto& p) { return p.get() == player; });
	if (inGame != _gamePlayers.end()) savePlayer(*player);
	if (ChunkStreamer* streamer = player->getChunkStreamer()) streamer->stop();
	_playerTracker.leave(player);
	// Player.remove: the menus close, what the cursor and the crafting grid held falls on the ground
	if (player->openMenuSlot()) {
		player->openMenuSlot()->removed();
		player->openMenuSlot().reset();
	}
	if (player->inventoryMenuSlot()) player->inventoryMenuSlot()->removed();
	_level->entities().forgetPlayer(player);
	auto it = std::find_if(_gamePlayers.begin(), _gamePlayers.end(), [player](const auto& p) { return p.get() == player; });
	if (it == _gamePlayers.end()) return;
	*it = std::move(_gamePlayers.back());
	_gamePlayers.pop_back();
}

void Server::savePlayer(const Player& player) {
	_playerData->save(player.getUUID(), PlayerData::save(player, _gameData, _world->getDimensionName()));
}

void Server::savePlayers() {
	for (const auto& player : _gamePlayers) savePlayer(*player);
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

		try {
			_deathMessages.load(getPath().parent_path() / "death-messages");
		} catch (const std::exception& e) {
			g_logger->logGameInfo(WARN, "Death messages unavailable (" + std::string(e.what()) + "): \"<player> died\" instead", "SERVER");
		}

		Commands::registerCommands();

		World::Settings worldSettings;
		worldSettings.directory		   = getPath().parent_path() / _config.getWorldName();
		worldSettings.autosaveInterval = std::chrono::seconds(_config.getAutosaveInterval());
		worldSettings.ioThreads		   = std::clamp<size_t>(std::thread::hardware_concurrency() / 4, 2, 8);
		worldSettings.compressionThreshold = _config.getCompressionThreshold();
		try {
			_world = std::make_unique<World>(_gameData, worldSettings);
		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR, "Failed to load world: " + std::string(e.what()), "SERVER");
			return 1;
		}

		_level		= std::make_unique<Level>(*this, *_world, _gameData);
		_playerData = std::make_unique<PlayerDataStorage>(worldSettings.directory, [this](std::function<void()> job) { _world->submitSave(std::move(job)); });
		// Chunks finish loading on I/O threads: their scheduled ticks start counting on the game thread
		_world->setChunkLoadListener([this](const std::shared_ptr<Chunk>& chunk) { _tickLoop.post([this, chunk] { _level->onChunkLoaded(chunk); }); });

		_tickLoop.setTickRate(_config.getTickRate());

		size_t networkThreads = std::clamp<size_t>(std::thread::hardware_concurrency() / 4, 1, 4);
		_networkManager		  = new NetworkManager(networkThreads, *this);
		_networkManager->startThreads();

		struct sigaction sa = {};
		sa.sa_handler		= handleStopSignal;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, nullptr);
		sigaction(SIGTERM, &sa, nullptr);

		g_logger->logGameInfo(INFO, "Server started (" + std::to_string(static_cast<int>(_tickLoop.getTickRate())) + " TPS), press Ctrl+C to stop",
							  "SERVER");
		_lastWorldMaintenance = std::chrono::steady_clock::now();
		// This thread becomes the game thread
		_tickLoop.run([] { return g_stopRequested != 0; });
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
