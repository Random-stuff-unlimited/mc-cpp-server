#include "buffer.hpp"
#include "logger.hpp"
#include "network/packet.hpp"

#include <iostream>

void serverboundKnownPacksPacket(Packet& packet) {
	int size = packet.getData().readVarInt();

	std::cout << "Received " << size << " known packs." << std::endl;

	for (int i = 0; i < size; ++i) {
		std::string name	= packet.getData().readString(10000);
		std::string id		= packet.getData().readString(10000);
		std::string version = packet.getData().readString(10000);
		std::cout << "Pack " << i + 1 << ": " << name << " | " << id << " | " << version << std::endl;
	}
}
