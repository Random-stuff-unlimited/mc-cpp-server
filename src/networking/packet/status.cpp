#include "buffer.hpp"
#include "enums.hpp"
#include "json.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "server.hpp"

#include <iostream>
#include <string>
#include <unistd.h>

using json = nlohmann::json;

void handleStatusPacket(Packet& packet, Server& server) {
	if (packet.getId() != 0x00) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		close(packet.getSocket());
		server.removePlayer(packet.getPlayer());
		return;
	}

	json jres           = {{"version",
	                        {{"name", server.getGameVersion()}, {"protocol", server.getProtocolVersion()}}},
	                       {"players",
	                        {{"max", server.getServerSize()},
	                         {"online", server.getAmountOnline()},
	                         {"sample", server.getPlayerSample()}}},
	                       {"description", {{"text", server.getServerMOTD()}}}};
	std::string payload = jres.dump();

	int jsonLen  = payload.size();
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
	packet.setReturnPacket(1);

	packet.setPacketSize(buf.getData().size());

	std::cout << "[Status] JSON response ready" << std::endl;
}
