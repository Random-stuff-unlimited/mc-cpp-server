#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include "logger.hpp"
#include "buffer.hpp"
#include <iostream>

void handleCookieRequest(Packet& packet, Server& server) {
    g_logger->logNetwork(INFO, "Received Cookie Request in Configuration state", "Configuration");

    Player* player = packet.getPlayer();
    if (!player) {
        g_logger->logNetwork(ERROR, "Error: No player associated with Cookie Request packet", "Configuration");
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    // Debug: Log raw packet data
    g_logger->logNetwork(INFO, "Cookie Request packet size: " + std::to_string(packet.getSize()) + " bytes", "Configuration");

    // Read the cookie identifier from the request
    std::string cookieIdentifier;
    try {
        // Create a fresh buffer from the packet data to read from beginning
        Buffer cookieBuffer(packet.getData().getData());
        cookieIdentifier = cookieBuffer.readString(32767); // Max string length
        g_logger->logNetwork(INFO, "Cookie Request for identifier: '" + cookieIdentifier + "'", "Configuration");
    } catch (const std::exception& e) {
        g_logger->logNetwork(ERROR, "Failed to read cookie identifier: " + std::string(e.what()), "Configuration");
        // Send empty response instead of disconnecting
        cookieIdentifier = "unknown";
    }

    // Create Cookie Response packet (0x01)
    Buffer payload;
    payload.writeVarInt(0x01); // Cookie Response packet ID
    payload.writeString(cookieIdentifier); // Echo back the identifier

    // For now, send empty cookie data (no stored cookie)
    payload.writeByte(0x00); // Has payload: false (no cookie data)

    Buffer final;
    final.writeVarInt(payload.getData().size());
    final.writeBytes(payload.getData());

    packet.getData() = final;
    packet.setReturnPacket(PACKET_SEND);
    packet.setPacketSize(final.getData().size());

    g_logger->logNetwork(INFO, "Sent Cookie Response for identifier: '" + cookieIdentifier + "' (no data), response size: " + std::to_string(final.getData().size()), "Configuration");

    (void)server; // Suppress unused parameter warning
}
