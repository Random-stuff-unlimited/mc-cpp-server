#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "network/packetRouter.hpp"
#include "player.hpp"

#include <chrono>
#include <exception>
#include <string>

void NetworkManager::workerThreadLoop(size_t index) {
	ThreadSafeQueue<Packet*>& queue = *_workerQueues[index];

	while (!_shutdownFlag.load()) {
		Packet* packet = nullptr;

		if (!queue.waitAndPopTimeout(packet, std::chrono::milliseconds(100))) continue;
		if (packet == nullptr) break;

		Player* player = packet->getPlayer();
		// Packets still queued for a player that is being disconnected are dropped
		if (player && !player->isDisconnected()) {
			try {
				packetRouter(packet, getServer());
				if (packet->getReturnPacket() == PACKET_DISCONNECT) {
					requestDisconnect(player);
				}
			} catch (const std::exception& e) {
				g_logger->logNetwork(ERROR, "Error processing packet: " + std::string(e.what()), "Worker");
				requestDisconnect(player);
			}
		}
		// Handlers never keep the incoming packet: anything sent was copied by sendPacket
		delete packet;
	}
}
