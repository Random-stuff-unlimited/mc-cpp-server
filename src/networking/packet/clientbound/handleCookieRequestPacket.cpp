#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void handleCookieRequestPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	// Read the cookie identifier from the request
	std::string cookieIdentifier;
	try {
		// Create a fresh buffer from the packet data to read from beginning
		Buffer cookieBuffer(packet.getData().getData());
		cookieIdentifier = cookieBuffer.readString(32767); // Max string length
	} catch (const std::exception& e) {
		// Send empty response instead of disconnecting
		cookieIdentifier = "unknown";
	}

	// Create Cookie Response packet (0x01)
	Buffer payload;
	payload.writeString(cookieIdentifier); // Echo back the identifier
	payload.writeByte(0x00); // Has payload: false (no cookie data)

	packet.sendPacket(0x01, payload, server, false);
}

void sendFinishConfigurationAfterCookie(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		return;
	}

	// Create Finish Configuration packet (0x03)
	Buffer payload;
	// No additional data needed for Finish Configuration

	packet.sendPacket(0x03, payload, server, false);
}