#include "world/blocks/Redstone.hpp"

#include "world/Level.hpp"
#include "world/PlaceContext.hpp"

// RedstoneTorchBlock and RedstoneWallTorchBlock: off when their support is powered, burn out after 8 toggles in 60
// ticks

namespace {
	constexpr int LEVEL_EVENT_REDSTONE_TORCH_BURNOUT = 1502;
} // namespace

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