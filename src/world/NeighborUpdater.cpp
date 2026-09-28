#include "world/NeighborUpdater.hpp"

#include "logger.hpp"

#include <string>

void NeighborUpdater::shapeUpdate(Direction direction, int neighborState, const BlockPos& pos, const BlockPos& neighborPos, int flags, int limit) {
	Update update{Update::Kind::Shape, pos, neighborPos};
	update.state	 = neighborState;
	update.direction = direction;
	update.flags	 = flags;
	update.limit	 = limit;
	addAndRun(update);
}

void NeighborUpdater::neighborChanged(const BlockPos& pos, int sourceBlock) {
	Update update{Update::Kind::Simple, pos, {}};
	update.block = sourceBlock;
	addAndRun(update);
}

void NeighborUpdater::neighborChanged(int state, const BlockPos& pos, int sourceBlock, bool movedByPiston) {
	Update update{Update::Kind::Full, pos, {}};
	update.state		 = state;
	update.block		 = sourceBlock;
	update.movedByPiston = movedByPiston;
	addAndRun(update);
}

void NeighborUpdater::updateNeighborsAtExceptFromFacing(const BlockPos& pos, int sourceBlock, const Direction* skip) {
	Update update{Update::Kind::Multi, pos, {}};
	update.block = sourceBlock;
	update.skip	 = skip ? static_cast<int8_t>(*skip) : -1;
	if (static_cast<int8_t>(Directions::UPDATE_ORDER[0]) == update.skip) update.index++;
	addAndRun(update);
}

void NeighborUpdater::addAndRun(const Update& update) {
	bool running = _count > 0;
	bool tooMany = _maxChained >= 0 && _count >= _maxChained;
	_count++;
	if (!tooMany) {
		if (running) {
			_addedThisLayer.push_back(update);
		} else {
			_stack.push_back(update);
		}
	} else if (_count - 1 == _maxChained) {
		g_logger->logGameInfo(ERROR,
							  "Too many chained neighbor updates. Skipping the rest. First skipped position: " + std::to_string(update.pos.x) + ", " +
									  std::to_string(update.pos.y) + ", " + std::to_string(update.pos.z),
							  "World");
	}
	if (!running) runUpdates();
}

void NeighborUpdater::runUpdates() {
	try {
		while (!_stack.empty() || !_addedThisLayer.empty()) {
			// The updates added by the previous step go on top, the first added ending on the very top
			for (auto it = _addedThisLayer.rbegin(); it != _addedThisLayer.rend(); ++it) _stack.push_back(*it);
			_addedThisLayer.clear();

			// A step may add updates (to _addedThisLayer only): the top stays in place meanwhile
			size_t top = _stack.size() - 1;
			while (_addedThisLayer.empty()) {
				if (!runNext(_stack[top])) {
					_stack.erase(_stack.begin() + top);
					break;
				}
			}
		}
	} catch (...) {
		// Like vanilla's finally: the next updates must start from a clean state
		_stack.clear();
		_addedThisLayer.clear();
		_count = 0;
		throw;
	}
	_stack.clear();
	_addedThisLayer.clear();
	_count = 0;
}

bool NeighborUpdater::runNext(Update& update) {
	switch (update.kind) {
	case Update::Kind::Shape:
		_target.executeShapeUpdate(update.direction, update.pos, update.neighborPos, update.state, update.flags, update.limit);
		return false;
	case Update::Kind::Simple:
		_target.executeNeighborChanged(_target.getBlockState(update.pos), update.pos, update.block, false);
		return false;
	case Update::Kind::Full:
		_target.executeNeighborChanged(update.state, update.pos, update.block, update.movedByPiston);
		return false;
	case Update::Kind::Multi: {
		BlockPos neighbor = update.pos.relative(Directions::UPDATE_ORDER[update.index++]);
		_target.executeNeighborChanged(_target.getBlockState(neighbor), neighbor, update.block, false);
		if (update.index < 6 && static_cast<int8_t>(Directions::UPDATE_ORDER[update.index]) == update.skip) update.index++;
		return update.index < 6;
	}
	}
	return false;
}
