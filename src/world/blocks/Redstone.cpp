#include "world/blocks/Redstone.hpp"

#include "lib/JavaHashSet.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/Shapes.hpp"

#include <algorithm>
#include <cmath>

namespace {
	constexpr int LEVEL_EVENT_REDSTONE_TORCH_BURNOUT = 1502;

	// Player.mayBuild: not in adventure or spectator mode
	bool mayBuild(const Player& player) { return player.getGameMode() == GameMode::Survival || player.getGameMode() == GameMode::Creative; }
	// The pitch of doors, trapdoors and fence gates: level.getRandom().nextFloat() * 0.1F + 0.9F
	float openPitch(Level& level) { return level.random().nextFloat() * 0.1F + 0.9F; }
} // namespace

Direction clockWise(Direction direction) {
	switch (direction) {
	case Direction::North:
		return Direction::East;
	case Direction::East:
		return Direction::South;
	case Direction::South:
		return Direction::West;
	case Direction::West:
		return Direction::North;
	default:
		return direction;
	}
}

Direction counterClockWise(Direction direction) {
	switch (direction) {
	case Direction::North:
		return Direction::West;
	case Direction::West:
		return Direction::South;
	case Direction::South:
		return Direction::East;
	case Direction::East:
		return Direction::North;
	default:
		return direction;
	}
}

RedstoneIds::RedstoneIds(const BlockContext& context) {
	const BlockRegistry& b = context.blocks;
	power				   = b.property("power");
	powered				   = b.property("powered");
	lit					   = b.property("lit");
	face				   = b.property("face");
	delay				   = b.property("delay");
	locked				   = b.property("locked");
	mode				   = b.property("mode");
	note				   = b.property("note");
	instrument			   = b.property("instrument");
	open				   = b.property("open");
	hinge				   = b.property("hinge");
	inWall				   = b.property("in_wall");
	sides[0]			   = b.property("north");
	sides[1]			   = b.property("east");
	sides[2]			   = b.property("south");
	sides[3]			   = b.property("west");
	none				   = b.value("none");
	side				   = b.value("side");
	up					   = b.value("up");
	floor				   = b.value("floor");
	wall				   = b.value("wall");
	ceiling				   = b.value("ceiling");
	compare				   = b.value("compare");
	subtract			   = b.value("subtract");
	wire				   = context.block("minecraft:redstone_wire");
	repeater			   = context.block("minecraft:repeater");
	comparator			   = context.block("minecraft:comparator");
	observer			   = context.block("minecraft:observer");
	redstoneBlock		   = context.block("minecraft:redstone_block");
	hopper				   = context.block("minecraft:hopper");
	air					   = context.defaultState("minecraft:air");
}

int RedstoneIds::sideIndex(Direction direction) {
	switch (direction) {
	case Direction::North:
		return 0;
	case Direction::East:
		return 1;
	case Direction::South:
		return 2;
	default:
		return 3;
	}
}

// ===== Redstone wire =====

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

// ===== Redstone torches =====

RedstoneTorchBlock::RedstoneTorchBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool wall)
	: RedstoneBehavior(std::move(context), std::move(ids)), _wall(wall) {
	_floorTorch = _context->block("minecraft:redstone_torch");
	_wallTorch	= _context->block("minecraft:redstone_wall_torch");
}

bool RedstoneTorchBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	if (!_wall) return _context->canSupportCenter(level, pos.below(), Direction::Up);
	Direction facing = _context->direction(state, _context->facing);
	return _context->isFaceSturdy(level.getBlockState(pos.relative(Directions::opposite(facing))), facing);
}

// StandingAndWallBlockItem: the first direction the player looks toward, but up, where the torch can hang on
int RedstoneTorchBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	for (Direction direction : context.nearestLookingDirections()) {
		if (direction == Direction::Up) continue;
		int state = blocks().defaultState(_floorTorch);
		if (direction != Direction::Down) {
			state = blocks().with(blocks().defaultState(_wallTorch), _context->facing, _context->directionValue(Directions::opposite(direction)));
		}
		if (level.behavior(state).canSurvive(level, context.clickedPos, state)) return state;
	}
	return -1;
}

int RedstoneTorchBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	Direction support = _wall ? Directions::opposite(_context->direction(state, _context->facing)) : Direction::Down;
	if (direction == support && !canSurvive(level, pos, state)) return _ids->air;
	return state;
}

void RedstoneTorchBlock::notifyNeighbors(Level& level, const BlockPos& pos, int state) const {
	for (Direction direction : Directions::ALL) level.updateNeighborsAt(pos.relative(direction), blockOf(state));
}

void RedstoneTorchBlock::onPlace(Level& level, const BlockPos& pos, int state, int, bool) const { notifyNeighbors(level, pos, state); }

void RedstoneTorchBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const {
	if (!movedByPiston) notifyNeighbors(level, pos, state);
}

bool RedstoneTorchBlock::hasNeighborSignal(Level& level, const BlockPos& pos, int state) const {
	if (!_wall) return level.hasSignal(pos.below(), Direction::Down);
	Direction behind = Directions::opposite(_context->direction(state, _context->facing));
	return level.hasSignal(pos.relative(behind), behind);
}

bool RedstoneTorchBlock::isToggledTooFrequently(Level& level, const BlockPos& pos, bool logToggle) const {
	auto& toggles = level.recentTorchToggles();
	if (logToggle) toggles.emplace_back(pos, level.getGameTime());
	int count = 0;
	for (const auto& [at, when] : toggles) {
		if (at == pos && ++count >= 8) return true;
	}
	return false;
}

void RedstoneTorchBlock::tick(Level& level, const BlockPos& pos, int state) const {
	bool powered = hasNeighborSignal(level, pos, state);
	auto& toggles = level.recentTorchToggles();
	while (!toggles.empty() && level.getGameTime() - toggles.front().second > 60) toggles.erase(toggles.begin());
	if (blocks().getBool(state, _ids->lit)) {
		if (!powered) return;
		level.setBlock(pos, blocks().withBool(state, _ids->lit, false), Level::UPDATE_ALL);
		if (isToggledTooFrequently(level, pos, true)) {
			level.levelEvent(nullptr, LEVEL_EVENT_REDSTONE_TORCH_BURNOUT, pos, 0);
			level.scheduleTick(pos, blockOf(level.getBlockState(pos)), 160);
		}
	} else if (!powered && !isToggledTooFrequently(level, pos, false)) {
		level.setBlock(pos, blocks().withBool(state, _ids->lit, true), Level::UPDATE_ALL);
	}
}

void RedstoneTorchBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	if (blocks().getBool(state, _ids->lit) == hasNeighborSignal(level, pos, state) && !level.willTickThisTick(pos, blockOf(state))) {
		level.scheduleTick(pos, blockOf(state), 2);
	}
}

int RedstoneTorchBlock::getSignal(Level&, const BlockPos&, int state, Direction direction) const {
	if (!blocks().getBool(state, _ids->lit)) return 0;
	// The block holding it gets nothing
	Direction held = _wall ? _context->direction(state, _context->facing) : Direction::Up;
	return direction != held ? 15 : 0;
}

int RedstoneTorchBlock::getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const {
	return direction == Direction::Down ? getSignal(level, pos, state, direction) : 0;
}

// ===== Diodes =====

bool DiodeBlock::canSurviveOn(Level&, int below) const { return _context->isFaceSturdy(below, Direction::Up, GameData::SupportType::Rigid); }

bool DiodeBlock::canSurvive(Level& level, const BlockPos& pos, int) const { return canSurviveOn(level, level.getBlockState(pos.below())); }

int DiodeBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	return blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(Directions::opposite(context.horizontalDirection())));
}

bool DiodeBlock::isLocked(Level&, const BlockPos&, int) const { return false; }

bool DiodeBlock::shouldTurnOn(Level& level, const BlockPos& pos, int state) const { return getInputSignal(level, pos, state) > 0; }

int DiodeBlock::getInputSignal(Level& level, const BlockPos& pos, int state) const {
	Direction direction = facing(state);
	BlockPos  input		= pos.relative(direction);
	int		  signal	= level.getSignal(input, direction);
	if (signal >= 15) return signal;
	int inputState = level.getBlockState(input);
	return std::max(signal, blockOf(inputState) == _ids->wire ? blocks().getInt(inputState, _ids->power) : 0);
}

int DiodeBlock::getAlternateSignal(Level& level, const BlockPos& pos, int state) const {
	Direction direction = facing(state);
	Direction right = clockWise(direction), left = counterClockWise(direction);
	bool	  diodesOnly = sideInputDiodesOnly();
	return std::max(level.getControlInputSignal(pos.relative(right), right, diodesOnly), level.getControlInputSignal(pos.relative(left), left, diodesOnly));
}

int DiodeBlock::getOutputSignal(Level&, const BlockPos&, int) const { return 15; }

void DiodeBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (isLocked(level, pos, state)) return;
	bool powered = blocks().getBool(state, _ids->powered);
	bool should	 = shouldTurnOn(level, pos, state);
	if (powered && !should) {
		level.setBlock(pos, blocks().withBool(state, _ids->powered, false), Level::UPDATE_CLIENTS);
	} else if (!powered) {
		level.setBlock(pos, blocks().withBool(state, _ids->powered, true), Level::UPDATE_CLIENTS);
		if (!should) level.scheduleTick(pos, blockOf(state), getDelay(state), VERY_HIGH);
	}
}

int DiodeBlock::getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const {
	if (!blocks().getBool(state, _ids->powered)) return 0;
	return facing(state) == direction ? getOutputSignal(level, pos, state) : 0;
}

int DiodeBlock::getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const { return getSignal(level, pos, state, direction); }

void DiodeBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	if (canSurvive(level, pos, state)) {
		checkTickOnNeighbor(level, pos, state);
		return;
	}
	level.dropResources(state, pos);
	level.removeBlock(pos, false);
	for (Direction direction : Directions::ALL) level.updateNeighborsAt(pos.relative(direction), blockOf(state));
}

void DiodeBlock::checkTickOnNeighbor(Level& level, const BlockPos& pos, int state) const {
	if (isLocked(level, pos, state)) return;
	bool powered = blocks().getBool(state, _ids->powered);
	if (powered == shouldTurnOn(level, pos, state) || level.willTickThisTick(pos, blockOf(state))) return;
	int priority = HIGH;
	if (shouldPrioritize(level, pos, state)) {
		priority = EXTREMELY_HIGH;
	} else if (powered) {
		priority = VERY_HIGH;
	}
	level.scheduleTick(pos, blockOf(state), getDelay(state), priority);
}

// The diode in front of this one, not facing back into it, ticks first
bool DiodeBlock::shouldPrioritize(Level& level, const BlockPos& pos, int state) const {
	Direction front		 = Directions::opposite(facing(state));
	int		  frontState = level.getBlockState(pos.relative(front));
	return _context->data.isInstanceOf(blockOf(frontState), "DiodeBlock") && _context->direction(frontState, _context->facing) != front;
}

void DiodeBlock::setPlacedBy(Level& level, const BlockPos& pos, int state) const {
	if (shouldTurnOn(level, pos, state)) level.scheduleTick(pos, blockOf(state), 1);
}

void DiodeBlock::onPlace(Level& level, const BlockPos& pos, int state, int, bool) const { updateNeighborsInFront(level, pos, state); }

void DiodeBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const {
	if (!movedByPiston) updateNeighborsInFront(level, pos, state);
}

void DiodeBlock::updateNeighborsInFront(Level& level, const BlockPos& pos, int state) const {
	Direction direction = facing(state);
	BlockPos  front		= pos.relative(Directions::opposite(direction));
	level.neighborChanged(front, blockOf(state));
	level.updateNeighborsAtExceptFromFacing(front, blockOf(state), direction);
}

// ----- Repeater -----

int RepeaterBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	int state = DiodeBlock::getStateForPlacement(level, context);
	return blocks().withBool(state, _ids->locked, isLocked(level, context.clickedPos, state));
}

int RepeaterBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int neighborState) const {
	if (direction == Direction::Down && !canSurviveOn(level, neighborState)) return _ids->air;
	bool sameAxis = Shapes::axisOf(direction) == Shapes::axisOf(facing(state));
	if (!sameAxis) return blocks().withBool(state, _ids->locked, isLocked(level, pos, state));
	return state;
}

bool RepeaterBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (!mayBuild(player)) return false;
	level.setBlock(pos, blocks().cycle(state, _ids->delay), Level::UPDATE_ALL);
	return true;
}

bool RepeaterBlock::isLocked(Level& level, const BlockPos& pos, int state) const { return getAlternateSignal(level, pos, state) > 0; }

int RepeaterBlock::getDelay(int state) const { return blocks().getInt(state, _ids->delay) * 2; }

// ----- Comparator -----

int ComparatorBlock::updateShape(Level& level, const BlockPos&, int state, Direction direction, const BlockPos&, int neighborState) const {
	return direction == Direction::Down && !canSurviveOn(level, neighborState) ? _ids->air : state;
}

std::unique_ptr<BlockEntity> ComparatorBlock::newBlockEntity(const BlockPos& pos, int) const { return std::make_unique<ComparatorBlockEntity>(pos); }

int ComparatorBlock::getOutputSignal(Level& level, const BlockPos& pos, int) const { return level.comparatorOutput(pos); }

int ComparatorBlock::calculateOutputSignal(Level& level, const BlockPos& pos, int state) const {
	int input = getInputSignal(level, pos, state);
	if (input == 0) return 0;
	int side = getAlternateSignal(level, pos, state);
	if (side > input) return 0;
	return blocks().get(state, _ids->mode) == _ids->subtract ? input - side : input;
}

bool ComparatorBlock::shouldTurnOn(Level& level, const BlockPos& pos, int state) const {
	int input = getInputSignal(level, pos, state);
	if (input == 0) return false;
	int side = getAlternateSignal(level, pos, state);
	return input > side || (input == side && blocks().get(state, _ids->mode) == _ids->compare);
}

// A container, cake... behind (or behind a conductor) gives its analog output instead (no item frames yet)
int ComparatorBlock::getInputSignal(Level& level, const BlockPos& pos, int state) const {
	int		  signal	= DiodeBlock::getInputSignal(level, pos, state);
	Direction direction = facing(state);
	BlockPos  input		= pos.relative(direction);
	int		  inputState = level.getBlockState(input);
	if (_context->properties(inputState).analogOutput) {
		return level.behavior(inputState).getAnalogOutputSignal(level, input, inputState, Directions::opposite(direction));
	}
	if (signal < 15 && level.isRedstoneConductor(inputState)) {
		input	   = input.relative(direction);
		inputState = level.getBlockState(input);
		if (_context->properties(inputState).analogOutput) {
			signal = level.behavior(inputState).getAnalogOutputSignal(level, input, inputState, Directions::opposite(direction));
		}
	}
	return signal;
}

bool ComparatorBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (!mayBuild(player)) return false;
	state		= blocks().cycle(state, _ids->mode);
	float pitch = blocks().get(state, _ids->mode) == _ids->subtract ? 0.55F : 0.5F;
	level.playSound(&player, pos, "minecraft:block.comparator.click", Level::SoundSource::Blocks, 0.3F, pitch);
	level.setBlock(pos, state, Level::UPDATE_CLIENTS);
	refreshOutputState(level, pos, state);
	return true;
}

void ComparatorBlock::checkTickOnNeighbor(Level& level, const BlockPos& pos, int state) const {
	if (level.willTickThisTick(pos, blockOf(state))) return;
	int output = calculateOutputSignal(level, pos, state);
	if (output != level.comparatorOutput(pos) || blocks().getBool(state, _ids->powered) != shouldTurnOn(level, pos, state)) {
		level.scheduleTick(pos, blockOf(state), 2, shouldPrioritize(level, pos, state) ? HIGH : NORMAL);
	}
}

void ComparatorBlock::refreshOutputState(Level& level, const BlockPos& pos, int state) const {
	int output = calculateOutputSignal(level, pos, state);
	int old	   = level.comparatorOutput(pos);
	level.setComparatorOutput(pos, output);
	if (old == output && blocks().get(state, _ids->mode) != _ids->compare) return;
	bool should	 = shouldTurnOn(level, pos, state);
	bool powered = blocks().getBool(state, _ids->powered);
	if (powered && !should) {
		level.setBlock(pos, blocks().withBool(state, _ids->powered, false), Level::UPDATE_CLIENTS);
	} else if (!powered && should) {
		level.setBlock(pos, blocks().withBool(state, _ids->powered, true), Level::UPDATE_CLIENTS);
	}
	updateNeighborsInFront(level, pos, state);
}

void ComparatorBlock::tick(Level& level, const BlockPos& pos, int state) const { refreshOutputState(level, pos, state); }

// ===== Levers and buttons =====

Direction FaceAttachedBlock::connectedDirection(int state) const {
	int face = blocks().get(state, _ids->face);
	if (face == _ids->ceiling) return Direction::Down;
	if (face == _ids->floor) return Direction::Up;
	return _context->direction(state, _context->facing);
}

bool FaceAttachedBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	// canAttach: the block on the other side holds it with a sturdy face
	Direction toSupport = Directions::opposite(connectedDirection(state));
	return _context->isFaceSturdy(level.getBlockState(pos.relative(toSupport)), Directions::opposite(toSupport));
}

int FaceAttachedBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	int base = blocks().defaultState(context.block);
	for (Direction direction : context.nearestLookingDirections()) {
		int state;
		if (direction == Direction::Up || direction == Direction::Down) {
			state = blocks().with(base, _ids->face, direction == Direction::Up ? _ids->ceiling : _ids->floor);
			state = blocks().with(state, _context->facing, _context->directionValue(context.horizontalDirection()));
		} else {
			state = blocks().with(blocks().with(base, _ids->face, _ids->wall), _context->facing, _context->directionValue(Directions::opposite(direction)));
		}
		if (canSurvive(level, context.clickedPos, state)) return state;
	}
	return -1;
}

int FaceAttachedBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	if (Directions::opposite(connectedDirection(state)) == direction && !canSurvive(level, pos, state)) return _ids->air;
	return state;
}

int FaceAttachedBlock::getSignal(Level&, const BlockPos&, int state, Direction) const { return blocks().getBool(state, _ids->powered) ? 15 : 0; }

int FaceAttachedBlock::getDirectSignal(Level&, const BlockPos&, int state, Direction direction) const {
	return blocks().getBool(state, _ids->powered) && connectedDirection(state) == direction ? 15 : 0;
}

void FaceAttachedBlock::updateNeighbours(Level& level, const BlockPos& pos, int state) const {
	level.updateNeighborsAt(pos, blockOf(state));
	level.updateNeighborsAt(pos.relative(Directions::opposite(connectedDirection(state))), blockOf(state));
}

bool LeverBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player&) const {
	// pull
	state = blocks().cycle(state, _ids->powered);
	level.setBlock(pos, state, Level::UPDATE_ALL);
	updateNeighbours(level, pos, state);
	level.playSound(nullptr, pos, "minecraft:block.lever.click", Level::SoundSource::Blocks, 0.3F, blocks().getBool(state, _ids->powered) ? 0.6F : 0.5F);
	return true;
}

void LeverBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const {
	if (!movedByPiston && blocks().getBool(state, _ids->powered)) updateNeighbours(level, pos, state);
}

ButtonBlock::ButtonBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, int ticksToStayPressed, std::string sound)
	: FaceAttachedBlock(std::move(context), std::move(ids)), _ticksToStayPressed(ticksToStayPressed), _sound(std::move(sound)) {}

bool ButtonBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (blocks().getBool(state, _ids->powered)) return true; // CONSUME
	// press
	level.setBlock(pos, blocks().withBool(state, _ids->powered, true), Level::UPDATE_ALL);
	updateNeighbours(level, pos, state);
	level.scheduleTick(pos, blockOf(state), _ticksToStayPressed);
	level.playSound(&player, pos, _sound + ".click_on", Level::SoundSource::Blocks);
	return true;
}

void ButtonBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const {
	if (!movedByPiston && blocks().getBool(state, _ids->powered)) updateNeighbours(level, pos, state);
}

// checkPressed: no arrows yet, so a pressed button always comes back up
void ButtonBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (!blocks().getBool(state, _ids->powered)) return;
	level.setBlock(pos, blocks().withBool(state, _ids->powered, false), Level::UPDATE_ALL);
	updateNeighbours(level, pos, state);
	level.playSound(nullptr, pos, _sound + ".click_off", Level::SoundSource::Blocks);
}

// ===== Pressure plates =====

PressurePlateBlock::PressurePlateBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, Kind kind, int maxWeight,
									   std::string sound)
	: RedstoneBehavior(std::move(context), std::move(ids)), _kind(kind), _maxWeight(maxWeight), _sound(std::move(sound)) {}

bool PressurePlateBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	int below = level.getBlockState(pos.below());
	// canSupportRigidBlock || canSupportCenter
	return _context->isFaceSturdy(below, Direction::Up, GameData::SupportType::Rigid) || _context->isFaceSturdy(below, Direction::Up, GameData::SupportType::Center);
}

int PressurePlateBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	return direction == Direction::Down && !canSurvive(level, pos, state) ? _ids->air : state;
}

int PressurePlateBlock::signalForState(int state) const {
	if (_kind == Kind::Weighted) return blocks().getInt(state, _ids->power);
	return blocks().getBool(state, _ids->powered) ? 15 : 0;
}

int PressurePlateBlock::withSignal(int state, int signal) const {
	if (_kind == Kind::Weighted) return blocks().withInt(state, _ids->power, signal);
	return blocks().withBool(state, _ids->powered, signal > 0);
}

// TOUCH_AABB: the plate's surface, 1/16 in from the sides, 4/16 high
int PressurePlateBlock::signalStrength(Level& level, const BlockPos& pos) const {
	AABB touch{pos.x + 1.0 / 16, static_cast<double>(pos.y), pos.z + 1.0 / 16, pos.x + 15.0 / 16, pos.y + 0.25, pos.z + 15.0 / 16};
	if (_kind != Kind::Weighted) return level.countEntities(touch, _kind == Kind::Mobs) > 0 ? 15 : 0;
	int count = std::min(level.countEntities(touch, false), _maxWeight);
	if (count <= 0) return 0;
	float ratio = static_cast<float>(std::min(_maxWeight, count)) / _maxWeight;
	return static_cast<int>(std::ceil(ratio * 15.0F));
}

void PressurePlateBlock::checkPressed(Level& level, const BlockPos& pos, int state, int signal) const {
	int	 strength	= signalStrength(level, pos);
	bool wasPressed = signal > 0, pressed = strength > 0;
	if (signal != strength) {
		level.setBlock(pos, withSignal(state, strength), Level::UPDATE_CLIENTS);
		updateNeighbours(level, pos);
	}
	if (!pressed && wasPressed) {
		level.playSound(nullptr, pos, _sound + ".click_off", Level::SoundSource::Blocks);
	} else if (pressed && !wasPressed) {
		level.playSound(nullptr, pos, _sound + ".click_on", Level::SoundSource::Blocks);
	}
	if (pressed) level.scheduleTick(pos, blockOf(state), _kind == Kind::Weighted ? 10 : 20);
}

void PressurePlateBlock::tick(Level& level, const BlockPos& pos, int state) const {
	int signal = signalForState(state);
	if (signal > 0) checkPressed(level, pos, state, signal);
}

void PressurePlateBlock::entityInside(Level& level, const BlockPos& pos, int state, Entity*) const {
	int signal = signalForState(state);
	if (signal == 0) checkPressed(level, pos, state, signal);
}

void PressurePlateBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const {
	if (!movedByPiston && signalForState(state) > 0) updateNeighbours(level, pos);
}

void PressurePlateBlock::updateNeighbours(Level& level, const BlockPos& pos) const {
	int block = blockOf(level.getBlockState(pos));
	level.updateNeighborsAt(pos, block);
	level.updateNeighborsAt(pos.below(), block);
}

int PressurePlateBlock::getSignal(Level&, const BlockPos&, int state, Direction) const { return signalForState(state); }

int PressurePlateBlock::getDirectSignal(Level&, const BlockPos&, int state, Direction direction) const {
	return direction == Direction::Up ? signalForState(state) : 0;
}

// ===== Observer =====

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

// ===== Lamp and target =====

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

// ===== Doors, trapdoors, fence gates =====

PoweredDoorBlock::PoweredDoorBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool openByHand, std::string sound)
	: DoorBlock(std::move(context)), _ids(std::move(ids)), _openByHand(openByHand), _sound(std::move(sound)) {}

bool PoweredDoorBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	int below = level.getBlockState(pos.below());
	if (_context->blocks.get(state, _context->half) == _context->lower) return _context->isFaceSturdy(below, Direction::Up);
	return _context->blocks.blockOf(below) == _context->blocks.blockOf(state);
}

void PoweredDoorBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool) const {
	const BlockRegistry& b		 = _context->blocks;
	bool				 lower	 = b.get(state, _context->half) == _context->lower;
	bool				 powered = level.hasNeighborSignal(pos) || level.hasNeighborSignal(pos.relative(lower ? Direction::Up : Direction::Down));
	if (sourceBlock == b.blockOf(state) || powered == b.getBool(state, _ids->powered)) return;
	if (powered != b.getBool(state, _ids->open)) {
		level.playSound(nullptr, pos, _sound + (powered ? ".open" : ".close"), Level::SoundSource::Blocks, 1.0F, openPitch(level));
	}
	level.setBlock(pos, b.withBool(b.withBool(state, _ids->powered, powered), _ids->open, powered), Level::UPDATE_CLIENTS);
}

bool PoweredDoorBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (!_openByHand) return false;
	state = _context->blocks.cycle(state, _ids->open);
	level.setBlock(pos, state, Level::UPDATE_CLIENTS | Level::UPDATE_IMMEDIATE);
	bool open = _context->blocks.getBool(state, _ids->open);
	level.playSound(&player, pos, _sound + (open ? ".open" : ".close"), Level::SoundSource::Blocks, 1.0F, openPitch(level));
	return true;
}

TrapDoorBlock::TrapDoorBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool openByHand, std::string sound)
	: RedstoneBehavior(std::move(context), std::move(ids)), _openByHand(openByHand), _sound(std::move(sound)) {}

int TrapDoorBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	int		  state = blocks().defaultState(context.block);
	Direction face	= context.clickedFace;
	int		  top = blocks().value("top"), bottom = blocks().value("bottom");
	if (!context.replaceClicked && face != Direction::Up && face != Direction::Down) {
		state = blocks().with(state, _context->facing, _context->directionValue(face));
		state = blocks().with(state, _context->half, context.clickY - context.clickedPos.y > 0.5 ? top : bottom);
	} else {
		state = blocks().with(state, _context->facing, _context->directionValue(Directions::opposite(context.horizontalDirection())));
		state = blocks().with(state, _context->half, face == Direction::Up ? bottom : top);
	}
	if (level.hasNeighborSignal(context.clickedPos)) state = blocks().withBool(blocks().withBool(state, _ids->open, true), _ids->powered, true);
	return blocks().withBool(state, _context->waterlogged, level.getFluidState(context.clickedPos).type == level.fluids().water());
}

void TrapDoorBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	bool powered = level.hasNeighborSignal(pos);
	if (powered == blocks().getBool(state, _ids->powered)) return;
	if (blocks().getBool(state, _ids->open) != powered) {
		state = blocks().withBool(state, _ids->open, powered);
		level.playSound(nullptr, pos, _sound + (powered ? ".open" : ".close"), Level::SoundSource::Blocks, 1.0F, openPitch(level));
	}
	level.setBlock(pos, blocks().withBool(state, _ids->powered, powered), Level::UPDATE_CLIENTS);
	level.scheduleWaterlogged(pos, state);
}

bool TrapDoorBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (!_openByHand) return false;
	state = blocks().cycle(state, _ids->open);
	level.setBlock(pos, state, Level::UPDATE_CLIENTS);
	level.scheduleWaterlogged(pos, state);
	level.playSound(&player, pos, _sound + (blocks().getBool(state, _ids->open) ? ".open" : ".close"), Level::SoundSource::Blocks, 1.0F, openPitch(level));
	return true;
}

FenceGateBlock::FenceGateBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, std::string sound)
	: RedstoneBehavior(std::move(context), std::move(ids)), _sound(std::move(sound)) {
	_walls = _context->tag("minecraft:walls");
}

int FenceGateBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	BlockPos  pos	  = context.clickedPos;
	bool	  powered = level.hasNeighborSignal(pos);
	Direction facing  = context.horizontalDirection();
	bool	  alongZ  = facing == Direction::North || facing == Direction::South;
	bool	  inWall  = alongZ ? isWall(level.getBlockState(pos.west())) || isWall(level.getBlockState(pos.east()))
						   : isWall(level.getBlockState(pos.north())) || isWall(level.getBlockState(pos.south()));
	int state = blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(facing));
	state	  = blocks().withBool(blocks().withBool(state, _ids->open, powered), _ids->powered, powered);
	return blocks().withBool(state, _ids->inWall, inWall);
}

int FenceGateBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int neighborState) const {
	if (Shapes::axisOf(clockWise(_context->direction(state, _context->facing))) != Shapes::axisOf(direction)) return state;
	bool inWall = isWall(neighborState) || isWall(level.getBlockState(pos.relative(Directions::opposite(direction))));
	return blocks().withBool(state, _ids->inWall, inWall);
}

void FenceGateBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	bool powered = level.hasNeighborSignal(pos);
	if (blocks().getBool(state, _ids->powered) == powered) return;
	level.setBlock(pos, blocks().withBool(blocks().withBool(state, _ids->powered, powered), _ids->open, powered), Level::UPDATE_CLIENTS);
	if (blocks().getBool(state, _ids->open) != powered) {
		level.playSound(nullptr, pos, _sound + (powered ? ".open" : ".close"), Level::SoundSource::Blocks, 1.0F, openPitch(level));
	}
}

bool FenceGateBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (blocks().getBool(state, _ids->open)) {
		state = blocks().withBool(state, _ids->open, false);
	} else {
		// Opens away from the player
		PlaceContext view{};
		view.yaw			 = player.getYaw();
		Direction playerFacing = view.horizontalDirection();
		if (_context->direction(state, _context->facing) == Directions::opposite(playerFacing)) {
			state = blocks().with(state, _context->facing, _context->directionValue(playerFacing));
		}
		state = blocks().withBool(state, _ids->open, true);
	}
	level.setBlock(pos, state, Level::UPDATE_CLIENTS | Level::UPDATE_IMMEDIATE);
	bool open = blocks().getBool(state, _ids->open);
	level.playSound(&player, pos, _sound + (open ? ".open" : ".close"), Level::SoundSource::Blocks, 1.0F, openPitch(level));
	return true;
}

// ===== Analog outputs =====

AnalogOutputBlock::AnalogOutputBlock(std::shared_ptr<const BlockContext> context, Kind kind) : _context(std::move(context)), _kind(kind) {
	const char* names[] = {"bites", "", "level", "", "eye", "charges", "honey_level"};
	_property			= names[static_cast<int>(kind)][0] ? _context->blocks.property(names[static_cast<int>(kind)]) : -1;
}

int AnalogOutputBlock::getAnalogOutputSignal(Level&, const BlockPos&, int state, Direction) const {
	const BlockRegistry& b = _context->blocks;
	switch (_kind) {
	case Kind::Cake:
		return (7 - b.getInt(state, _property)) * 2;
	case Kind::CandleCake:
		return 14;
	case Kind::Level:
	case Kind::HoneyLevel:
		return b.getInt(state, _property);
	case Kind::LavaCauldron:
		return 3;
	case Kind::EndPortalFrame:
		return b.getBool(state, _property) ? 15 : 0;
	case Kind::RespawnAnchor:
		return static_cast<int>(std::floor(b.getInt(state, _property) / 4.0F * 15));
	}
	return 0;
}

// ===== Facing of directional blocks =====

int FacingPlacement::getStateForPlacement(Level&, const PlaceContext& context) const {
	Direction facing;
	switch (_rule) {
	case Rule::NearestOpposite:
		facing = Directions::opposite(context.nearestLookingDirection());
		break;
	case Rule::HorizontalOpposite:
		facing = Directions::opposite(context.horizontalDirection());
		break;
	case Rule::HorizontalClockwise:
		facing = clockWise(context.horizontalDirection());
		break;
	case Rule::ClickedFace:
		facing = context.clickedFace;
		break;
	case Rule::Hopper: {
		Direction into = Directions::opposite(context.clickedFace);
		facing		   = into == Direction::Up || into == Direction::Down ? Direction::Down : into;
		break;
	}
	}
	const BlockRegistry& blocks = _context->blocks;
	int					 state	= blocks.with(blocks.defaultState(context.block), _context->facing, _context->directionValue(facing));
	return state >= 0 ? state : GENERIC_PLACEMENT;
}
