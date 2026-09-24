#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "network/packetRouter.hpp"
#include "PacketIds.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/World.hpp"

#include <algorithm>
#include "player.hpp"

#include <string>

namespace {
	GameMode gameModeFromConfig(const std::string& name) {
		if (name == "creative") return GameMode::Creative;
		if (name == "adventure") return GameMode::Adventure;
		if (name == "spectator") return GameMode::Spectator;
		return GameMode::Survival;
	}

	void disconnect(Packet* packet) {
		packet->getPlayer()->setPlayerState(PlayerState::None);
		packet->setReturnPacket(PACKET_DISCONNECT);
	}

	void enterPlay(Packet* packet, Server& server) {
		g_logger->logNetwork(INFO, "Transitioning to Play state", "Configuration");
		handleAcknowledgeFinishConfigurationPacket(*packet, server);

		Player*				player = packet->getPlayer();
		const World::Spawn& spawn  = server.getWorld().getSpawn();
		player->setGameMode(gameModeFromConfig(server.getConfig().getGamemode()));
		player->setPosition(spawn.x, spawn.y, spawn.z);

		sendPlayPacket(*packet, server);
		changeDifficultyPacket(*packet, server);
		playerAbilitiesPacket(*packet, server);
		setHeldItemPacket(*packet, server);
		synchronizePlayerPositionPacket(*packet, server);

		// Stream the chunks around the spawn, within the smaller of the server's and the client's view distance
		int viewDistance = std::min<int>(server.getConfig().getViewDistance(), player->getPlayerConfig()->getViewDistance());
		player->createChunkStreamer();
		player->getChunkStreamer()->start(spawn.x, spawn.z, std::max(2, viewDistance));
	}

	void handleMove(Packet* packet, bool withRotation) {
		Buffer& data = packet->getData();
		double	x	 = data.readDouble();
		double	y	 = data.readDouble(); // Feet
		double	z	 = data.readDouble();
		if (withRotation) packet->getPlayer()->setYaw(data.readFloat());
		packet->getPlayer()->setPosition(x, y, z);
		if (ChunkStreamer* streamer = packet->getPlayer()->getChunkStreamer()) streamer->onPlayerMove(x, z);
	}
} // namespace

// ========================================
// Main Packet Router
// ========================================

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

	int32_t id = static_cast<int32_t>(packet->getId());
	switch (player->getPlayerState()) {
	case PlayerState::Handshake:
		if (id == PacketId::Handshake::Serverbound::INTENTION) {
			handleHandshakePacket(*packet, server);
		} else {
			disconnect(packet);
		}
		break;

	case PlayerState::Status:
		if (id == PacketId::Status::Serverbound::STATUS_REQUEST) {
			handleStatusPacket(*packet, server);
		} else if (id == PacketId::Status::Serverbound::PING_REQUEST) {
			handlePingPacket(*packet, server);
		} else {
			disconnect(packet);
		}
		break;

	case PlayerState::Login:
		if (packet->getSize() > 32767) {
			g_logger->logNetwork(ERROR, "Packet size too large: " + std::to_string(packet->getSize()), "PacketRouter");
			packet->setReturnPacket(PACKET_DISCONNECT);
			return;
		}
		if (id == PacketId::Login::Serverbound::HELLO) {
			handleLoginStartPacket(*packet, server);
		} else if (id == PacketId::Login::Serverbound::LOGIN_ACKNOWLEDGED) {
			handleLoginAcknowledgedPacket(*packet, server);
			clientboundFeatureFlagsPacket(*packet, server);
			clientboundKnownPacksPacket(*packet, server);
		} else if (id == PacketId::Login::Serverbound::CUSTOM_QUERY_ANSWER || id == PacketId::Login::Serverbound::COOKIE_RESPONSE) {
			// Answers to requests the server doesn't send yet: nothing to do
		} else {
			disconnect(packet);
		}
		break;

	case PlayerState::Configuration:
		if (id == PacketId::Configuration::Serverbound::CLIENT_INFORMATION) {
			handleClientInformationPacket(*packet, server);
		} else if (id == PacketId::Configuration::Serverbound::SELECT_KNOWN_PACKS) {
			serverboundKnownPacksPacket(*packet);
			sendRegistryData(*packet, server);
			sendUpdateTags(*packet, server);
			handleFinishConfigurationPacket(*packet, server);
		} else if (id == PacketId::Configuration::Serverbound::FINISH_CONFIGURATION) {
			enterPlay(packet, server);
		} else if (id == PacketId::Configuration::Serverbound::COOKIE_RESPONSE || id == PacketId::Configuration::Serverbound::CUSTOM_PAYLOAD ||
				   id == PacketId::Configuration::Serverbound::KEEP_ALIVE || id == PacketId::Configuration::Serverbound::PONG ||
				   id == PacketId::Configuration::Serverbound::RESOURCE_PACK ||
				   id == PacketId::Configuration::Serverbound::CUSTOM_CLICK_ACTION) {
			// Nothing to do yet
		} else {
			Buffer payload;
			payload.writeString("{\"text\":\"Unknown packet in Configuration state\"}");
			packet->sendPacket(PacketId::Configuration::Clientbound::DISCONNECT, payload, server);
			disconnect(packet);
		}
		break;

	case PlayerState::Play:
		switch (id) {
		case PacketId::Play::Serverbound::ACCEPT_TELEPORTATION:
			handleConfirmTeleportationPacket(*packet, server);
			gameEventPacket(*packet, server);
			break;
		case PacketId::Play::Serverbound::MOVE_PLAYER_POS:
			handleMove(packet, false);
			break;
		case PacketId::Play::Serverbound::MOVE_PLAYER_POS_ROT:
			handleMove(packet, true);
			break;
		case PacketId::Play::Serverbound::MOVE_PLAYER_ROT:
			player->setYaw(packet->getData().readFloat());
			break;
		case PacketId::Play::Serverbound::CHUNK_BATCH_RECEIVED:
			if (ChunkStreamer* streamer = player->getChunkStreamer()) streamer->onBatchReceived(packet->getData().readFloat());
			break;
		case PacketId::Play::Serverbound::KEEP_ALIVE:
			player->onKeepAliveResponse(packet->getData().readLong());
			break;
		case PacketId::Play::Serverbound::PLAYER_ACTION:
			handlePlayerActionPacket(*packet, server);
			break;
		case PacketId::Play::Serverbound::USE_ITEM_ON:
			handleUseItemOnPacket(*packet, server);
			break;
		case PacketId::Play::Serverbound::SET_CARRIED_ITEM:
			handleSetCarriedItemPacket(*packet, server);
			break;
		case PacketId::Play::Serverbound::SET_CREATIVE_MODE_SLOT:
			handleSetCreativeModeSlotPacket(*packet, server);
			break;
		case PacketId::Play::Serverbound::PLAYER_LOADED:
			g_logger->logNetwork(DEBUG, "Player fully loaded in game", "Play");
			break;
		default:
			break; // Not handled yet (see docs/PACKETS_MISSING.md)
		}
		break;

	default:
		g_logger->logNetwork(WARN, "Unknown player state: " + std::to_string(static_cast<int>(player->getPlayerState())), "PacketRouter");
		packet->setReturnPacket(PACKET_DISCONNECT);
		break;
	}
}
