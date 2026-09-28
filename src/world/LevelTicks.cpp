#include "world/LevelTicks.hpp"

#include "world/Chunk.hpp"

#include <algorithm>

namespace {
	// std heaps keep the greatest element on top: "a runs after b" makes the earliest tick the top
	struct RunsAfter {
		bool operator()(const ScheduledTick& a, const ScheduledTick& b) const { return LevelTicks::drainsBefore(b, a); }
	};
	struct ContainerRunsAfter {
		bool operator()(const ChunkTicks* a, const ChunkTicks* b) const { return LevelTicks::intraTickBefore(*b->peek(), *a->peek()); }
	};
} // namespace

bool LevelTicks::drainsBefore(const ScheduledTick& a, const ScheduledTick& b) {
	if (a.triggerTick != b.triggerTick) return a.triggerTick < b.triggerTick;
	return intraTickBefore(a, b);
}

bool LevelTicks::intraTickBefore(const ScheduledTick& a, const ScheduledTick& b) {
	if (a.priority != b.priority) return a.priority < b.priority;
	return a.subTickOrder < b.subTickOrder;
}

// ----- ChunkTicks -----

void ChunkTicks::setPending(std::vector<SavedTick> ticks) {
	std::lock_guard<std::mutex> lock(_mutex);
	_pending = std::move(ticks);
	for (const SavedTick& tick : _pending) _perPosition.insert({tick.pos.asLong(), tick.type});
}

// Like vanilla: loaded ticks get negative sub-tick orders, so they run before any tick scheduled after loading
void ChunkTicks::unpack(int64_t gameTime) {
	if (_pending.empty()) return;
	std::vector<SavedTick> pending;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		pending.swap(_pending);
	}
	int64_t order = -static_cast<int64_t>(pending.size());
	for (const SavedTick& tick : pending) scheduleUnchecked({tick.type, tick.pos, gameTime + tick.delay, tick.priority, order++});
}

ScheduledTick ChunkTicks::poll() {
	std::lock_guard<std::mutex> lock(_mutex);
	std::pop_heap(_queue.begin(), _queue.end(), RunsAfter());
	ScheduledTick tick = _queue.back();
	_queue.pop_back();
	_perPosition.erase({tick.pos.asLong(), tick.type});
	return tick;
}

void ChunkTicks::schedule(const ScheduledTick& tick) {
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_perPosition.insert({tick.pos.asLong(), tick.type}).second) return;
	}
	scheduleUnchecked(tick);
}

void ChunkTicks::scheduleUnchecked(const ScheduledTick& tick) {
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_queue.push_back(tick);
		std::push_heap(_queue.begin(), _queue.end(), RunsAfter());
	}
	if (_owner) _owner->onTickAdded(*this, tick);
}

bool ChunkTicks::hasScheduledTick(const BlockPos& pos, int type) const { return _perPosition.count({pos.asLong(), type}) != 0; }

void ChunkTicks::removeIf(const std::function<bool(const ScheduledTick&)>& predicate) {
	std::lock_guard<std::mutex> lock(_mutex);
	auto removed = std::remove_if(_queue.begin(), _queue.end(), [&](const ScheduledTick& tick) {
		if (!predicate(tick)) return false;
		_perPosition.erase({tick.pos.asLong(), tick.type});
		return true;
	});
	_queue.erase(removed, _queue.end());
	std::make_heap(_queue.begin(), _queue.end(), RunsAfter());
}

std::vector<SavedTick> ChunkTicks::pack(int64_t gameTime) const {
	std::lock_guard<std::mutex> lock(_mutex);
	std::vector<SavedTick>		saved(_pending);
	for (const ScheduledTick& tick : _queue) saved.push_back({tick.type, tick.pos, static_cast<int>(tick.triggerTick - gameTime), tick.priority});
	return saved;
}

// ----- LevelTicks -----

void LevelTicks::addContainer(int64_t chunkKey, ChunkTicks* ticks) {
	_allContainers[chunkKey] = ticks;
	if (const ScheduledTick* next = ticks->peek()) _nextTickForContainer[chunkKey] = next->triggerTick;
	ticks->_owner = this;
}

void LevelTicks::removeContainer(int64_t chunkKey) {
	auto it = _allContainers.find(chunkKey);
	if (it == _allContainers.end()) return;
	it->second->_owner = nullptr;
	_allContainers.erase(it);
	_nextTickForContainer.erase(chunkKey);
}

// chunkScheduleUpdater: the container's next tick changed
void LevelTicks::onTickAdded(ChunkTicks& container, const ScheduledTick& tick) {
	const ScheduledTick* next = container.peek();
	if (next && *next == tick) updateContainerScheduling(tick);
}

void LevelTicks::updateContainerScheduling(const ScheduledTick& tick) {
	_nextTickForContainer[Chunk::key(tick.pos.chunkX(), tick.pos.chunkZ())] = tick.triggerTick;
}

bool LevelTicks::schedule(const ScheduledTick& tick) {
	auto it = _allContainers.find(Chunk::key(tick.pos.chunkX(), tick.pos.chunkZ()));
	if (it == _allContainers.end()) return false;
	it->second->schedule(tick);
	return true;
}

void LevelTicks::tick(int64_t gameTime, int maxTicks, const std::function<void(const BlockPos&, int)>& run) {
	// collectTicks
	sortContainersToTick(gameTime);
	drainContainers(gameTime, maxTicks);
	for (ChunkTicks* container : _containersToTick) updateContainerScheduling(*container->peek()); // rescheduleLeftoverContainers

	// runCollectedTicks: ticks scheduled meanwhile go to their container, for the next game tick
	while (!_toRunThisTick.empty()) {
		ScheduledTick tick = _toRunThisTick.front();
		_toRunThisTick.pop_front();
		if (!_toRunThisTickSet.empty()) _toRunThisTickSet.erase({tick.pos.asLong(), tick.type});
		_alreadyRunThisTick.push_back(tick);
		run(tick.pos, tick.type);
	}

	// cleanupAfterTick
	_toRunThisTick.clear();
	_containersToTick.clear();
	_alreadyRunThisTick.clear();
	_toRunThisTickSet.clear();
}

void LevelTicks::sortContainersToTick(int64_t gameTime) {
	for (auto it = _nextTickForContainer.begin(); it != _nextTickForContainer.end();) {
		if (it->second > gameTime) {
			++it;
			continue;
		}
		auto container = _allContainers.find(it->first);
		const ScheduledTick* next = container == _allContainers.end() ? nullptr : container->second->peek();
		if (!next) {
			it = _nextTickForContainer.erase(it);
		} else if (next->triggerTick > gameTime) {
			it->second = next->triggerTick;
			++it;
		} else if (_tickCheck(it->first)) {
			pushContainer(container->second);
			it = _nextTickForContainer.erase(it);
		} else {
			++it;
		}
	}
}

void LevelTicks::drainContainers(int64_t gameTime, int maxTicks) {
	while (static_cast<int>(_toRunThisTick.size()) < maxTicks && !_containersToTick.empty()) {
		ChunkTicks* container = popContainer();
		_toRunThisTick.push_back(container->poll());
		drainFromCurrentContainer(*container, gameTime, maxTicks);
		const ScheduledTick* next = container->peek();
		if (next) {
			if (next->triggerTick <= gameTime && static_cast<int>(_toRunThisTick.size()) < maxTicks) {
				pushContainer(container);
			} else {
				updateContainerScheduling(*next);
			}
		}
	}
}

// Takes the container's ticks while they come before the next container's first one
void LevelTicks::drainFromCurrentContainer(ChunkTicks& container, int64_t gameTime, int maxTicks) {
	if (static_cast<int>(_toRunThisTick.size()) >= maxTicks) return;
	const ScheduledTick* otherNext = _containersToTick.empty() ? nullptr : _containersToTick.front()->peek();
	while (static_cast<int>(_toRunThisTick.size()) < maxTicks) {
		const ScheduledTick* next = container.peek();
		if (!next || next->triggerTick > gameTime || (otherNext && intraTickBefore(*otherNext, *next))) break;
		_toRunThisTick.push_back(container.poll());
	}
}

void LevelTicks::pushContainer(ChunkTicks* container) {
	_containersToTick.push_back(container);
	std::push_heap(_containersToTick.begin(), _containersToTick.end(), ContainerRunsAfter());
}

ChunkTicks* LevelTicks::popContainer() {
	std::pop_heap(_containersToTick.begin(), _containersToTick.end(), ContainerRunsAfter());
	ChunkTicks* container = _containersToTick.back();
	_containersToTick.pop_back();
	return container;
}

bool LevelTicks::hasScheduledTick(const BlockPos& pos, int type) const {
	auto it = _allContainers.find(Chunk::key(pos.chunkX(), pos.chunkZ()));
	return it != _allContainers.end() && it->second->hasScheduledTick(pos, type);
}

bool LevelTicks::willTickThisTick(const BlockPos& pos, int type) {
	if (_toRunThisTickSet.empty() && !_toRunThisTick.empty()) {
		for (const ScheduledTick& tick : _toRunThisTick) _toRunThisTickSet.insert({tick.pos.asLong(), tick.type});
	}
	return _toRunThisTickSet.count({pos.asLong(), type}) != 0;
}
