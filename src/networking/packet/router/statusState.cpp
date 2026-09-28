#include "PacketIds.hpp"
#include "network/packetRouter.hpp"

// Network thread: the Status state. Status Request gets the server description, Ping gets its timestamp back
void handleStatusState(Packet* packet, Server& server) {
	switch (packet->getId()) {
	case PacketId::Status::Serverbound::STATUS_REQUEST:
		handleStatusPacket(*packet, server);
		break;
	case PacketId::Status::Serverbound::PING_REQUEST:
		handlePingPacket(*packet, server);
		break;
	default:
		disconnect(packet);
		break;
	}
}