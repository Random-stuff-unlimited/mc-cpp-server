#include "networking.hpp"
#include "packet.hpp"
#include <sys/epoll.h>
#include <shared_mutex>
#include <mutex>
#include <unistd.h>
#include <errno.h>

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
			
			// Find the player connection associated with this file descriptor
			std::shared_ptr<Player> player = nullptr;
			{
				std::shared_lock<std::mutex> lock(_connectionsMutex);
				auto it = _connections.find(static_cast<uint32_t>(fd));
				if (it != _connections.end()) {
					player = it->second;
				}
			}
			// Handle different types of events
			if (eventFlags & EPOLLERR || eventFlags & EPOLLHUP) {
				// Connection error or hangup - remove player
				{
					std::unique_lock<std::mutex> lock(_connectionsMutex);
					_connections.erase(static_cast<uint32_t>(fd));
				}
				close(fd);
				continue;
			}
			
			if (eventFlags & EPOLLIN) {
				// Data available to read
				try {
					if (!player) {
						sockaddr_in client_addr{};
              			socklen_t addr_len = sizeof(client_addr);
		                int client_fd = accept(_epollFd, (sockaddr*)&client_addr, &addr_len);
						handleIncomingData(client_fd);
					} else {
						handleIncomingData(player);
					}
				} catch (const std::exception& e) {
					// Handle parsing errors - disconnect player
					{
						std::unique_lock<std::mutex> lock(_connectionsMutex);
						_connections.erase(static_cast<uint32_t>(fd));
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
				std::shared_lock<std::mutex> lock(_connectionsMutex);
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

void NetworkManager::handleIncomingData(std::shared_ptr<Player> connection) {
	Packet *p;

	try {
		p = new Packet(connection);
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet: " << e.what() << std::endl;
	}
}

void NetworkManager::handleIncomingData(int socket) {
	Packet *p;

	try {
		p = new Packet(socket);
		_incomingPackets.push(p);
	} catch (const std::exception& e) {
		std::cerr << "[Network Manager] Failed to receive packet: " << e.what() << std::endl;
	}
}