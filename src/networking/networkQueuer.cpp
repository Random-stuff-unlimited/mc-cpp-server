#include "networking.hpp"
#include "player.hpp"
#include "packet.hpp"
#include "server.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <exception>
#include <netinet/in.h>
#include <iostream>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>

void NetworkManager::receiverThreadLoop() {
	const int MaxEvent = 256;
	epoll_event events[MaxEvent];

	while (!_shutdownFlag.load()) {
		int eventCount = epoll_wait(_epollFd, events, MaxEvent, 50);

		if (eventCount == -1) {
			if (errno == EINTR) continue;
			// Log error and break on other errors
			break;
		}

		for (int i = 0; i < eventCount; i++) {
			int fd = events[i].data.fd;
			uint32_t eventFlags = events[i].events;

			Player* p;
			for (unsigned int i = 0; i < getServer().getPlayerLst().size(); i++) {
				if (fd == getServer().getPlayerLst()[i].getSocketFd()) {
					p = &getServer().getPlayerLst()[i];
					break ;
				}
			}
			// Handle different types of events
			if (eventFlags & EPOLLERR || eventFlags & EPOLLHUP) {
				// Connection error or hangup - remove player
				{
					getServer().removePlayer(p);
				}
				close(fd);
				continue;
			}

			if (eventFlags & EPOLLIN) {
				// Data available to read
				try {
					if (!p) {
						sockaddr_in client_addr{};
              			socklen_t addr_len = sizeof(client_addr);
		                int client_fd = accept(_epollFd, (sockaddr*)&client_addr, &addr_len);
						handleIncomingData(client_fd);
					} else {
						handleIncomingData(p);
					}
				} catch (const std::exception& e) {
					// Handle parsing errors - disconnect player
					{
						getServer().removePlayer(p);
					}
					close(fd);
				}
			}
		}
	}
}

void NetworkManager::senderThreadLoop() {
	while (!_shutdownFlag.load()) {
		Packet* p = nullptr;

		while (_outgoingPackets.tryPop(p)) {
			try {
				send(p->getSocket(),static_cast<const void *>(&p->getData().getData()), p->getSize(), MSG_NOSIGNAL); // May break ||SEND
				delete p;
			} catch (const std::exception& e) {
				std::cerr << "[Network Manager] Failed to send packet: " << e.what() << std::endl;
				delete p;
			}
		}
		usleep(1000); // 1ms
	}
}

void NetworkManager::enqueueOutgoingPacket(Packet* p) {
	_outgoingPackets.push(p);
}

void NetworkManager::handleIncomingData(Player* connection) {
	Packet *p;

	try {
		p = new Packet(connection, getServer());
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet: " << e.what() << std::endl;
	}
}

void NetworkManager::handleIncomingData(int socket) {
	Packet *p;

	try {
		p = new Packet(socket, getServer());
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet: " << e.what() << std::endl;
	}
}

void NetworkManager::workerThreadLoop() {

}
