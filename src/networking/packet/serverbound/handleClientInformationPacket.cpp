#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "logger.hpp"

void handleClientInformationPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		g_logger->logNetwork(LogLevel::Error, "No player associated with Client Information packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	PlayerConfig* config = player->getPlayerConfig();
	if (!config) {
		g_logger->logNetwork(LogLevel::Error, "Player config is null for Client Information packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	g_logger->logNetwork(LogLevel::Info, "Processing Client Information packet for player: " + player->getPlayerName(), "Configuration");
	g_logger->logNetwork(LogLevel::Debug, "Packet size: " + std::to_string(packet.getSize()) + " bytes", "Configuration");
	g_logger->logNetwork(LogLevel::Debug, "Packet ID: 0x" + std::to_string(packet.getId()), "Configuration");

	try {
		// Log buffer state before reading
		g_logger->logNetwork(LogLevel::Debug, "Buffer size: " + std::to_string(packet.getData().getData().size()), "Configuration");
		
		// Read locale (string)
		g_logger->logNetwork(LogLevel::Debug, "Reading locale...", "Configuration");
		std::string locale = packet.getData().readString(16);
		config->setLocale(locale);
		g_logger->logNetwork(LogLevel::Info, "Locale: " + locale, "Configuration");
		
		// Read view distance (byte)
		g_logger->logNetwork(LogLevel::Debug, "Reading view distance...", "Configuration");
		uint8_t viewDistance = packet.getData().readByte();
		config->setViewDistance(viewDistance);
		g_logger->logNetwork(LogLevel::Info, "View distance: " + std::to_string(viewDistance), "Configuration");
		
		// Read chat mode (varint)
		g_logger->logNetwork(LogLevel::Debug, "Reading chat mode...", "Configuration");
		int32_t chatMode = packet.getData().readVarInt();
		config->setChatMode(chatMode);
		g_logger->logNetwork(LogLevel::Info, "Chat mode: " + std::to_string(chatMode), "Configuration");
		
		// Read chat colors (boolean as byte)
		g_logger->logNetwork(LogLevel::Debug, "Reading chat colors...", "Configuration");
		bool chatColors = packet.getData().readByte() != 0;
		config->setChatColors(chatColors);
		g_logger->logNetwork(LogLevel::Info, "Chat colors: " + std::string(chatColors ? "true" : "false"), "Configuration");
		
		// Read displayed skin parts (byte)
		g_logger->logNetwork(LogLevel::Debug, "Reading skin parts...", "Configuration");
		uint8_t skinParts = packet.getData().readByte();
		config->setDisplayedSkinParts(skinParts);
		g_logger->logNetwork(LogLevel::Info, "Skin parts: " + std::to_string(skinParts), "Configuration");
		
		// Read main hand (varint)
		g_logger->logNetwork(LogLevel::Debug, "Reading main hand...", "Configuration");
		int32_t mainHand = packet.getData().readVarInt();
		config->setMainHand(mainHand);
		g_logger->logNetwork(LogLevel::Info, "Main hand: " + std::to_string(mainHand), "Configuration");
		
		// Read enable text filtering (boolean as byte)
		g_logger->logNetwork(LogLevel::Debug, "Reading text filtering...", "Configuration");
		bool textFiltering = packet.getData().readByte() != 0;
		config->setTextFiltering(textFiltering);
		g_logger->logNetwork(LogLevel::Info, "Text filtering: " + std::string(textFiltering ? "true" : "false"), "Configuration");
		
		// Read allow server listings (boolean as byte)
		g_logger->logNetwork(LogLevel::Debug, "Reading server listings...", "Configuration");
		bool serverListings = packet.getData().readByte() != 0;
		config->setServerListings(serverListings);
		g_logger->logNetwork(LogLevel::Info, "Server listings: " + std::string(serverListings ? "true" : "false"), "Configuration");
		
		g_logger->logNetwork(LogLevel::Info, "Client Information processed successfully", "Configuration");
		packet.setReturnPacket(PACKET_OK);
		
	} catch (const std::exception& e) {
		g_logger->logNetwork(LogLevel::Error, "Error reading Client Information packet: " + std::string(e.what()), "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
	}

	(void)server;
}