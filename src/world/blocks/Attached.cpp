#include "world/blocks/Attached.hpp"

#include "world/Level.hpp"
#include "world/Shapes.hpp"

// ----- Torches -----

bool TorchBlock::canSurvive(Level& level, const BlockPos& pos, int) const { return _context->canSupportCenter(level, pos.below(), Direction::Up); }

int TorchBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	if (direction == Direction::Down && !canSurvive(level, pos, state)) return _air;
	return state;
}

bool WallTorchBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	Direction facing = _context->direction(state, _context->facing);
	return _context->isFaceSturdy(level.getBlockState(pos.relative(Directions::opposite(facing))), facing);
}

int WallTorchBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	if (Directions::opposite(direction) == _context->direction(state, _context->facing) && !canSurvive(level, pos, state)) return _air;
	return state;
}

// Attached to the block below, or above when hanging
bool LanternBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	Direction toSupport = _context->blocks.getBool(state, _context->hanging) ? Direction::Up : Direction::Down;
	return _context->canSupportCenter(level, pos.relative(toSupport), Directions::opposite(toSupport));
}

int LanternBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	level.scheduleWaterlogged(pos, state);
	Direction toSupport = _context->blocks.getBool(state, _context->hanging) ? Direction::Up : Direction::Down;
	if (toSupport == direction && !canSurvive(level, pos, state)) return _air;
	return state;
}

bool LadderBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	Direction facing = _context->direction(state, _context->facing);
	return _context->isFaceSturdy(level.getBlockState(pos.relative(Directions::opposite(facing))), facing);
}

int LadderBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	if (Directions::opposite(direction) == _context->direction(state, _context->facing) && !canSurvive(level, pos, state)) return _air;
	level.scheduleWaterlogged(pos, state);
	return state;
}

bool CarpetBlock::canSurvive(Level& level, const BlockPos& pos, int) const { return !_context->isAir(level.getBlockState(pos.below())); }

int CarpetBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	return canSurvive(level, pos, state) ? state : _air;
}

// ----- Snow layers -----

SnowLayerBlock::SnowLayerBlock(std::shared_ptr<const BlockContext> context) : AttachedBlock(std::move(context)) {
	_cannotSurviveOn = _context->tag("minecraft:snow_layer_cannot_survive_on");
	_canSurviveOn	 = _context->tag("minecraft:snow_layer_can_survive_on");
	_snow			 = _context->block("minecraft:snow");
}

bool SnowLayerBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	int below = level.getBlockState(pos.below());
	if (_context->inTag(_cannotSurviveOn, below)) return false;
	if (_context->inTag(_canSurviveOn, below)) return true;
	return Shapes::isFaceFull(_context->data.getCollisionShape(below), Direction::Up) ||
		   (_context->is(below, _snow) && _context->blocks.getInt(below, _context->layers) == 8);
}

int SnowLayerBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	return canSurvive(level, pos, state) ? state : _air;
}

void SnowLayerBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (level.getBlockLight(pos) > 11) {
		level.dropResources(state, pos);
		level.removeBlock(pos, false);
	}
}

// ----- Sugar cane and cactus -----

SugarCaneBlock::SugarCaneBlock(std::shared_ptr<const BlockContext> context) : AttachedBlock(std::move(context)) {
	_dirt		= _context->tag("minecraft:dirt");
	_sand		= _context->tag("minecraft:sand");
	_frostedIce = _context->block("minecraft:frosted_ice");
}

bool SugarCaneBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	BlockPos below		= pos.below();
	int		 belowState = level.getBlockState(below);
	if (_context->blocks.blockOf(belowState) == _context->blocks.blockOf(state)) return true;
	if (!_context->inTag(_dirt, belowState) && !_context->inTag(_sand, belowState)) return false;
	for (Direction direction : Directions::HORIZONTAL) {
		BlockPos side	   = below.relative(direction);
		int		 sideState = level.getBlockState(side);
		if (level.fluids().isWater(level.fluids().stateOf(sideState).type) || _context->is(sideState, _frostedIce)) return true;
	}
	return false;
}

int SugarCaneBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	if (!canSurvive(level, pos, state)) level.scheduleTick(pos, _context->blocks.blockOf(state), 1);
	return state;
}

void SugarCaneBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (!canSurvive(level, pos, state)) level.destroyBlock(pos, true);
}

void SugarCaneBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	const BlockContext& c = *_context;
	if (!c.isAir(level.getBlockState(pos.above()))) return;
	int block  = c.blocks.blockOf(state);
	int height = 1;
	while (c.is(level.getBlockState(pos.relative(Direction::Down, height)), block)) height++;
	if (height >= 3) return;
	int age = c.blocks.getInt(state, c.age);
	constexpr int flags = Level::UPDATE_INVISIBLE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS;
	if (age == 15) {
		level.setBlock(pos.above(), c.blocks.defaultState(block), Level::UPDATE_ALL);
		level.setBlock(pos, c.blocks.withInt(state, c.age, 0), flags);
	} else {
		level.setBlock(pos, c.blocks.withInt(state, c.age, age + 1), flags);
	}
}

CactusBlock::CactusBlock(std::shared_ptr<const BlockContext> context) : AttachedBlock(std::move(context)) {
	_sand		  = _context->tag("minecraft:sand");
	_cactusFlower = _context->block("minecraft:cactus_flower");
}

bool CactusBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	for (Direction direction : Directions::HORIZONTAL) {
		int side = level.getBlockState(pos.relative(direction));
		if (_context->properties(side).solid || level.fluids().isLava(level.fluids().stateOf(side).type)) return false;
	}
	int below = level.getBlockState(pos.below());
	return (_context->blocks.blockOf(below) == _context->blocks.blockOf(state) || _context->inTag(_sand, below)) &&
		   !_context->properties(level.getBlockState(pos.above())).liquid;
}

int CactusBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	if (!canSurvive(level, pos, state)) level.scheduleTick(pos, _context->blocks.blockOf(state), 1);
	return state;
}

void CactusBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (!canSurvive(level, pos, state)) level.destroyBlock(pos, true);
}

void CactusBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	const BlockContext& c	  = *_context;
	BlockPos			above = pos.above();
	if (!c.isAir(level.getBlockState(above))) return;
	int block  = c.blocks.blockOf(state);
	int height = 1;
	int age	   = c.blocks.getInt(state, c.age);
	while (c.is(level.getBlockState(pos.relative(Direction::Down, height)), block)) {
		if (++height == 3 && age == 15) return;
	}
	constexpr int flags = Level::UPDATE_INVISIBLE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS;
	if (age == 8 && canSurvive(level, above, c.blocks.defaultState(block))) {
		double chance = height >= 3 ? 0.25 : 0.1;
		if (level.random().nextDouble() <= chance) level.setBlock(above, c.blocks.defaultState(_cactusFlower), Level::UPDATE_ALL);
	} else if (age == 15 && height < 3) {
		level.setBlock(above, c.blocks.defaultState(block), Level::UPDATE_ALL);
		int reset = c.blocks.withInt(state, c.age, 0);
		level.setBlock(pos, reset, flags);
		// Vanilla runs the neighborChanged of the young cactus's state at the new block: a cactus does nothing there
		level.neighborChanged(reset, above, block, false);
	}
	if (age < 15) level.setBlock(pos, c.blocks.withInt(state, c.age, age + 1), flags);
}

// ----- Doors and beds -----

int DoorBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int neighborState) const {
	const BlockContext& c	  = *_context;
	int					half  = c.blocks.get(state, c.half);
	bool				lower = half == c.lower;
	bool				vertical = direction == Direction::Up || direction == Direction::Down;
	if (!vertical || lower != (direction == Direction::Up)) {
		if (lower && direction == Direction::Down && !c.isFaceSturdy(level.getBlockState(pos.below()), Direction::Up)) return _air;
		return state;
	}
	// The other half: copy its state (open, powered...), with our half
	if (c.data.isInstanceOf(c.blocks.blockOf(neighborState), "DoorBlock") && c.blocks.get(neighborState, c.half) != half) {
		return c.blocks.with(neighborState, c.half, half);
	}
	return _air;
}

int BedBlock::updateShape(Level&, const BlockPos&, int state, Direction direction, const BlockPos&, int neighborState) const {
	const BlockContext& c	   = *_context;
	Direction			facing = c.direction(state, c.facing);
	// The foot's other half is in the facing direction, the head's behind
	Direction toOther = c.blocks.get(state, c.part) == c.foot ? facing : Directions::opposite(facing);
	if (direction != toOther) return state;
	if (c.blocks.blockOf(neighborState) == c.blocks.blockOf(state) && c.blocks.get(neighborState, c.part) != c.blocks.get(state, c.part)) {
		return c.blocks.with(state, c.occupied, c.blocks.get(neighborState, c.occupied));
	}
	return _air;
}
