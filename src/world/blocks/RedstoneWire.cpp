#include "world/blocks/Redstone.hpp"

#include "lib/JavaHashSet.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"

#include <algorithm>

// RedStoneWireBlock with the DefaultRedstoneWireEvaluator (the one vanilla uses without the experiment)

namespace {
	// Player.mayBuild: not in adventure or spectator mode
	bool mayBuild(const Player& player) { return player.getGameMode() == GameMode::Survival || player.getGameMode() == GameMode::Creative; }
} // namespace

RedStoneWireBlock::RedStoneWireBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids)
	: RedstoneBehavior(std::move(context), std::move(ids)) {
	int cross = blocks().defaultState(_ids->wire);
	for (int property : _ids->sides) cross = blocks().with(cross, property, _ids->side);
	_crossState = cross;
}

bool RedStoneWireBlock::connected(int state, Direction direction) const {
	return blocks().get(state, _ids->sides[RedstoneIds::sideIndex(direction)]) != _ids->none;
}
bool RedStoneWireBlock::isCross(int state) const {
	return connected(state, Direction::North) && connected(state, Direction::South) && connected(state, Direction::East) && connected(state, Direction::West);
}
bool RedStoneWireBlock::isDot(int state) const {
	return !connected(state, Direction::North) && !connected(state, Direction::South) && !connected(state, Direction::East) && !connected(state, Direction::West);
}
int RedStoneWireBlock::wireSignal(int state) const { return blockOf(state) == _ids->wire ? blocks().getInt(state, _ids->power) : 0; }

bool RedStoneWireBlock::canSurviveOn(Level&, const BlockPos&, int state) const {
	return _context->isFaceSturdy(state, Direction::Up) || blockOf(state) == _ids->hopper;
}

bool RedStoneWireBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	BlockPos below = pos.below();
	return canSurviveOn(level, below, level.getBlockState(below));
}

// shouldConnectTo(state, direction), direction null for shouldConnectTo(state)
bool RedStoneWireBlock::shouldConnectTo(Level& level, int state, const Direction* direction) const {
	int block = blockOf(state);
	if (block == _ids->wire) return true;
	if (block == _ids->repeater) {
		Direction facing = _context->direction(state, _context->facing);
		return direction && (facing == *direction || Directions::opposite(facing) == *direction);
	}
	if (block == _ids->observer) return direction && *direction == _context->direction(state, _context->facing);
	return level.isSignalSource(state) && direction;
}

int RedStoneWireBlock::getConnectingSide(Level& level, const BlockPos& pos, Direction direction) const {
	return getConnectingSide(level, pos, direction, !level.isRedstoneConductor(level.getBlockState(pos.above())));
}

int RedStoneWireBlock::getConnectingSide(Level& level, const BlockPos& pos, Direction direction, bool nonConductorAbove) const {
	BlockPos neighbor	   = pos.relative(direction);
	int		 neighborState = level.getBlockState(neighbor);
	if (nonConductorAbove) {
		bool climbable = _context->data.isInstanceOf(blockOf(neighborState), "TrapDoorBlock") || canSurviveOn(level, neighbor, neighborState);
		if (climbable && shouldConnectTo(level, level.getBlockState(neighbor.above()), nullptr)) {
			return _context->isFaceSturdy(neighborState, Directions::opposite(direction)) ? _ids->up : _ids->side;
		}
	}
	bool toward = shouldConnectTo(level, neighborState, &direction);
	if (!toward && (level.isRedstoneConductor(neighborState) || !shouldConnectTo(level, level.getBlockState(neighbor.below()), nullptr))) return _ids->none;
	return _ids->side;
}

int RedStoneWireBlock::getMissingConnections(Level& level, int state, const BlockPos& pos) const {
	bool nonConductorAbove = !level.isRedstoneConductor(level.getBlockState(pos.above()));
	for (Direction direction : Directions::HORIZONTAL) {
		if (connected(state, direction)) continue;
		state = blocks().with(state, _ids->sides[RedstoneIds::sideIndex(direction)], getConnectingSide(level, pos, direction, nonConductorAbove));
	}
	return state;
}

int RedStoneWireBlock::getConnectionState(Level& level, int state, const BlockPos& pos) const {
	bool dot = isDot(state);
	state	 = getMissingConnections(level, blocks().withInt(blocks().defaultState(_ids->wire), _ids->power, blocks().getInt(state, _ids->power)), pos);
	if (dot && isDot(state)) return state;
	bool north = connected(state, Direction::North), south = connected(state, Direction::South);
	bool east = connected(state, Direction::East), west = connected(state, Direction::West);
	bool noNorthSouth = !north && !south, noEastWest = !east && !west;
	if (!west && noNorthSouth) state = blocks().with(state, _ids->sides[3], _ids->side);
	if (!east && noNorthSouth) state = blocks().with(state, _ids->sides[1], _ids->side);
	if (!north && noEastWest) state = blocks().with(state, _ids->sides[0], _ids->side);
	if (!south && noEastWest) state = blocks().with(state, _ids->sides[2], _ids->side);
	return state;
}

int RedStoneWireBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	return getConnectionState(level, _crossState, context.clickedPos);
}

int RedStoneWireBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const {
	if (direction == Direction::Down) return canSurviveOn(level, neighborPos, neighborState) ? state : _ids->air;
	if (direction == Direction::Up) return getConnectionState(level, state, pos);
	int property = _ids->sides[RedstoneIds::sideIndex(direction)];
	int side	 = getConnectingSide(level, pos, direction);
	if ((side != _ids->none) == connected(state, direction) && !isCross(state)) return blocks().with(state, property, side);
	int cross = blocks().with(blocks().withInt(_crossState, _ids->power, blocks().getInt(state, _ids->power)), property, side);
	return getConnectionState(level, cross, pos);
}

void RedStoneWireBlock::updateIndirectNeighbourShapes(Level& level, const BlockPos& pos, int state, int flags, int limit) const {
	for (Direction direction : Directions::HORIZONTAL) {
		if (!connected(state, direction) || blockOf(level.getBlockState(pos.relative(direction))) == _ids->wire) continue;
		Direction back = Directions::opposite(direction);
		for (int dy : {-1, 1}) {
			BlockPos corner = pos.relative(direction).offset(0, dy, 0);
			if (blockOf(level.getBlockState(corner)) != _ids->wire) continue;
			BlockPos from = corner.relative(back);
			level.neighborShapeChanged(back, corner, from, level.getBlockState(from), flags, limit);
		}
	}
}

// DefaultRedstoneWireEvaluator.updatePowerStrength: the new power, then updates around, in the order of a Java HashSet
// of the position and its neighbors (the source of the wire's famous locational behavior)
void RedStoneWireBlock::updatePowerStrength(Level& level, const BlockPos& pos, int state) const {
	// calculateTargetStrength: the power of the blocks around (not counting wires), else the wires' minus one
	_shouldSignal = false;
	int blockSignal = level.getBestNeighborSignal(pos);
	_shouldSignal	= true;
	int target		= blockSignal;
	if (blockSignal != 15) {
		int	 incoming		 = 0;
		bool conductorAbove = level.isRedstoneConductor(level.getBlockState(pos.above()));
		for (Direction direction : Directions::HORIZONTAL) {
			BlockPos neighbor	   = pos.relative(direction);
			int		 neighborState = level.getBlockState(neighbor);
			incoming			   = std::max(incoming, wireSignal(neighborState));
			if (level.isRedstoneConductor(neighborState) && !conductorAbove) {
				incoming = std::max(incoming, wireSignal(level.getBlockState(neighbor.above())));
			} else if (!level.isRedstoneConductor(neighborState)) {
				incoming = std::max(incoming, wireSignal(level.getBlockState(neighbor.below())));
			}
		}
		target = std::max(blockSignal, std::max(0, incoming - 1));
	}
	if (blocks().getInt(state, _ids->power) == target) return;
	if (level.getBlockState(pos) == state) level.setBlock(pos, blocks().withInt(state, _ids->power, target), Level::UPDATE_CLIENTS);
	JavaHashSet<BlockPos> toUpdate;
	toUpdate.add(pos);
	for (Direction direction : Directions::ALL) toUpdate.add(pos.relative(direction));
	for (const BlockPos& at : toUpdate.values()) level.updateNeighborsAt(at, _ids->wire);
}

void RedStoneWireBlock::checkCornerChangeAt(Level& level, const BlockPos& pos) const {
	if (blockOf(level.getBlockState(pos)) != _ids->wire) return;
	level.updateNeighborsAt(pos, _ids->wire);
	for (Direction direction : Directions::ALL) level.updateNeighborsAt(pos.relative(direction), _ids->wire);
}

void RedStoneWireBlock::updateNeighborsOfNeighboringWires(Level& level, const BlockPos& pos) const {
	for (Direction direction : Directions::HORIZONTAL) checkCornerChangeAt(level, pos.relative(direction));
	for (Direction direction : Directions::HORIZONTAL) {
		BlockPos neighbor = pos.relative(direction);
		checkCornerChangeAt(level, level.isRedstoneConductor(level.getBlockState(neighbor)) ? neighbor.above() : neighbor.below());
	}
}

void RedStoneWireBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (blockOf(oldState) == _ids->wire) return;
	updatePowerStrength(level, pos, state);
	// Direction.Plane.VERTICAL: up, then down
	level.updateNeighborsAt(pos.above(), _ids->wire);
	level.updateNeighborsAt(pos.below(), _ids->wire);
	updateNeighborsOfNeighboringWires(level, pos);
}

void RedStoneWireBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const {
	if (movedByPiston) return;
	for (Direction direction : Directions::ALL) level.updateNeighborsAt(pos.relative(direction), _ids->wire);
	updatePowerStrength(level, pos, state);
	updateNeighborsOfNeighboringWires(level, pos);
}

void RedStoneWireBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	if (canSurvive(level, pos, state)) {
		updatePowerStrength(level, pos, state);
	} else {
		level.dropResources(state, pos);
		level.removeBlock(pos, false);
	}
}

int RedStoneWireBlock::getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const {
	if (!_shouldSignal || direction == Direction::Down) return 0;
	int power = blocks().getInt(state, _ids->power);
	if (power == 0) return 0;
	if (direction != Direction::Up && !connected(getConnectionState(level, state, pos), Directions::opposite(direction))) return 0;
	return power;
}

int RedStoneWireBlock::getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const {
	return _shouldSignal ? getSignal(level, pos, state, direction) : 0;
}

bool RedStoneWireBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (!mayBuild(player) || !(isCross(state) || isDot(state))) return false;
	int changed = isCross(state) ? blocks().defaultState(_ids->wire) : _crossState;
	changed		= getConnectionState(level, blocks().withInt(changed, _ids->power, blocks().getInt(state, _ids->power)), pos);
	if (changed == state) return false;
	level.setBlock(pos, changed, Level::UPDATE_ALL);
	// updatesOnShapeChange: conductors next to a side that changed hear it
	for (Direction direction : Directions::HORIZONTAL) {
		BlockPos neighbor = pos.relative(direction);
		if (connected(state, direction) != connected(changed, direction) && level.isRedstoneConductor(level.getBlockState(neighbor))) {
			level.updateNeighborsAtExceptFromFacing(neighbor, _ids->wire, Directions::opposite(direction));
		}
	}
	return true;
}