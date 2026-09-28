#include "world/blocks/Redstone.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/Shapes.hpp"

#include <algorithm>

// DiodeBlock (repeaters and comparators), RepeaterBlock and ComparatorBlock

namespace {
	// Player.mayBuild: not in adventure or spectator mode
	bool mayBuild(const Player& player) { return player.getGameMode() == GameMode::Survival || player.getGameMode() == GameMode::Creative; }
} // namespace

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