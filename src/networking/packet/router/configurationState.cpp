#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packetRouter.hpp"

// Network thread: the Configuration state. Client Information is kept, Select Known Packs sends the registries and
// ends the configuration
void handleConfigurationState(Packet* packet, Server& server) {
	switch (packet->getId()) {
	case PacketId::Configuration::Serverbound::CLIENT_INFORMATION:
		handleClientInformationPacket(*packet, server);
		break;
	case PacketId::Configuration::Serverbound::SELECT_KNOWN_PACKS:
		serverboundKnownPacksPacket(*packet);
		sendRegistryData(*packet, server);
		sendUpdateTags(*packet, server);
		handleFinishConfigurationPacket(*packet, server);
		break;
	case PacketId::Configuration::Serverbound::COOKIE_RESPONSE:
	case PacketId::Configuration::Serverbound::CUSTOM_PAYLOAD:
	case PacketId::Configuration::Serverbound::KEEP_ALIVE:
	case PacketId::Configuration::Serverbound::PONG:
	case PacketId::Configuration::Serverbound::RESOURCE_PACK:
	case PacketId::Configuration::Serverbound::CUSTOM_CLICK_ACTION:
		// Nothing to do yet
		break;
	default: {
		Buffer payload;
		payload.writeString("{\"text\":\"Unknown packet in Configuration state\"}");
		packet->sendPacket(PacketId::Configuration::Clientbound::DISCONNECT, payload, server);
		disconnect(packet);
		break;
	}
	}
}