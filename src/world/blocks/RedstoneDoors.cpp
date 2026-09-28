#include "world/blocks/Redstone.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/Shapes.hpp"

// Doors, trapdoors and fence gates: open with power, or by hand (not the iron ones)

namespace {
	// The pitch of doors, trapdoors and fence gates: level.getRandom().nextFloat() * 0.1F + 0.9F
	float openPitch(Level& level) { return level.random().nextFloat() * 0.1F + 0.9F; }
} // namespace

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