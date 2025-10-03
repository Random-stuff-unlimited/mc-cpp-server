#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <arpa/inet.h>
#include <cstdint>
#include <errno.h>
#include <exception>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

void NetworkManager::receiverThreadLoop() {
	const int MaxEvent = 256;
	epoll_event events[MaxEvent];

	while (!_shutdownFlag.load()) {
		int eventCount = epoll_wait(_epollFd, events, MaxEvent, 50);

		if (eventCount == -1) {
			if (errno == EINTR)
				continue;
			break;
		}

		for (int i = 0; i < eventCount; i++) {
			int fd              = events[i].data.fd;
			uint32_t eventFlags = events[i].events;

			if (fd == _serverSocket) {
				sockaddr_in client_addr{};
				socklen_t addr_len = sizeof(client_addr);
				int client_fd      = accept(_serverSocket, (sockaddr*)&client_addr, &addr_len);
				if (client_fd != -1) {
					std::cout << "[Network Manager] New connection accepted on socket " << client_fd
					          << std::endl;
					epoll_event event;
					event.events  = EPOLLIN;
					event.data.fd = client_fd;
					if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
						std::cerr << "[Network Manager] Failed to add new client socket to epoll"
						          << std::endl;
						close(client_fd);
					}
				}
				continue;
			}

			auto it   = getServer().getPlayerLst().find(fd);
			Player* p = nullptr;
			if (it != getServer().getPlayerLst().end()) {
				p = it->second;
			} else {
				auto temp_it = getServer().getTempPlayerLst().find(fd);
				if (temp_it != getServer().getTempPlayerLst().end())
					p = temp_it->second;
			}

			if (eventFlags & EPOLLERR || eventFlags & EPOLLHUP) {
				if (p)
					getServer().removePlayerFromAnyList(p);
				epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, nullptr);
				close(fd);
				continue;
			}

			if (eventFlags & EPOLLIN) {
				try {
					if (p)
						handleIncomingData(p);
					else
						handleIncomingData(fd);
				} catch (const std::exception& e) {
					if (p)
						getServer().removePlayerFromAnyList(p);
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
			if (p == nullptr)
				break;

			try {
				std::cout << "Sending packet to player " << std::endl;
				send(p->getSocket(), p->getData().getData().data(), p->getSize(), MSG_NOSIGNAL);
				if (p->getPlayer() && p->getPlayer()->getPlayerState() == PlayerState::None) {
					std::cout << "[Network Manager] Closing status connection after response"
					          << std::endl;
					getServer().removePlayerFromAnyList(p->getPlayer());
					epoll_ctl(_epollFd, EPOLL_CTL_DEL, p->getSocket(), nullptr);
					close(p->getSocket());
				}
			} catch (const std::exception& e) {
				std::cerr << "[Network Manager] Failed to send packet: " << e.what() << std::endl;
			}
			delete p;
			p = nullptr;
			std::cout << "Packet sent" << std::endl;
		}
		usleep(1000);
	}
}

void NetworkManager::enqueueOutgoingPacket(Packet* p) {
	_outgoingPackets.push(p);
}

void NetworkManager::handleIncomingData(Player* connection) {
	Packet* p;
	std::cout << "Handling incoming data for player " << std::endl;
	try {
		p = new Packet(connection);
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet 1: " << e.what() << std::endl;
		throw;
	}
}

void NetworkManager::handleIncomingData(int socket) {
	Packet* p;
	std::cout << "Handling incoming data for socket " << socket << std::endl;
	try {
		p = new Packet(socket, getServer());
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet 2: " << e.what() << std::endl;
		throw;
	}
}
