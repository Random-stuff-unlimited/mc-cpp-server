#include "PacketIds.hpp"
#include "logger.hpp"
#include "network/PluginChannels.hpp"
#include "network/packetRouter.hpp"

// Network thread: the Login state. Login Start names the player, Login Acknowledged moves it to Configuration
void handleLoginState(Packet* packet, Server& server) {
	if (packet->getSize() > 32767) {
		g_logger->logNetwork(ERROR, "Packet size too large: " + std::to_string(packet->getSize()), "PacketRouter");
		packet->setReturnPacket(PACKET_DISCONNECT);
		return;
	}
	switch (packet->getId()) {
	case PacketId::Login::Serverbound::HELLO:
		handleLoginStartPacket(*packet, server);
		break;
	case PacketId::Login::Serverbound::LOGIN_ACKNOWLEDGED:
		handleLoginAcknowledgedPacket(*packet, server);
		if (packet->getPlayer()) PluginChannels::sendBrand(server, *packet->getPlayer()); // startConfiguration
		clientboundFeatureFlagsPacket(*packet, server);
		clientboundKnownPacksPacket(*packet, server);
		break;
	case PacketId::Login::Serverbound::CUSTOM_QUERY_ANSWER:
		// Answers to requests the server doesn't send: nothing to do
		break;
	case PacketId::Login::Serverbound::COOKIE_RESPONSE:
		if (packet->getPlayer()) Cookies::handleResponse(*packet->getPlayer(), packet->getData());
		break;
	default:
		disconnect(packet);
		break;
	}
}