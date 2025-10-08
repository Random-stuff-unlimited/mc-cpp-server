#include "logger.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

#include <string>

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

	// g_logger->logGameInfo(INFO, "Received client settings:", "ClientInfo");
	// g_logger->logGameInfo(INFO, "Local: " + config->getLocale(), "ClientInfo");
	// g_logger->logGameInfo(
	//         INFO, "View Distance: " + std::to_string(config->getViewDistance()), "ClientInfo");
	// g_logger->logGameInfo(
	//         INFO, "Chat Mode: " + std::to_string(config->getChatMode()), "ClientInfo");
	// g_logger->logGameInfo(
	//         INFO, "Chat Colors: " + std::to_string(config->getChatColors()), "ClientInfo");
	// g_logger->logGameInfo(INFO,
	//                       "Displayed Skin Parts: " +
	//                               std::to_string(config->getDisplayedSkinParts()),
	//                       "ClientInfo");
	// g_logger->logGameInfo(
	//         INFO, "Main Hand: " + std::to_string(config->getMainHand()), "ClientInfo");
	// g_logger->logGameInfo(INFO,
	//                       "Enabled Text Filtering: " + std::to_string(config->getTextFiltering()),
	//                       "ClientInfo");
	// g_logger->logGameInfo(INFO,
	//                       "Allow Server Listings: " + std::to_string(config->getServerListings()),
	//                       "ClientInfo");
	packet.setReturnPacket(PACKET_OK);
	(void)server;
}
