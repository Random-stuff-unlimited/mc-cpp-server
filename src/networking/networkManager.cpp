#include "logger.hpp"
#include "network/networking.hpp"
#include "network/server.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
	// Players whose output this thread appended to since its last flush()
	thread_local std::vector<std::shared_ptr<Player>> t_toFlush;
} // namespace

NetworkManager::NetworkManager(size_t threadCount, Server& server) : _server(server) {
	listen();
	for (size_t i = 0; i < std::max<size_t>(1, threadCount); i++) {
		auto thread		= std::make_unique<NetworkThread>();
		thread->index	= static_cast<int>(i);
		thread->epollFd = epoll_create1(EPOLL_CLOEXEC);
		thread->wakeFd	= eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (thread->epollFd == -1 || thread->wakeFd == -1) throw std::runtime_error("Failed to create epoll or eventfd");

		epoll_event wake{};
		wake.events	 = EPOLLIN;
		wake.data.fd = thread->wakeFd;
		// Every thread accepts: EPOLLEXCLUSIVE wakes only one of them per new connection
		epoll_event accept{};
		accept.events  = EPOLLIN | EPOLLEXCLUSIVE;
		accept.data.fd = _serverSocket;
		if (epoll_ctl(thread->epollFd, EPOLL_CTL_ADD, thread->wakeFd, &wake) == -1 ||
			epoll_ctl(thread->epollFd, EPOLL_CTL_ADD, _serverSocket, &accept) == -1) {
			throw std::runtime_error("Failed to set up epoll");
		}
		_threads.push_back(std::move(thread));
	}
}

NetworkManager::~NetworkManager() {
	stopThreads();
	for (auto& thread : _threads) {
		for (auto& [fd, connection] : thread->connections) close(fd);
		close(thread->epollFd);
		close(thread->wakeFd);
	}
	if (_serverSocket != -1) close(_serverSocket);
}

void NetworkManager::listen() {
	_serverSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (_serverSocket == -1) throw std::runtime_error("Failed to create server socket");

	int opt = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to set socket options");
	}

	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port	  = htons(getServer().getConfig().getServerPort());
	if (getServer().getConfig().getServerAddress() == "0.0.0.0") {
		serverAddr.sin_addr.s_addr = INADDR_ANY;
	} else if (inet_aton(getServer().getConfig().getServerAddress().c_str(), &serverAddr.sin_addr) == 0) {
		close(_serverSocket);
		throw std::runtime_error("Invalid IP address");
	}

	if (bind(_serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to bind socket to " + getServer().getConfig().getServerAddress() + ":" +
								 std::to_string(getServer().getConfig().getServerPort()));
	}
	if (::listen(_serverSocket, SOMAXCONN) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to listen on socket");
	}
}

void NetworkManager::startThreads() {
	_shutdownFlag = false;
	for (auto& thread : _threads) {
		if (!thread->thread.joinable()) thread->thread = std::thread(&NetworkManager::threadLoop, this, std::ref(*thread));
	}
}

void NetworkManager::stopThreads() {
	_shutdownFlag = true;
	for (auto& thread : _threads) {
		uint64_t one = 1;
		(void)!write(thread->wakeFd, &one, sizeof(one));
	}
	for (auto& thread : _threads) {
		if (thread->thread.joinable()) thread->thread.join();
	}
}

void NetworkManager::acceptConnections(NetworkThread& thread) {
	while (true) {
		sockaddr_in clientAddr{};
		socklen_t	addrLen	 = sizeof(clientAddr);
		int			clientFd = accept4(_serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (clientFd == -1) {
			if (errno == EINTR) continue;
			return; // EAGAIN: another thread may have taken it
		}
		// Packets are grouped by tick already: no need for Nagle's delay
		int noDelay = 1;
		setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));

		std::shared_ptr<Player> player;
		try {
			player = getServer().addTempPlayer("None", PlayerState::Handshake, clientFd);
		} catch (const std::exception& e) {
			g_logger->logNetwork(ERROR, "Failed to create player for new connection: " + std::string(e.what()), "Network Manager");
			close(clientFd);
			continue;
		}
		player->setNetworkThread(thread.index);

		epoll_event event{};
		event.events  = EPOLLIN;
		event.data.fd = clientFd;
		if (epoll_ctl(thread.epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1) {
			g_logger->logNetwork(ERROR, "Failed to add new client socket to epoll", "Network Manager");
			getServer().removePlayerFromAnyList(player.get());
			close(clientFd);
			continue;
		}
		Connection& connection	 = thread.connections[clientFd];
		connection				 = Connection();
		connection.player		 = std::move(player);
		connection.lastProgress = std::chrono::steady_clock::now();
	}
}

void NetworkManager::markForFlush(const std::shared_ptr<Player>& player) { t_toFlush.push_back(player); }

void NetworkManager::flush() {
	if (t_toFlush.empty()) return;
	for (const auto& player : t_toFlush) queueWrite(player);
	t_toFlush.clear();
}

// Hands the player to its network thread, which writes its output
void NetworkManager::queueWrite(const std::shared_ptr<Player>& player) {
	if (player->output().flushQueued.exchange(true)) return; // Already queued
	NetworkThread& thread = *_threads[player->getNetworkThread()];
	bool		   wake;
	{
		std::lock_guard<std::mutex> lock(thread.flushMutex);
		wake = thread.toFlush.empty();
		thread.toFlush.push_back(player);
	}
	if (wake) {
		uint64_t one = 1;
		(void)!write(thread.wakeFd, &one, sizeof(one));
	}
}

void NetworkManager::requestDisconnect(Player* player) {
	if (!player || !player->markDisconnected()) return;
	std::shared_ptr<Player> owned = player->shared_from_this();

	getServer().removePlayerFromAnyList(player);
	// Its chunks and its entity belong to the game thread
	Server& server = getServer();
	server.getTickLoop().post([&server, owned] { server.leaveGame(owned.get()); });

	// Only the network thread of the socket closes it, after writing what was queued before
	{
		std::lock_guard<std::mutex> lock(player->output().mutex);
		player->output().closing = true;
	}
	queueWrite(owned);
}
