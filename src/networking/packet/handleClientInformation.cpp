#include "logger.hpp"
#include "packet.hpp"
#include "networking.hpp"

#include <string>

void handleClientInformation(Packet& packet) {
    packet.getPlayer()->setLocale(packet.getData().readString(16));
    packet.getPlayer()->setViewDistance(packet.getData().readByte());
    packet.getPlayer()->setChatMode(packet.getData().readVarInt());
    packet.getPlayer()->setChatColors(packet.getData().readByte() != 0);
    packet.getPlayer()->setDisplayedSkinParts(packet.getData().readByte());
    packet.getPlayer()->setMainHand(packet.getData().readVarInt());
    packet.getPlayer()->setEnabledTextFiltering(packet.getData().readByte() != 0);
    packet.getPlayer()->setAllowServerListings(packet.getData().readByte() != 0);

    g_logger->logGameInfo(INFO, "Received client settings:", "ClientInfo");
    g_logger->logGameInfo(INFO, "Local: " + packet.getPlayer()->getLocale(), "ClientInfo");
    g_logger->logGameInfo(INFO, "View Distance: " + std::to_string(packet.getPlayer()->getViewDistance()), "ClientInfo");
    g_logger->logGameInfo(INFO, "Chat Mode: " + std::to_string(packet.getPlayer()->getChatMode()), "ClientInfo");
    g_logger->logGameInfo(INFO, "Chat Colors: " + std::to_string(packet.getPlayer()->getChatColorsStatus()), "ClientInfo");
    g_logger->logGameInfo(INFO, "Displayed Skin Parts: " + std::to_string(packet.getPlayer()->getDisplayedSkinParts()), "ClientInfo");
    g_logger->logGameInfo(INFO, "Main Hand: " + std::to_string(packet.getPlayer()->getMainHand()), "ClientInfo");
    g_logger->logGameInfo(INFO, "Enabled Text Filtering: " + std::to_string(packet.getPlayer()->getTextFilteringStatus()), "ClientInfo");
    g_logger->logGameInfo(INFO, "Allow Server Listings: " + std::to_string(packet.getPlayer()->getServerListingsStatus()), "ClientInfo");
}
