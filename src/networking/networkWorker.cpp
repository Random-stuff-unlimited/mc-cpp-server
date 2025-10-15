#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>

void NetworkManager::workerThreadLoop() {
	while (!_shutdownFlag.load()) {
		Packet* packet = nullptr;

		if (_incomingPackets.waitAndPopTimeout(packet, std::chrono::milliseconds(100))) {
			if (packet == nullptr) break;
			try {

				// g_logger->logNetwork(INFO, "Handling incoming data for player", "Worker");
				packetRouter(packet, getServer());
				if (packet->getReturnPacket() == PACKET_SEND) {
					_outgoingPackets.push(packet);
					packet = nullptr;
				} else if (packet->getReturnPacket() == PACKET_DISCONNECT) {
					Player* player = packet->getPlayer();
					if (player) {
						getServer().removePlayerFromAnyList(player);
						epoll_ctl(_epollFd, EPOLL_CTL_DEL, packet->getSocket(), nullptr);
						close(packet->getSocket());
						// g_logger->logNetwork(INFO,
						//                      "Disconnected player socket " +
						//                              std::to_string(packet->getSocket()),
						//                      "Worker");
					}
				}
			} catch (const std::exception& e) {
				std::cerr << "Error processing packet: " << e.what() << std::endl;
			}
		}
		if (packet != nullptr) delete packet;
	}
}
