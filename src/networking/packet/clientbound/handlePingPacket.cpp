#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <unistd.h>

void handlePingPacket(Packet& packet, Server& server) {
	if (packet.getId() != 0x01) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	long timestamp = packet.getData().readInt64();

	Buffer buf;

	buf.writeInt64(timestamp);

	packet.sendPacket(0x01, buf, server, true);
}
