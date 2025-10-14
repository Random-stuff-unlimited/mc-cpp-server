#include "lib/json.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <string>
#include <unistd.h>

using json = nlohmann::json;

void handleStatusPacket(Packet& packet, Server& server) {
	if (packet.getId() != 0x00) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	json jres = {
			{"version", {{"name", server.getConfig().getVersion()}, {"protocol", server.getConfig().getProtocolVersion()}}},
			{"players", {{"max", server.getConfig().getServerSize()}, {"online", server.getAmountOnline()}, {"sample", server.getPlayerSample()}}},
			{"description", {{"text", server.getConfig().getServerMotd()}}}};
	std::string payload = jres.dump();

	int jsonLen	 = payload.size();
	int packetId = 0x00;

	int packetIdVarintSize = packet.getVarintSize(packetId);
	int jsonLenVarintSize  = packet.getVarintSize(jsonLen);
	int totalPayloadSize   = packetIdVarintSize + jsonLenVarintSize + jsonLen;

	Buffer buf;
	buf.writeVarInt(totalPayloadSize);
	buf.writeVarInt(packetId);
	buf.writeVarInt(jsonLen);
	buf.writeBytes(payload.c_str());
	packet.getData() = buf;
	packet.setReturnPacket(PACKET_SEND);
	packet.setPacketSize(buf.getData().size());
	packet.getPlayer()->setPlayerState(PlayerState::Status);

	// g_logger->logNetwork(INFO, "JSON response ready - connection will be closed", "Status");
}
