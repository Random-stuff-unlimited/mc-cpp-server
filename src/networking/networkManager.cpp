#include "networking.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>

NetworkManager::NetworkManager(size_t workerCount)
	: _shutdownFlag(false), _receiverThreadInit(0), _senderThreadInit(0)
{
	_workerThreads.reserve(workerCount);

	setupEpoll();
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
		// Main networkManager

		if (!_receiverThreadInit) {
			_receiverThread = std::thread(&NetworkManager::receiverThreadLoop, this);
			_receiverThreadInit = 1;
		}

		if (!_senderThreadInit) {
			_senderThread = std::thread(&NetworkManager::senderThreadLoop, this);
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