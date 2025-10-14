#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void handleClientInformation(Packet& packet, Server& server) {
	PlayerConfig* config = packet.getPlayer()->getPlayerConfig();

	config->setLocale(packet.getData().readString(16));
	config->setViewDistance(packet.getData().readByte());
	config->setChatMode(packet.getData().readVarInt());
	config->setChatColors(packet.getData().readByte() != 0);
	config->setDisplayedSkinParts(packet.getData().readByte());
	config->setMainHand(packet.getData().readVarInt());
	config->setTextFiltering(packet.getData().readByte() != 0);
	config->setServerListings(packet.getData().readByte() != 0);

	packet.setReturnPacket(PACKET_OK);
	(void)server;
}
