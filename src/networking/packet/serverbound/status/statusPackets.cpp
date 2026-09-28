#include "PacketIds.hpp"
#include "lib/json.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <string>
#include <unistd.h>

using json = nlohmann::json;

// The status packets the server receives. One file per state, by direction.

// ServerboundStatusRequestPacket: the server answers with its MOTD, version and player count
void handleStatusPacket(Packet& packet, Server& server) {
	if (packet.getId() != PacketId::Status::Serverbound::STATUS_REQUEST) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	json jres = {
			{"version", {{"name", server.getGameData().getVersionName()}, {"protocol", server.getGameData().getProtocolVersion()}}},
			{"players", {{"max", server.getConfig().getServerSize()}, {"online", server.getAmountOnline()}, {"sample", server.getPlayerSample()}}},
			{"description", {{"text", server.getConfig().getServerMotd()}}}};
	std::string payload = jres.dump();

	Buffer buf;

	int jsonLen = payload.size();

	buf.writeVarInt(jsonLen);
	buf.writeBytes(payload.c_str());

	packet.sendPacket(PacketId::Status::Clientbound::STATUS_RESPONSE, buf, server);
}

// ServerboundPingRequestPacket: the server answers with the same timestamp (connection quality check)
void handlePingPacket(Packet& packet, Server& server) {
	if (packet.getId() != PacketId::Status::Serverbound::PING_REQUEST) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	long timestamp = packet.getData().readInt64();

	Buffer buf;

	buf.writeInt64(timestamp);

	packet.sendPacket(PacketId::Status::Clientbound::PONG_RESPONSE, buf, server);
}