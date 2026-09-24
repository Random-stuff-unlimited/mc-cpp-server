#include "PacketIds.hpp"
#include "lib/json.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <cstdint>
#include <string>
#include <unistd.h>

void handleHandshakePacket(Packet& packet, Server& server) {
	if (packet.getId() != 0x00) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}
	int			protocolVersion = packet.getData().readVarInt();
	std::string serverAddr		= packet.getData().readString(255);
	uint16_t	port			= packet.getData().readUShort();
	int			nextState		= packet.getData().readVarInt();
	// g_logger->logNetwork(INFO, "Protocol=" + std::to_string(protocolVersion) + ", Addr=" +
	// serverAddr + ", State=" + std::to_string(nextState), "Handshake");
	if (nextState == 1) {
		packet.getPlayer()->setPlayerState(PlayerState::Status);
		// g_logger->logNetwork(INFO, "Status request - keeping in temp list", "Handshake");
	} else if (nextState == 2) {
		packet.getPlayer()->setPlayerState(PlayerState::Login);
		const GameData& gameData = server.getGameData();
		if (protocolVersion != gameData.getProtocolVersion()) {
			// Login Disconnect: the client shows this message instead of failing later during configuration
			nlohmann::json reason = {{"text", "Incompatible version: this server runs Minecraft " + gameData.getVersionName()}};
			Buffer		   payload;
			payload.writeString(reason.dump());
			packet.sendPacket(PacketId::Login::Clientbound::LOGIN_DISCONNECT, payload, server);
			packet.getPlayer()->setPlayerState(PlayerState::None);
			packet.setReturnPacket(PACKET_DISCONNECT);
			return;
		}
		server.promoteTempPlayer(packet.getPlayer());
		// g_logger->logNetwork(INFO, "Login attempt - player promoted to main list", "Handshake");
	} else {
		// g_logger->logNetwork(INFO, "Status packet sent removed the player", "Handshake");
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
	}
	(void)port;
}
