#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "network/packetRouter.hpp"
#include "PacketIds.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Combat.hpp"
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
		viewDistance = std::max(2, viewDistance);
		player->createChunkStreamer();
		player->getChunkStreamer()->start(spawn.x, spawn.z, viewDistance);

		// Tab list and player entities, both ways
		server.getPlayerTracker().join(player->shared_from_this(), viewDistance);
		Combat::sendHealth(server, *player);
	}

	constexpr uint8_t MOVE_FLAG_ON_GROUND = 0x01;

	void handleMove(Packet* packet, Server& server, bool withPosition, bool withRotation) {
		Buffer& data	  = packet->getData();
		Player* player	  = packet->getPlayer();
		double	x = player->getX(), y = player->getY(), z = player->getZ();
		double	previousY = y;
		if (withPosition) {
			x = data.readDouble();
			y = data.readDouble(); // Feet
			z = data.readDouble();
			player->setPosition(x, y, z);
		}
		if (withRotation) {
			float yaw = data.readFloat();
			player->setRotation(yaw, data.readFloat());
		}
		player->setOnGround(data.readUByte() & MOVE_FLAG_ON_GROUND);

		if (withPosition) {
			if (ChunkStreamer* streamer = player->getChunkStreamer()) streamer->onPlayerMove(x, z);
		}
		server.getPlayerTracker().move(player, withPosition, withRotation);
		if (withPosition) Combat::onMove(server, *player, previousY);
	}

	enum InteractType { INTERACT = 0, ATTACK = 1, INTERACT_AT = 2 };
	enum PlayerCommandAction { START_SPRINTING = 1, STOP_SPRINTING = 2 };
	constexpr int CLIENT_COMMAND_RESPAWN = 0;
	constexpr int ANIMATE_SWING_MAIN_HAND = 0, ANIMATE_SWING_OFF_HAND = 3;
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
			handleMove(packet, server, true, false);
			break;
		case PacketId::Play::Serverbound::MOVE_PLAYER_POS_ROT:
			handleMove(packet, server, true, true);
			break;
		case PacketId::Play::Serverbound::MOVE_PLAYER_ROT:
			handleMove(packet, server, false, true);
			break;
		case PacketId::Play::Serverbound::MOVE_PLAYER_STATUS_ONLY:
			handleMove(packet, server, false, false);
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
		case PacketId::Play::Serverbound::INTERACT: {
			int entityId = packet->getData().readVarInt();
			if (packet->getData().readVarInt() == ATTACK) {
				if (auto target = server.getPlayerTracker().findVisible(player, entityId)) Combat::attack(server, *player, *target);
			}
			// Right-clicking entities isn't handled yet
			break;
		}
		case PacketId::Play::Serverbound::SWING: {
			Buffer animation;
			animation.writeVarInt(player->getPlayerID());
			animation.writeUByte(packet->getData().readVarInt() == 0 ? ANIMATE_SWING_MAIN_HAND : ANIMATE_SWING_OFF_HAND);
			server.getPlayerTracker().broadcast(player, PacketId::Play::Clientbound::ANIMATE, animation, false);
			break;
		}
		case PacketId::Play::Serverbound::PLAYER_COMMAND: {
			packet->getData().readVarInt(); // Entity id (always the player itself)
			int action = packet->getData().readVarInt();
			if (action == START_SPRINTING) player->setSprinting(true);
			if (action == STOP_SPRINTING) player->setSprinting(false);
			break;
		}
		case PacketId::Play::Serverbound::CLIENT_COMMAND:
			if (packet->getData().readVarInt() == CLIENT_COMMAND_RESPAWN) Combat::respawn(server, *player);
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
