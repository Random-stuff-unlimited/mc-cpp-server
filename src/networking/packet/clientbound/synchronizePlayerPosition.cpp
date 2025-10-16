#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void synchronizePlayerPosition(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		g_logger->logNetwork(ERROR, "No player associated with synchronizePlayerPosition packet", "PACKET");
		packet.setReturnPacket(PACKET_ERROR);
		return;
	}

	Buffer buf;

	// Packet ID for Synchronize Player Position (0x41)
	buf.writeByte(0x41);

	// Teleport ID (VarInt) - unique identifier for this teleport
	buf.writeVarInt(1);

	// X position (Double) - absolute position
	buf.writeDouble(0.0);

	// Y position (Double) - spawn above ground
	buf.writeDouble(100.0);

	// Z position (Double) - absolute position
	buf.writeDouble(0.0);

	// DX (Double) - relative X change (not used when flags indicate absolute)
	buf.writeDouble(0.0);

	// DY (Double) - relative Y change (not used when flags indicate absolute)
	buf.writeDouble(0.0);

	// DZ (Double) - relative Z change (not used when flags indicate absolute)
	buf.writeDouble(0.0);

	// Yaw (Float) - horizontal rotation
	buf.writeFloat(0.0f);

	// Pitch (Float) - vertical rotation
	buf.writeFloat(0.0f);

	// Flags (Byte) - indicates which position updates are relative
	// 0x00 = all absolute positions (no relative updates)
	buf.writeByte(0x00);

	// Set packet data
	packet.setPacketId(0x41);
	packet.getData() = buf;
	packet.setPacketSize(buf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	g_logger->logNetwork(INFO, "Sent synchronizePlayerPosition packet (0x41)", "PACKET");
}
