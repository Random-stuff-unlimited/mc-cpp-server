#include "logger.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>

void NetworkManager::workerThreadLoop() {
	while (!_shutdownFlag.load()) {
		Packet* packet = nullptr;

		if (_incomingPackets.waitAndPopTimeout(packet, std::chrono::milliseconds(100))) {
			if (packet == nullptr)
				break;
			try {
				g_logger->logNetwork(INFO, "Handling incoming data for player", "NetworkWorker");
				packetRouter(*packet, getServer());
				if (packet->getReturnPacket() == PACKET_SEND) {
					_outgoingPackets.push(packet);
					packet = nullptr;
				} else if (packet->getReturnPacket() == PACKET_DISCONNECT) {
					Player* player = packet->getPlayer();
					if (player) {
						getServer().removePlayerFromAnyList(player);
						epoll_ctl(_epollFd, EPOLL_CTL_DEL, packet->getSocket(), nullptr);
						close(packet->getSocket());
						g_logger->logNetwork(INFO,
						                     "Disconnected player socket " +
						                             std::to_string(packet->getSocket()),
						                     "NetworkWorker");
					}
				}
			} catch (const std::exception& e) {
				g_logger->logNetwork(ERROR,
				                     "Error processing packet: " + std::string(e.what()),
				                     "NetworkWorker");
			}
		}
		if (packet != nullptr)
			delete packet;
	}
}
