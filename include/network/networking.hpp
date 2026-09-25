#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include "../player.hpp"
#include "packet.hpp"

// Forward declaration to avoid circular dependency
class Server;
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <vector>

// The sockets, spread over a few network threads. Each thread has its own epoll and owns its connections: it reads
// them, decodes and decompresses the packets, and writes them. Nothing about a socket is shared between threads.
//
// Incoming packets are handled right away on the network thread for the Handshake, Status, Login and Configuration
// states. Play packets go to the game thread (TickLoop).
//
// Outgoing packets are appended to the player's output buffer (PlayerOutput) by whatever thread sends them, and
// written when that thread calls flush(): the game thread flushes once per tick, so a player gets at most one
// write per tick however many packets it receives.
class NetworkManager {
  public:
	NetworkManager(size_t threadCount, Server& server);
	~NetworkManager();

	void startThreads();
	void stopThreads();

	Server& getServer() { return _server; }

	// Called by Packet::send after appending to an empty output buffer: the calling thread's next flush() writes it
	static void markForFlush(const std::shared_ptr<Player>& player);
	// Hands the output of the players marked by this thread to their network threads
	void flush();
	// Stops reading from the player, removes it from the game, and closes its socket once the packets already queued
	// for it are sent. Safe from any thread
	void requestDisconnect(Player* player);

  private:
	struct Connection {
		std::shared_ptr<Player>				  player;
		std::vector<uint8_t>				  in; // Received bytes not yet forming a full packet, from inStart
		size_t								  inStart = 0;
		std::vector<uint8_t>				  out; // Bytes the socket didn't take yet, from outOffset
		size_t								  outOffset = 0;
		bool								  waitingWritable = false; // EPOLLOUT armed
		std::chrono::steady_clock::time_point lastProgress;
	};
	struct NetworkThread {
		int										 index;
		int										 epollFd = -1;
		int										 wakeFd	 = -1; // eventfd: written when toFlush gets its first player
		std::thread								 thread;
		std::mutex								 flushMutex;
		std::vector<std::shared_ptr<Player>>	 toFlush;
		std::unordered_map<int, Connection>		 connections; // By socket, only used by this thread
	};

	static constexpr size_t				  MAX_PENDING_OUTPUT = 16 * 1024 * 1024;
	static constexpr std::chrono::seconds PENDING_OUTPUT_TIMEOUT{10};

	Server&										_server;
	int											_serverSocket = -1;
	std::atomic<bool>							_shutdownFlag{false};
	std::vector<std::unique_ptr<NetworkThread>> _threads;

	void listen();
	void threadLoop(NetworkThread& thread);
	void acceptConnections(NetworkThread& thread);
	// Returns false when the connection must be closed
	bool readFromSocket(NetworkThread& thread, Connection& connection);
	bool extractPackets(Connection& connection);
	void dispatch(Packet* packet);
	void writeQueued(NetworkThread& thread);
	// Returns false when the connection must be closed
	bool writeConnection(NetworkThread& thread, int fd, Connection& connection);
	void watchWritable(NetworkThread& thread, int fd, Connection& connection, bool writable);
	void checkStalledOutputs(NetworkThread& thread);
	void closeConnection(NetworkThread& thread, int fd);
	void queueWrite(const std::shared_ptr<Player>& player);
};

#endif
