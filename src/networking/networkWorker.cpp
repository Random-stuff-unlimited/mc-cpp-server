#include <chrono>
#include <iostream>
#include <exception>
#include "packet.hpp"
#include "networking.hpp"

void NetworkManager::workerThreadLoop() {
    while (!_shutdownFlag.load()) {
        Packet* packet = nullptr;

        if (_incomingPackets.waitAndPopTimeout(packet, std::chrono::milliseconds(100))) {
            if (packet == nullptr) {
                break ;
            }

            try {
                packetRouter(*packet, getServer());
                if (packet->getReturnPacket() == 1) {
                    _outgoingPackets.push(packet);
                } else {
                    delete packet;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error processing packet: " << e.what() << std::endl;
            }
        }

        delete packet;
    }
}
