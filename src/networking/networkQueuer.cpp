#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <arpa/inet.h>
#include <cstdint>
#include <errno.h>
#include <exception>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

void NetworkManager::receiverThreadLoop() {
	const int	MaxEvent = 256;
	epoll_event events[MaxEvent];

	while (!_shutdownFlag.load()) {
		int eventCount = epoll_wait(_epollFd, events, MaxEvent, 50);

		if (eventCount == -1) {
			if (errno == EINTR) continue;
			break;
		}

		for (int i = 0; i < eventCount; i++) {
			int		 fd			= events[i].data.fd;
			uint32_t eventFlags = events[i].events;

			if (fd == _serverSocket) {
				sockaddr_in client_addr{};
				socklen_t	addr_len  = sizeof(client_addr);
				int			client_fd = accept(_serverSocket, (sockaddr*)&client_addr, &addr_len);
				if (client_fd != -1) {
					// g_logger->logNetwork(INFO, "New connection accepted on socket " +
					// std::to_string(client_fd), "Network Manager");
					epoll_event event;
					event.events  = EPOLLIN;
					event.data.fd = client_fd;
					if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
						std::cerr << "[Network Manager] Failed to add new client socket to epoll" << std::endl;
						close(client_fd);
					}
				}
				continue;
			}

			auto	it = getServer().getPlayerLst().find(fd);
			Player* p  = nullptr;
			if (it != getServer().getPlayerLst().end()) {
				p = it->second;
			} else {
				auto temp_it = getServer().getTempPlayerLst().find(fd);
				if (temp_it != getServer().getTempPlayerLst().end()) p = temp_it->second;
			}

			if (eventFlags & EPOLLERR || eventFlags & EPOLLHUP) {
				if (p) getServer().removePlayerFromAnyList(p);
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
					if (p) getServer().removePlayerFromAnyList(p);
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
			if (p == nullptr) break;

			try {
				// g_logger->logNetwork(INFO, "Sending packet to player", "Network Manager");
				send(p->getSocket(), p->getData().getData().data(), p->getSize(), MSG_NOSIGNAL);
				if (p->getPlayer() && p->getPlayer()->getPlayerState() == PlayerState::None) {
					// g_logger->logNetwork(INFO, "Closing status connection after response",
					// "Network Manager");
					getServer().removePlayerFromAnyList(p->getPlayer());
					epoll_ctl(_epollFd, EPOLL_CTL_DEL, p->getSocket(), nullptr);
					close(p->getSocket());
				}
			} catch (const std::exception& e) {
				std::cerr << "[Network Manager] Failed to send packet: " << e.what() << std::endl;
			}
			delete p;
			p = nullptr;
			// g_logger->logNetwork(INFO, "Packet sent", "Network Manager");
		}
		usleep(1000);
	}
}

void NetworkManager::enqueueOutgoingPacket(Packet* p) { _outgoingPackets.push(p); }

void NetworkManager::handleIncomingData(Player* connection) {
	Packet* p;
	// g_logger->logNetwork(INFO, "Handling incoming data for player", "Network Manager");
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
	// g_logger->logNetwork(INFO, "Handling incoming data for socket " + std::to_string(socket),
	// "Network Manager");
	try {
		p = new Packet(socket, getServer());
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet 2: " << e.what() << std::endl;
		throw;
	}
}
