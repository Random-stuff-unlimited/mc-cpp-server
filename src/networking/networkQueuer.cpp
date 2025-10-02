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
			break;
		}

		for (int i = 0; i < eventCount; i++) {
			int fd = events[i].data.fd;
			uint32_t eventFlags = events[i].events;

			if (fd == _serverSocket) {
				sockaddr_in client_addr{};
				socklen_t addr_len = sizeof(client_addr);
				int client_fd = accept(_serverSocket, (sockaddr*)&client_addr, &addr_len);
				if (client_fd != -1) {
					handleIncomingData(client_fd);
					epoll_event event;
					event.events = EPOLLIN;
					event.data.fd = client_fd;
					epoll_ctl(_epollFd, EPOLL_CTL_ADD, client_fd, &event);
				}
                continue;
			}

			auto it = getServer().getPlayerLst().find(fd);
			Player* p = nullptr;
			if (it != getServer().getPlayerLst().end())
			    p = it->second;

			if (!p)
				continue;

			if (eventFlags & EPOLLERR || eventFlags & EPOLLHUP) {
				getServer().removePlayer(p);
				epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, nullptr);
				close(fd);
				continue;
			}

			if (eventFlags & EPOLLIN) {
				try {
					handleIncomingData(p);
				} catch (const std::exception& e) {
					getServer().removePlayer(p);
					epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, nullptr);
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
	std::cout << "Handling incoming data for player " << std::endl;
	try {
		p = new Packet(connection);
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet 1: " << e.what() << std::endl;
	}
}

void NetworkManager::handleIncomingData(int socket) {
    Packet *p;
    std::cout << "Handling incoming data for socket " << socket << std::endl;
	try {
		p = new Packet(socket, getServer());
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet 2: " << e.what() << std::endl;
	}
}
