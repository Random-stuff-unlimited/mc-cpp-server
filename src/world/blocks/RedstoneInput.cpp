#include "world/blocks/Redstone.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"

#include <algorithm>
#include <cmath>

// The blocks the player presses: FaceAttachedBlock (levers and buttons) and PressurePlateBlock

// FaceAttachedHorizontalDirectionalBlock: levers and buttons, on a floor, a wall or a ceiling

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

// BasePressurePlateBlock: pressed by entities on it. Wooden plates: any entity, stone ones: players only (no mobs
// yet), weighted plates: power by the number of entities

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