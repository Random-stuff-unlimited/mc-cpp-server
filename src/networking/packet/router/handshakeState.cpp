#include "PacketIds.hpp"
#include "network/packetRouter.hpp"

// Network thread: the Handshake state. The client sends its Intention, the server picks the next state
void handleHandshakeState(Packet* packet, Server& server) {
	if (packet->getId() == PacketId::Handshake::Serverbound::INTENTION) {
		handleHandshakePacket(*packet, server);
	} else {
		disconnect(packet);
	}
}