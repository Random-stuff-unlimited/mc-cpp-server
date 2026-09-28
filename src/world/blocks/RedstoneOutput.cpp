#include "world/blocks/Redstone.hpp"

#include "world/Level.hpp"
#include "world/PlaceContext.hpp"

// The redstone output blocks: ObserverBlock, RedstoneLampBlock and TargetBlock

// ObserverBlock: a 2-tick pulse when the block it faces changes

int ObserverBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	return blocks().with(blocks().defaultState(_ids->observer), _context->facing, _context->directionValue(context.nearestLookingDirection()));
}

void ObserverBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (blocks().getBool(state, _ids->powered)) {
		level.setBlock(pos, blocks().withBool(state, _ids->powered, false), Level::UPDATE_CLIENTS);
	} else {
		level.setBlock(pos, blocks().withBool(state, _ids->powered, true), Level::UPDATE_CLIENTS);
		level.scheduleTick(pos, _ids->observer, 2);
	}
	updateNeighborsInFront(level, pos, state);
}

int ObserverBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	// startSignal
	if (_context->direction(state, _context->facing) == direction && !blocks().getBool(state, _ids->powered) &&
		!level.hasScheduledTick(pos, _ids->observer)) {
		level.scheduleTick(pos, _ids->observer, 2);
	}
	return state;
}

void ObserverBlock::updateNeighborsInFront(Level& level, const BlockPos& pos, int state) const {
	Direction facing = _context->direction(state, _context->facing);
	BlockPos  back	 = pos.relative(Directions::opposite(facing));
	level.neighborChanged(back, _ids->observer);
	level.updateNeighborsAtExceptFromFacing(back, _ids->observer, facing);
}

void ObserverBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (blockOf(oldState) == _ids->observer) return;
	if (blocks().getBool(state, _ids->powered) && !level.hasScheduledTick(pos, _ids->observer)) {
		int off = blocks().withBool(state, _ids->powered, false);
		level.setBlock(pos, off, Level::UPDATE_CLIENTS | Level::UPDATE_KNOWN_SHAPE);
		updateNeighborsInFront(level, pos, off);
	}
}

void ObserverBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool) const {
	if (blocks().getBool(state, _ids->powered) && level.hasScheduledTick(pos, _ids->observer)) {
		updateNeighborsInFront(level, pos, blocks().withBool(state, _ids->powered, false));
	}
}

int ObserverBlock::getSignal(Level&, const BlockPos&, int state, Direction direction) const {
	return blocks().getBool(state, _ids->powered) && _context->direction(state, _context->facing) == direction ? 15 : 0;
}

int ObserverBlock::getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const { return getSignal(level, pos, state, direction); }

// RedstoneLampBlock: on at once, off 4 ticks after losing power

int RedstoneLampBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	return blocks().withBool(blocks().defaultState(context.block), _ids->lit, level.hasNeighborSignal(context.clickedPos));
}

void RedstoneLampBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	bool lit = blocks().getBool(state, _ids->lit);
	if (lit == level.hasNeighborSignal(pos)) return;
	if (lit) {
		level.scheduleTick(pos, blockOf(state), 4);
	} else {
		level.setBlock(pos, blocks().cycle(state, _ids->lit), Level::UPDATE_CLIENTS);
	}
}

void RedstoneLampBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (blocks().getBool(state, _ids->lit) && !level.hasNeighborSignal(pos)) level.setBlock(pos, blocks().cycle(state, _ids->lit), Level::UPDATE_CLIENTS);
}

// TargetBlock: powered by projectiles (none yet); it still gives its power and resets

void TargetBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (blocks().getInt(state, _ids->power) != 0) level.setBlock(pos, blocks().withInt(state, _ids->power, 0), Level::UPDATE_ALL);
}

void TargetBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (blockOf(oldState) == blockOf(state)) return;
	if (blocks().getInt(state, _ids->power) > 0 && !level.hasScheduledTick(pos, blockOf(state))) {
		level.setBlock(pos, blocks().withInt(state, _ids->power, 0), Level::UPDATE_CLIENTS | Level::UPDATE_KNOWN_SHAPE);
	}
}

int TargetBlock::getSignal(Level&, const BlockPos&, int state, Direction) const { return blocks().getInt(state, _ids->power); }