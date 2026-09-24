#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include "../lib/UUID.hpp"
#include "../player.hpp"
#include "packet.hpp"

// Forward declaration to avoid circular dependency
class Server;
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <vector>

template <typename T> class ThreadSafeQueue {
  private:
	std::queue<T>			_queue;
	mutable std::mutex		_mutex;
	std::condition_variable _condition;

  public:
	void push(T item) {
		std::lock_guard<std::mutex> lock(_mutex);
		_queue.push(std::move(item));
		_condition.notify_one();
	}

	bool tryPop(T& item) {
		std::lock_guard<std::mutex> lock(_mutex);
		if (_queue.empty()) return false;

		item = std::move(_queue.front());
		_queue.pop();
		return true;
	}

	bool waitAndPopTimeout(T& item, const std::chrono::milliseconds& timeout) {
		std::unique_lock<std::mutex> lock(_mutex);

		if (_condition.wait_for(lock, timeout, [this] { return !_queue.empty(); })) {
			item = std::move(_queue.front());
			_queue.pop();
			return true;
		}
		return false;
	}

	void waitAndPop(T& item) {
		std::unique_lock<std::mutex> lock(_mutex);
		_condition.wait(lock, [this] { return !_queue.empty(); });

		item = std::move(_queue.front());
		_queue.pop();
	}

	size_t size() const {
		std::lock_guard<std::mutex> lock(_mutex);
		return _queue.size();
	}
};

class NetworkManager {
  private:
	// One queue per worker: a connection always goes to the same worker, so its packets are handled in order
	std::vector<std::unique_ptr<ThreadSafeQueue<Packet*>>> _workerQueues;
	ThreadSafeQueue<Packet*>							   _outgoingPackets;

	// Bytes received but not yet forming a full packet, per socket. Only used by the receiver thread
	std::unordered_map<int, std::vector<uint8_t>> _recvBuffers;

	// Bytes a client couldn't take yet. Only used by the sender thread
	struct PendingOutput {
		std::shared_ptr<Player>				  player;
		std::vector<uint8_t>				  data;
		size_t								  offset		   = 0;
		bool								  closeWhenFlushed = false;
		std::chrono::steady_clock::time_point lastProgress;
	};
	static constexpr size_t					  MAX_PENDING_OUTPUT	 = 16 * 1024 * 1024;
	static constexpr std::chrono::seconds	  PENDING_OUTPUT_TIMEOUT{10};
	std::unordered_map<Player*, PendingOutput> _pendingOutput;

	std::vector<std::thread> _workerThreads;
	std::atomic<bool>		 _shutdownFlag;
	std::thread				 _receiverThread;
	std::thread				 _senderThread;
	char					 _receiverThreadInit;
	char					 _senderThreadInit;
	Server&					 _server;
	int						 _epollFd;
	int						 _serverSocket;
	size_t					 _workerCount;

  public:
	NetworkManager(size_t  worker_count,
				   Server& s); // Could use std::thread::hardware_concurrency() for the worker size;
	~NetworkManager() {
		if (_epollFd != -1) {
			close(_epollFd);
		}
	}

	void start();
	void startThreads();
	void stopThreads();
	void shutdown();

	void					  addPlayerConnection(std::shared_ptr<Player> connection);
	void					  removePlayerConnection(UUID id);
	ThreadSafeQueue<Packet*>* getOutgoingQueue() { return &_outgoingPackets; }

	Server& getServer() { return _server; }

	void enqueueOutgoingPacket(Packet* p);
	// Stops reading from the player and closes its socket once the packets already queued for it are sent. Safe from any thread
	void requestDisconnect(Player* player);

  private:
	void receiverThreadLoop();
	void senderThreadLoop();
	void workerThreadLoop(size_t index);

	void setupEpoll();
	void acceptConnections();
	bool readFromSocket(const std::shared_ptr<Player>& player);
	bool extractPackets(const std::shared_ptr<Player>& player);
	void queueOutput(Packet* p);
	void flushPendingOutput();
	bool sendAvailable(int socket, const uint8_t* data, size_t size, size_t& sent);
	bool detachPlayer(Player* player);
	void closePlayerSocket(Player* player);
};

#endif
