#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <unistd.h>
#include <iostream>

void synchronizePlayerPositionPacket(Packet& packet, Server& server) {
	std::cout << "=== Sending Player Position and Look ===\n";

	Buffer buf;

	// Teleport ID
	buf.writeVarInt(1);

	// Player position (using writeInt64 for double precision storage)
	// Note: This is a simplified approach - ideally you'd add writeDouble to Buffer
	int64_t x_bits = 0x3FE0000000000000;
	int64_t y_bits = 0x4050000000000000;
	int64_t z_bits = 0x3FE0000000000000;

	buf.writeInt64(x_bits);
	buf.writeInt64(y_bits);
	buf.writeInt64(z_bits);

	// Velocity (all zero)
	buf.writeInt64(0);
	buf.writeInt64(0);
	buf.writeInt64(0);

	// Rotation (using writeInt for float storage)
	buf.writeInt(0);
	buf.writeInt(0);

	// Flags (0x00 = absolute positioning)
	buf.writeInt(0x00);

	int packetId		 = 0x41;
	int packetIdSize	 = packet.getVarintSize(packetId);
	int totalPayloadSize = packetIdSize + buf.getData().size();

	Buffer finalBuf;
	finalBuf.writeVarInt(totalPayloadSize);
	finalBuf.writeVarInt(packetId);
	finalBuf.writeBytes(buf.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}
