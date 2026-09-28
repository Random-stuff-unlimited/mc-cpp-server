#include "PacketIds.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Combat.hpp"
#include "world/PlayerDataStorage.hpp"
#include "world/World.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace {
	GameMode gameModeFromConfig(const std::string& name) {
		if (name == "creative") return GameMode::Creative;
		if (name == "adventure") return GameMode::Adventure;
		if (name == "spectator") return GameMode::Spectator;
		return GameMode::Survival;
	}
} // namespace

void disconnect(Packet* packet) {
	packet->getPlayer()->setPlayerState(PlayerState::None);
	packet->setReturnPacket(PACKET_DISCONNECT);
}

// Network threads: every state but Play. Each state is handled by its own file
void packetRouter(Packet* packet, Server& server) {
	if (packet == nullptr) return;

	Player* player = packet->getPlayer();
	if (player == nullptr) {
		packet->setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	g_logger->logNetwork(DEBUG,
						 "Routing packet ID: 0x" + std::to_string(packet->getId()) + " (size: " + std::to_string(packet->getSize()) +
								 ") for state: " + std::to_string(static_cast<int>(player->getPlayerState())),
						 "PacketRouter");

	switch (player->getPlayerState()) {
	case PlayerState::Handshake:
		handleHandshakeState(packet, server);
		break;
	case PlayerState::Status:
		handleStatusState(packet, server);
		break;
	case PlayerState::Login:
		handleLoginState(packet, server);
		break;
	case PlayerState::Configuration:
		handleConfigurationState(packet, server);
		break;
	default:
		g_logger->logNetwork(WARN, "Unknown player state: " + std::to_string(static_cast<int>(player->getPlayerState())), "PacketRouter");
		disconnect(packet);
		break;
	}
}

// Game thread: the Play state
void playPacketRouter(Packet* packet, Server& server) { handlePlayState(packet, server); }

// Game thread: the player acknowledged the end of the configuration, it enters the world
void enterPlay(Packet* packet, Server& server) {
	g_logger->logNetwork(INFO, "Transitioning to Play state", "Configuration");
	handleAcknowledgeFinishConfigurationPacket(*packet, server);

	Player*				player = packet->getPlayer();
	const World::Spawn& spawn  = server.getWorld().getSpawn();
	GameMode			defaultGameMode = gameModeFromConfig(server.getConfig().getGamemode());
	// PrepareSpawnTask: the saved position if the player played here before, the world spawn otherwise. The
	// configured game mode is only for new players (ServerPlayer.calculateGameModeForNewPlayer)
	player->setGameMode(defaultGameMode);
	player->setPosition(spawn.x, spawn.y, spawn.z);
	if (std::optional<nbt::TagCompound> saved = server.getPlayerData().load(player->getUUID())) {
		PlayerData::load(*player, *saved, server.getGameData(), defaultGameMode);
		std::string dimension = PlayerData::dimension(*saved);
		// A dimension the server doesn't have: vanilla puts the player in the overworld, at the saved position
		if (!dimension.empty() && dimension != server.getWorld().getDimensionName()) {
			g_logger->logGameInfo(WARN, player->getPlayerName() + " was in " + dimension + ", which isn't loaded: placed in " + server.getWorld().getDimensionName(),
								  "PlayerData");
		}
	}

	sendPlayPacket(*packet, server);
	changeDifficultyPacket(*packet, server);
	playerAbilitiesPacket(*packet, server);
	setHeldItemPacket(*packet, server);
	sendInitialRecipeBook(*packet, server);
	synchronizePlayerPositionPacket(*packet, server);

	// Stream the chunks around the player, within the smaller of the server's and the client's view distance
	int viewDistance = std::min<int>(server.getConfig().getViewDistance(), player->getPlayerConfig()->getViewDistance());
	viewDistance = std::max(2, viewDistance);
	player->createChunkStreamer();
	player->getChunkStreamer()->start(player->getX(), player->getZ(), viewDistance);

	// Tab list and player entities, both ways
	server.getPlayerTracker().join(player->shared_from_this(), viewDistance);
	Combat::sendHealth(server, *player);
	server.addGamePlayer(player->shared_from_this());
}