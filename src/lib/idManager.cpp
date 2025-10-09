#include "network/id_manager.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>

IdManager::IdManager() : _nextId(0) {}

IdManager::~IdManager() {}

uint32_t IdManager::allocate() {
	std::lock_guard<std::mutex> lock(_mutex);
	if (!_freedIds.empty()) {
		uint32_t reusedId = *_freedIds.begin();
		_freedIds.erase(_freedIds.begin());
		std::cout << "[IdManager] Reused freed ID: " << reusedId << std::endl;
		return reusedId;
	}
	uint32_t newId = _nextId++;
	std::cout << "[IdManager] Allocated new ID: " << newId << std::endl;
	return newId;
}

void IdManager::release(uint32_t id) {
	std::lock_guard<std::mutex> lock(_mutex);
	if (id >= _nextId) {
		std::cerr << "[IdManager] WARNING: Trying to release invalid ID " << id
				  << " (nextId: " << _nextId << ")" << std::endl;
		return;
	}
	if (_freedIds.find(id) != _freedIds.end()) {
		std::cerr << "[IdManager] WARNING: Trying to release already freed ID " << id << std::endl;
		return;
	}

	_freedIds.insert(id);
	std::cout << "[IdManager] Released ID: " << id << " (freed count: " << _freedIds.size() << ")"
			  << std::endl;
}

size_t IdManager::getAllocatedCount() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _nextId - _freedIds.size();
}

size_t IdManager::getFreedCount() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _freedIds.size();
}

void IdManager::reset() {
	std::lock_guard<std::mutex> lock(_mutex);
	_nextId = 0;
	_freedIds.clear();
	std::cout << "[IdManager] Reset - all IDs cleared" << std::endl;
}
