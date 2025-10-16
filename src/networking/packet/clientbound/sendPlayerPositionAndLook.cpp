#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"

void sendPlayerPositionAndLook(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	// Packet ID for Synchronize Player Position (0x40 in Play state)
	buf.writeByte(0x40);

	// X position (Double)
	buf.writeDouble(0.0); // Default spawn position

	// Y position (Double)
	buf.writeDouble(100.0); // Above ground level

	// Z position (Double)
	buf.writeDouble(0.0); // Default spawn position

	// Yaw (Float) - rotation around Y axis
	buf.writeFloat(0.0f);

	// Pitch (Float) - rotation around X axis
	buf.writeFloat(0.0f);

	// Flags (Byte) - indicates which values are relative
	// 0x00 = all absolute positions
	buf.writeByte(0x00);

	// Teleport ID (VarInt) - for confirming teleportation
	buf.writeVarInt(1);

	// Set packet data
	packet.setPacketId(0x40);
	packet.getData() = buf;
	packet.setPacketSize(buf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}
