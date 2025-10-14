#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include "../player.hpp"
#include "lib/UUID.hpp"
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
	ThreadSafeQueue<Packet*> _incomingPackets;
	ThreadSafeQueue<Packet*> _outgoingPackets;

	std::vector<std::thread> _workerThreads;
	std::atomic<bool>		 _shutdownFlag;
	std::thread				 _receiverThread;
	std::thread				 _senderThread;
	char					 _receiverThreadInit;
	char					 _senderThreadInit;
	Server&					 _server;
	int						 _epollFd;
	int						 _serverSocket;

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

  private:
	void receiverThreadLoop();
	void senderThreadLoop();
	void workerThreadLoop();

	void setupEpoll();
	void handleIncomingData(Player* connection);
	void handleIncomingData(int socket);
};

void packetRouter(Packet* packet, Server& server);
void handleHandshakePacket(Packet& packet, Server& server);
void handleStatusPacket(Packet& packet, Server& server);
void handlePingPacket(Packet& packet, Server& server);
void handleClientInformation(Packet& packet, Server& server);
void handleLoginStartPacket(Packet& packet, Server& server);
void handleLoginAcknowledged(Packet& packet, Server& server);
void handleCookieRequest(Packet& packet, Server& server);
void handleFinishConfiguration(Packet& packet, Server& server);
void handleAcknowledgeFinishConfiguration(Packet& packet, Server& server);
void writePlayPacket(Packet& packet, Server& server);
void writeSetCenterPacket(Packet& packet, Server& server);

// Chunk batch functions
void sendChunkBatchStart(Packet& packet, Server& server);
void sendChunkBatchFinished(Packet& packet, Server& server, int batchSize);
void sendChunkBatchSequence(Packet& packet, Server& server);

// Chunk data functions
void sendChunkData(Packet& packet, Server& server, int chunkX, int chunkZ);
void sendPlayerPositionAndLook(Packet& packet, Server& server);
void sendSpawnPosition(Packet& packet, Server& server);

// Spawn sequence functions
void sendPlayerAbilities(Packet& packet, Server& server);
void sendSetHealth(Packet& packet, Server& server);
void sendSetExperience(Packet& packet, Server& server);
void sendUpdateTime(Packet& packet, Server& server);
void sendSetHeldItem(Packet& packet, Server& server);
void handleConfirmTeleportation(Packet& packet, Server& server);
void completeSpawnSequence(Packet& packet, Server& server);
void sendDisconnectPacket(Packet* packet, const std::string& reason, Server& server);

Buffer generateEmptyChunkSections();
void   writeLightData(Buffer& buf, const World::ChunkData& chunkData);
void   writeActualLightData(Buffer& buf, const World::ChunkData& chunkData);
void   writeEmptyLightData(Buffer& buf);

void clientboundKnownPacks(Packet& packet);
void serverboundKnownPacks(Packet& packet);

void gameEventPacket(Packet& packet, Server& server);

#endif
