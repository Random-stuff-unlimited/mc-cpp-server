#include "network/networking.hpp"
#include "network/server.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

NetworkManager::NetworkManager(size_t workerCount, Server& s)
	: _incomingPackets(), _outgoingPackets(), _workerThreads(), _shutdownFlag(false),
	  _receiverThread(), _senderThread(), _receiverThreadInit(0), _senderThreadInit(0), _server(s),
	  _epollFd(-1), _serverSocket(-1) {
	_workerThreads.reserve(workerCount);

	setupEpoll();
	start();
}

void NetworkManager::setupEpoll() {
	_epollFd = epoll_create(EPOLL_CLOEXEC);
	if (_epollFd == -1) {
		throw std::runtime_error("Failed to create epoll file descriptor");
	}
}

void NetworkManager::startThreads() {
	try {
		_shutdownFlag = false;

		if (!_receiverThreadInit) {
			_receiverThread		= std::thread(&NetworkManager::receiverThreadLoop, this);
			_receiverThreadInit = 1;
		}

		if (!_senderThreadInit) {
			_senderThread	  = std::thread(&NetworkManager::senderThreadLoop, this);
			_senderThreadInit = 1;
		}
		size_t workerCount = _workerThreads.capacity();
		for (size_t i = 0; i < workerCount; i++) {
			_workerThreads.emplace_back(&NetworkManager::workerThreadLoop, this);
		}

	} catch (const std::exception& e) {
		stopThreads();
		throw std::runtime_error(std::string("Thread start error: ") + e.what());
	}
}

void NetworkManager::stopThreads() {
	_shutdownFlag = true;

	_incomingPackets.push(nullptr);
	_outgoingPackets.push(nullptr);

	for (auto& worker : _workerThreads) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	_workerThreads.clear();

	if (_receiverThread.joinable()) {
		_receiverThread.join();
		_receiverThreadInit = 0;
	}

	// Join sender thread
	if (_senderThread.joinable()) {
		_senderThread.join();
		_senderThreadInit = 0;
	}
}

void NetworkManager::start() {
	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket == -1) {
		throw std::runtime_error("Failed to create server socket");
	}

	int flags = fcntl(_serverSocket, F_GETFL, 0);
	fcntl(_serverSocket, F_SETFL, flags | O_NONBLOCK);

	int opt = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		close(_serverSocket);
		throw std::runtime_error("Failed to set socket options");
	}

	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port	  = htons(getServer().getServerPort());

	if (strcmp(getServer().getServerAddr().c_str(), "0.0.0.0") == 0) {
		serverAddr.sin_addr.s_addr = INADDR_ANY;
	} else {
		if (inet_aton(getServer().getServerAddr().c_str(), &serverAddr.sin_addr) == 0) {
			close(_serverSocket);
			throw std::runtime_error("Invalid IP address");
		}
	}

	if (bind(_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to bind socket to " +
								 std::string(getServer().getServerAddr()) + ":" +
								 std::to_string(getServer().getServerPort()));
	}

	if (listen(_serverSocket, SOMAXCONN) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to listen on socket");
	}
	struct epoll_event event;
	event.events  = EPOLLIN | EPOLLET; // Edge-triggered for efficiency
	event.data.fd = _serverSocket;

	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverSocket, &event) == -1) {
		close(_serverSocket);
		throw std::runtime_error("Failed to add server socket to epoll");
	}
}
