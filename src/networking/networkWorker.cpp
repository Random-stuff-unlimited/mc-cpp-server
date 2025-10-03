#include "networking.hpp"
#include "packet.hpp"
#include "server.hpp"

#include <chrono>
#include <exception>
#include <iostream>

void NetworkManager::workerThreadLoop() {
	while (!_shutdownFlag.load()) {
		Packet* packet = nullptr;

		if (_incomingPackets.waitAndPopTimeout(packet, std::chrono::milliseconds(100))) {
			if (packet == nullptr) {
				break;
			}

			try {
				std::cout << "Handling incoming data for player " << std::endl;
				packetRouter(*packet, getServer());
				if (packet->getReturnPacket() == 1) {
					_outgoingPackets.push(packet);
					// Don't delete packet here - it will be deleted by sender thread
					packet = nullptr;
				}
			} catch (const std::exception& e) {
				std::cerr << "Error processing packet: " << e.what() << std::endl;
			}
		}

		// Only delete if packet wasn't moved to outgoing queue
		if (packet != nullptr) {
			delete packet;
		}
	}
}
