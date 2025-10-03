#include "buffer.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <iostream>
#include <unistd.h>

void handlePingPacket(Packet& packet, Server& server) {
	if (packet.getId() != 0x01) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	long timestamp = packet.getData().readLong();

	std::cout << "[Ping] Received ping request with timestamp: " << timestamp << std::endl;

	int packetId           = 0x01;
	int packetIdVarintSize = packet.getVarintSize(packetId);
	int totalPayloadSize   = packetIdVarintSize + 8;

	Buffer buf;
	buf.writeVarInt(totalPayloadSize);
	buf.writeVarInt(packetId);
	buf.writeLong(timestamp);

	packet.getData() = buf;
	packet.setReturnPacket(PACKET_SEND);
	packet.setPacketSize(buf.getData().size());
	packet.getPlayer()->setPlayerState(PlayerState::None);

	std::cout << "[Ping] Pong response ready - echoing timestamp " << timestamp << std::endl;

	(void)server;
}
