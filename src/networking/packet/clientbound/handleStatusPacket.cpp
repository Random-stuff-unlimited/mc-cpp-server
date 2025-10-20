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

	Buffer buf;

	int jsonLen	 = payload.size();

	buf.writeVarInt(jsonLen);
	buf.writeBytes(payload.c_str());

	packet.sendPacket(0x00, buf, server, true);
}
