#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>

void NetworkManager::workerThreadLoop() {
	while (!_shutdownFlag.load()) {
		Packet* packet = nullptr;

		if (_incomingPackets.waitAndPopTimeout(packet, std::chrono::milliseconds(100))) {
			if (packet == nullptr)
				break;
			try {
				std::cout << "Handling incoming data for player " << std::endl;
				packetRouter(packet, getServer(), &_outgoingPackets);
				if (packet->getReturnPacket() == PACKET_SEND) {
					_outgoingPackets.push(packet);
					packet = nullptr;
				} else if (packet->getReturnPacket() == PACKET_DISCONNECT) {
					Player* player = packet->getPlayer();
					if (player) {
						getServer().removePlayerFromAnyList(player);
						epoll_ctl(_epollFd, EPOLL_CTL_DEL, packet->getSocket(), nullptr);
						close(packet->getSocket());
						std::cout << "[Worker] Disconnected player socket " << packet->getSocket()
						          << std::endl;
					}
				}
			} catch (const std::exception& e) {
				std::cerr << "Error processing packet: " << e.what() << std::endl;
			}
		}
		if (packet != nullptr)
			delete packet;
	}
}
