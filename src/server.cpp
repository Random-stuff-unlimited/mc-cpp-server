#include "config.hpp"
#include "Commands.hpp"
#include "lib/filesystem.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Portals.hpp"
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

namespace {
	// ClientboundGameEventPacket.Type ids for the weather
	constexpr int GAME_EVENT_START_RAIN	  = 1;
	constexpr int GAME_EVENT_STOP_RAIN	  = 2;
	constexpr int GAME_EVENT_RAIN_LEVEL	  = 7;
	constexpr int GAME_EVENT_THUNDER_LEVEL = 8;
	constexpr int GAME_EVENT_WAIT_CHUNKS	  = 13;
	constexpr int GAME_EVENT_WIN_GAME		  = 4;
	// ClientboundAnimatePacket ids (ClientAnimatePacket)
	constexpr int ANIMATE_WAKE_UP = 2;
} // namespace

Server::Server() : _playerLst(), _config(), _networkManager(nullptr), _playerTracker(*this), _tickLoop(*this) {}

Server::~Server() {
	// No more packets first, then save the players and the worlds, then drop the players (they release their chunks)
	if (_networkManager) _networkManager->stopThreads();
	for (const auto& level : _levels) level->saveEntities(); // Mobs go into their chunks first
	if (_world && _playerData) savePlayers();				  // PlayerList.saveAll, before the I/O threads stop
	for (const auto& level : _levels) level->savePoi();
	for (const auto& world : _worlds) world->shutdown();
	_gamePlayers.clear();
	_levels.clear();
	_level = nullptr;
	// Player destructors use _idManager, which is destroyed before the player maps
	clearPlayers();
	delete _networkManager;
}

Level* Server::getLevel(const std::string& dimension) {
	for (const auto& level : _levels) {
		if (level->dimensionName() == dimension) return level.get();
	}
	return nullptr;
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

void Server::sendActionBar(Player& player, const std::string& translationKey) {
	Buffer message;
	TextComponent::writeTranslatable(message, translationKey, {});
	message.writeBool(true); // Above the hotbar
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
	if (_level) _level->broadcastToChunk(chunkX, chunkZ, packetId, data, except);
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

// MinecraftServer.tickChildren: every level ticks (ServerLevel.tick, the overworld first), then the players
// (their connections), then what changed goes to the clients
void Server::tick(bool worldRuns) {
	for (const auto& level : _levels) tickLevel(*level, worldRuns);
	// The clients advance the time themselves: resynchronized every second, like vanilla
	if (_tickLoop.getTickCount() % 20 == 0) sendTime(nullptr);

	// The players (ServerGamePacketListenerImpl.tick -> ServerPlayer.doTick)
	for (const auto& player : _gamePlayers) {
		if (!player->isDisconnected() && player->level() && !player->wonGame()) {
			Portals::handlePortal(*player->level(), *player); // Entity.baseTick's handlePortal
			if (player->wonGame()) continue;
			Combat::tick(*this, *player);
			Survival::tick(*player->level(), *player);
		}
	}
	// The movements of this tick to the players that see them
	_playerTracker.tick(_tickLoop.getTickCount());
	// Their entity data that changed (air, item in use)
	for (const auto& player : _gamePlayers) {
		if (!player->isDisconnected()) Survival::sendDirtyData(*this, *player);
	}
	// The block changes, then the actions they answer (Block Changed Ack), the entities and the inventories
	for (const auto& level : _levels) {
		level->sendChanges();
		level->sendEntityChanges();
	}
	// ServerPlayer.tick: the open menu's changes (the inventory's by default)
	for (const auto& player : _gamePlayers) {
		if (!player->isDisconnected() && player->level()) Menus::tick(*player, *player->level());
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
		for (const auto& level : _levels) level->saveEntities(); // Before chunks are saved or unloaded
		bool autosaved = false;
		for (const auto& world : _worlds) autosaved |= world->tick();
		if (autosaved) {
			savePlayers(); // MinecraftServer.saveEverything: the players with the chunks
			for (const auto& level : _levels) level->savePoi();
		}
		for (const auto& level : _levels) level->dropUnloadedChunks();
	}
}

// The phases of vanilla's ServerLevel.tick, in its order: this order is what redstone timings rely on
void Server::tickLevel(Level& level, bool worldRuns) {
	World& world = level.world();
	level.setHandlingTick(true);
	if (worldRuns) {
		// ServerLevel.advanceWeatherCycle: the level changes go to the players of this level, the start and stop of the
		// rain to everyone (vanilla's own broadcastAll)
		bool  wasRaining = level.isRaining();
		float oRain = world.rainLevel(), oThunder = world.thunderLevel();
		world.tickWeather(level.dimensionType().hasSkyLight);
		auto event = [](int type, float value) {
			Buffer buffer;
			buffer.writeUByte(static_cast<uint8_t>(type));
			buffer.writeFloat(value);
			return buffer;
		};
		if (world.rainLevel() != oRain) {
			Buffer packet = event(GAME_EVENT_RAIN_LEVEL, world.rainLevel());
			level.broadcastToLevel(PacketId::Play::Clientbound::GAME_EVENT, packet);
		}
		if (world.thunderLevel() != oThunder) {
			Buffer packet = event(GAME_EVENT_THUNDER_LEVEL, world.thunderLevel());
			level.broadcastToLevel(PacketId::Play::Clientbound::GAME_EVENT, packet);
		}
		if (wasRaining != level.isRaining()) {
			Buffer toggle = event(wasRaining ? GAME_EVENT_STOP_RAIN : GAME_EVENT_START_RAIN, 0.0F);
			Buffer rain	  = event(GAME_EVENT_RAIN_LEVEL, world.rainLevel());
			Buffer thunder = event(GAME_EVENT_THUNDER_LEVEL, world.thunderLevel());
			broadcastToGame(PacketId::Play::Clientbound::GAME_EVENT, toggle);
			broadcastToGame(PacketId::Play::Clientbound::GAME_EVENT, rain);
			broadcastToGame(PacketId::Play::Clientbound::GAME_EVENT, thunder);
		}
	}
	tickSleeping(level);
	level.updateSkyBrightness();
	if (worldRuns) {
		world.tickTime();
		level.tickScheduled(); // Block ticks, then fluid ticks
		level.tickChunks();	   // Random ticks
		level.runBlockEvents();
	}
	level.setHandlingTick(false);
	// Entities (items, mobs...) and block entities stop while the game is frozen
	if (worldRuns) {
		level.tickEntities();
		level.tickBlockEntities();
	}
}

void Server::tickSleeping(Level& level) {
	// SleepStatus: every player but spectators counts; each sleeping one's timer counts to 100 (Player.sleepCounter),
	// and a player wakes up if its bed is gone
	int active = 0, sleeping = 0, deepSleeping = 0;
	for (const auto& player : level.players()) {
		if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator) continue;
		active++;
		SurvivalState& state = player->survival();
		if (!state.sleeping) continue;
		int bedState = level.getBlockState(state.sleepingPos);
		if (bedState < 0 || !_gameData.isInstanceOf(level.blocks().blockOf(bedState), "BedBlock")) {
			wakeUp(*player); // The bed is gone: the player wakes up (LivingEntity.tick)
			continue;
		}
		sleeping++;
		state.sleepTimer = std::min(100, state.sleepTimer + 1);
		if (state.sleepTimer >= 100) deepSleeping++;
	}
	// playersSleepingPercentage 100: everyone in the level sleeps long enough, the night is skipped to the next morning
	int needed = std::max(1, active);
	if (sleeping < needed || deepSleeping < needed) return;
	int64_t time = level.getDayTime() + 24000;
	level.world().setDayTime(time - time % 24000);
	for (const auto& player : level.players()) {
		if (!player->isDisconnected() && player->survival().sleeping) wakeUp(*player);
	}
	if (level.isRaining()) level.world().resetWeatherCycle(); // The night cleared the weather
}

void Server::wakeUp(Player& player) {
	SurvivalState& state = player.survival();
	if (!state.sleeping || !player.level()) return;
	Level& level	 = *player.level();
	state.sleeping	 = false;
	state.sleepTimer = 0;
	// The bed is free again (LivingEntity.stopSleeping): only if it is still there
	const BlockPos& bed		= state.sleepingPos;
	int				bedState = level.getBlockState(bed);
	if (bedState >= 0 && _gameData.isInstanceOf(level.blocks().blockOf(bedState), "BedBlock")) {
		level.setBlock(bed, level.blocks().withBool(bedState, level.blocks().property("occupied"), false), Level::UPDATE_CLIENTS);
	}
	// The player stands up, where the bed is (its feet on the floor)
	player.setPosition(bed.x + 0.5, bed.y, bed.z + 0.5);
	player.setOnGround(true);
	// The wake animation (ClientboundAnimatePacket) and the standing pose, to everyone that sees the player
	Buffer animate;
	animate.writeVarInt(player.getPlayerID());
	animate.writeUByte(ANIMATE_WAKE_UP);
	_playerTracker.broadcast(&player, PacketId::Play::Clientbound::ANIMATE, animate, false);
	Survival::setPose(player, Pose::Standing);
	_playerTracker.move(&player);
}

void Server::sendDefaultSpawn(const std::shared_ptr<Player>& to) {
	if (!_world) return;
	const World::Spawn& spawn = _world->getSpawn();
	Buffer				packet;
	packet.writeString(_world->getDimensionName()); // GlobalPos: the dimension, then the position
	packet.writePosition(static_cast<int>(std::floor(spawn.x)), static_cast<int>(std::floor(spawn.y)), static_cast<int>(std::floor(spawn.z)));
	packet.writeFloat(0.0F); // Yaw
	packet.writeFloat(0.0F); // Pitch
	if (to) {
		Packet::send(to, PacketId::Play::Clientbound::SET_DEFAULT_SPAWN_POSITION, packet, *this);
	} else {
		broadcastToGame(PacketId::Play::Clientbound::SET_DEFAULT_SPAWN_POSITION, packet);
	}
}

void Server::sendLevelInfo(const std::shared_ptr<Player>& player, Level& level) {
	// The world border (no border command yet: vanilla's default, centered on 0, 0)
	Buffer border;
	border.writeDouble(0.0);						   // Center x
	border.writeDouble(0.0);						   // Center z
	border.writeDouble(static_cast<double>(5.999997E7F)); // Old size
	border.writeDouble(static_cast<double>(5.999997E7F)); // New size
	border.writeVarLong(0);							   // Lerp time
	border.writeVarInt(29999984);					   // Absolute max size
	border.writeVarInt(5);							   // Warning blocks
	border.writeVarInt(15);							   // Warning time
	Packet::send(player, PacketId::Play::Clientbound::INITIALIZE_BORDER, border, *this);
	sendTime(player);
	sendDefaultSpawn(player);
	if (level.isRaining()) {
		auto event = [&](int type, float value) {
			Buffer buffer;
			buffer.writeUByte(static_cast<uint8_t>(type));
			buffer.writeFloat(value);
			Packet::send(player, PacketId::Play::Clientbound::GAME_EVENT, buffer, *this);
		};
		event(GAME_EVENT_START_RAIN, 0.0F);
		event(GAME_EVENT_RAIN_LEVEL, level.getRainLevel());
		event(GAME_EVENT_THUNDER_LEVEL, level.getThunderLevel() * level.getRainLevel());
	}
	Buffer waitChunks;
	waitChunks.writeUByte(GAME_EVENT_WAIT_CHUNKS);
	waitChunks.writeFloat(0);
	Packet::send(player, PacketId::Play::Clientbound::GAME_EVENT, waitChunks, *this);
	sendTickingState(player); // TickRateManager.updateJoiningPlayer
}

void Server::showEndCredits(Player& player) {
	if (player.wonGame()) return;
	std::shared_ptr<Player> self = player.shared_from_this();
	// removePlayerImmediately: gone from the end, its chunks and its viewers
	if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->stop();
	if (Level* level = player.level()) level->removePlayer(&player);
	player.setWonGame(true);
	Buffer event;
	event.writeUByte(GAME_EVENT_WIN_GAME);
	event.writeFloat(0.0F); // Like vanilla: the client rolls the credits
	Packet::send(self, PacketId::Play::Clientbound::GAME_EVENT, event, *this);
	player.setSeenCredits(true);
}

void Server::changeDimension(Player& player, Level& destination, double x, double y, double z, float yaw, float pitch, uint8_t keptData) {
	Level* origin = player.level();
	if (!origin || origin == &destination || player.isDisconnected()) return;
	std::shared_ptr<Player> self = player.shared_from_this();
	if (player.survival().sleeping) wakeUp(player);

	// Respawn into the new dimension, keeping the attributes and entity data (ClientboundRespawnPacket, KEEP_ALL_DATA)
	Buffer respawn;
	writeSpawnInfo(respawn, player, *this, destination);
	respawn.writeUByte(keptData);
	Packet::send(self, PacketId::Play::Clientbound::RESPAWN, respawn, *this);
	Packet packet(self);
	changeDifficultyPacket(packet, *this);

	// It leaves the old level (its chunks, its entities) and joins the new one at the destination
	if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->stop();
	origin->removePlayer(&player);
	player.setLevel(&destination);
	player.setPosition(x, y, z);
	player.setRotation(yaw, pitch);
	player.combat().fallDistance = 0;
	synchronizePlayerPositionPacket(packet, *this);
	destination.addPlayer(self);
	playerAbilitiesPacket(packet, *this);
	sendLevelInfo(self, destination);
	// PlayerList.sendAllPlayerInfo: the whole inventory, health and food again, the selected slot
	Menus::inventory(player, destination).sendAllDataToRemote();
	player.survival().lastSentHealth = -1.0F;
	player.survival().lastSentFood	 = -1;
	setHeldItemPacket(packet, *this);

	int viewDistance = player.getChunkStreamer() ? player.getChunkStreamer()->viewDistance() : _config.getViewDistance();
	if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->start(x, z, viewDistance);
	_playerTracker.respawn(&player);
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
	if (player->level()) player->level()->addPlayer(player);
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
	if (player->level()) player->level()->removePlayer(player);
	auto it = std::find_if(_gamePlayers.begin(), _gamePlayers.end(), [player](const auto& p) { return p.get() == player; });
	if (it == _gamePlayers.end()) return;
	*it = std::move(_gamePlayers.back());
	_gamePlayers.pop_back();
}

void Server::savePlayer(const Player& player) {
	const std::string& dimension = player.level() ? player.level()->dimensionName() : _world->getDimensionName();
	_playerData->save(player.getUUID(), PlayerData::save(player, _gameData, dimension));
}

void Server::savePlayers() {
	for (const auto& player : _gamePlayers) savePlayer(*player);
}

void Server::loadDimension(const World::Settings& overworldSettings, const std::string& dimension) {
	World::Settings settings = overworldSettings;
	settings.dimension		 = dimension;
	// DimensionType.getStorageFolder: the vanilla layout, so an imported world finds its nether and end too
	if (dimension == "minecraft:the_nether") {
		settings.directory = overworldSettings.directory / "DIM-1";
		// No generator for the nether yet: a superflat of netherrack under a bedrock floor
		settings.generator = {{"type", "flat"},
							  {"biome", "minecraft:nether_wastes"},
							  {"layers", {{{"block", "minecraft:bedrock"}, {"height", 1}}, {{"block", "minecraft:netherrack"}, {"height", 63}}}}};
	} else if (dimension == "minecraft:the_end") {
		settings.directory = overworldSettings.directory / "DIM1";
		// A superflat of end stone, the obsidian platform is placed when a player arrives
		settings.generator = {{"type", "flat"}, {"biome", "minecraft:the_end"}, {"layers", {{{"block", "minecraft:end_stone"}, {"height", 48}}}}};
	}
	if (_world) settings.seed = _world->getSeed();
	auto world = std::make_unique<World>(_gameData, settings);
	if (_world) world->setPrimary(_world);
	auto level = std::make_unique<Level>(*this, *world, _gameData);
	// Chunks finish loading on I/O threads: their scheduled ticks start counting on the game thread
	Level* levelPtr = level.get();
	world->setChunkLoadListener([this, levelPtr](const std::shared_ptr<Chunk>& chunk) { _tickLoop.post([levelPtr, chunk] { levelPtr->onChunkLoaded(chunk); }); });
	if (!_world) {
		_world = world.get();
		_level = level.get();
	}
	_worlds.push_back(std::move(world));
	_levels.push_back(std::move(level));
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
			// The overworld first: the other dimensions take its seed, clock and weather
			loadDimension(worldSettings, "minecraft:overworld");
			loadDimension(worldSettings, "minecraft:the_nether");
			loadDimension(worldSettings, "minecraft:the_end");
		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR, "Failed to load world: " + std::string(e.what()), "SERVER");
			return 1;
		}
		_playerData = std::make_unique<PlayerDataStorage>(worldSettings.directory, [this](std::function<void()> job) { _world->submitSave(std::move(job)); });

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
