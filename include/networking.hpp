#ifndef NETWORKING_HPP
# define NETWORKING_HPP

# include "packet.hpp"
# include "player.hpp"
# include <queue>
# include <thread>
# include <mutex>
# include <atomic>

class NetworkManager {
	private:
		std::mutex _connectionsMutex;
		
		ThreadSafeQueue<Packet> _incomingPackets;
		ThreadSafeQueue<Packet> _outgoingPackets;

		std::thread _receiverThread;
		std::thread _senderThread;
		std::vector<std::thread> _workerThreads;
		std::atomic<bool> _shutdownFlag;

		int _epollFd;
	public:
		NetworkManager(size_t worker_count); // Could use std::thread::hardware_concurrency() for the worker size;
		~NetworkManager();


		void start();
		void shutdown();

		void addPlayerConnection(std::shared_ptr<Player> connection);
		//void removePlayerConnection(UUID id);

		void enqueueOutgoingPacket(Packet p);
	private:
		void receiverThreadLoop();
		void senderThreadLoop();
		void workerThreadLoop();

		void setupEpoll();
		void handleIncomingData(std::shared_ptr<Player> connection);
};

template<typename T>
class ThreadSafeQueue {
	private:
		std::queue<T> _queue;
		mutable std::mutex _mutex;
		std::conditional_variable _condition;

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

			if (_condition.waitFor(lock, timeout, [this] { return !_queue.empty(); })) {
				item = std::move(_queue.front());
				_queue.pop();
				return true;
			}
			return false;
		}

		void waitAndPop(T& item) {
			std::unique_lock(std::mutex) lock(_mutex);
			_condition.wait(lock, [this] { return !_queue.empty(); });

			item = std::move(_queue.front());
			_queue.pop();
		}

		size_t size() const {
			std::lock_guard<std::mutex> lock(_mutex);
			return _queue.size();
		}

};

#endif