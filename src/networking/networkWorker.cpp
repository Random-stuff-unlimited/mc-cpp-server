#include "PacketIds.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <exception>
#include <memory>
#include <string>

// Network thread: a packet just decoded. The game state is only touched on the game thread, so Play packets
// (and entering Play) are handed to it, in order; the other states are handled here
void NetworkManager::dispatch(Packet* packet) {
	Player*		player = packet->getPlayer();
	PlayerState state  = player->getPlayerState();

	if (state == PlayerState::Play) {
		getServer().getTickLoop().post(packet);
		return;
	}
	if (state == PlayerState::Configuration && packet->getId() == PacketId::Configuration::Serverbound::FINISH_CONFIGURATION) {
		// The next packets are Play packets: they must follow this one to the game thread
		player->setPlayerState(PlayerState::Play);
		Server& server = getServer();
		server.getTickLoop().post([&server, packet] { server.enterGame(packet); });
		return;
	}

	std::unique_ptr<Packet> owned(packet);
	try {
		packetRouter(packet, getServer());
		if (packet->getReturnPacket() == PACKET_DISCONNECT) requestDisconnect(player);
	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Error processing packet: " + std::string(e.what()), "Network");
		requestDisconnect(player);
	}
}
