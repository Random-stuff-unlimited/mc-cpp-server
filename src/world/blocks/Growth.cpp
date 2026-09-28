#include "world/blocks/Growth.hpp"

#include "world/Level.hpp"

// ----- Cocoa -----

CocoaBlock::CocoaBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) { _jungleLogs = _context->tag("minecraft:jungle_logs"); }

bool CocoaBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	return _context->inTag(_jungleLogs, level.getBlockState(pos.relative(_context->direction(state, _context->facing))));
}

int CocoaBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	if (direction == _context->direction(state, _context->facing) && !canSurvive(level, pos, state)) return _context->defaultState("minecraft:air");
	return state;
}

void CocoaBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (level.random().nextInt(5) != 0) return;
	int age = _context->blocks.getInt(state, _context->age);
	if (age < 2) level.setBlock(pos, _context->blocks.withInt(state, _context->age, age + 1), Level::UPDATE_CLIENTS);
}

// ----- Kelp and vines -----

GrowingPlantBlock::GrowingPlantBlock(std::shared_ptr<const BlockContext> context, Kind kind, bool head)
	: _context(std::move(context)), _kind(kind), _isHead(head) {
	const BlockContext& c = *_context;
	const char*			names[4][2] = {{"minecraft:kelp", "minecraft:kelp_plant"},
									   {"minecraft:weeping_vines", "minecraft:weeping_vines_plant"},
									   {"minecraft:twisting_vines", "minecraft:twisting_vines_plant"},
									   {"minecraft:cave_vines", "minecraft:cave_vines_plant"}};
	_head		 = c.block(names[static_cast<int>(kind)][0]);
	_body		 = c.block(names[static_cast<int>(kind)][1]);
	_growth		 = kind == Kind::Kelp || kind == Kind::TwistingVines ? Direction::Up : Direction::Down;
	_probability = kind == Kind::Kelp ? 0.14 : 0.1;
	_fluidTicks	 = kind == Kind::Kelp;
	_magma		 = c.block("minecraft:magma_block");
	_water		 = c.block("minecraft:water");
	_berries	 = c.blocks.property("berries");
}

bool GrowingPlantBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	BlockPos support	  = pos.relative(Directions::opposite(_growth));
	int		 supportState = level.getBlockState(support);
	if (_kind == Kind::Kelp && _context->is(supportState, _magma)) return false; // canAttachTo
	return isPlant(supportState) || _context->isFaceSturdy(supportState, _growth);
}

bool GrowingPlantBlock::canGrowInto(int state) const {
	// Kelp: any water block, even flowing
	return _kind == Kind::Kelp ? _context->is(state, _water) : _context->isAir(state);
}

int GrowingPlantBlock::convert(int from, int to) const {
	if (_kind != Kind::CaveVines) return to;
	return _context->blocks.with(to, _berries, _context->blocks.get(from, _berries));
}

void GrowingPlantBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (!canSurvive(level, pos, state)) level.destroyBlock(pos, true);
}

int GrowingPlantBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int neighborState) const {
	const BlockContext& c	  = *_context;
	Direction			behind = Directions::opposite(_growth);
	if (_isHead) {
		if (direction == behind) {
			if (!canSurvive(level, pos, state)) {
				level.scheduleTick(pos, _head, 1);
			} else if (isPlant(level.getBlockState(pos.relative(_growth)))) {
				return convert(state, c.blocks.defaultState(_body));
			}
		}
		if (direction == _growth && isPlant(neighborState)) return convert(state, c.blocks.defaultState(_body));
	} else {
		if (direction == behind && !canSurvive(level, pos, state)) level.scheduleTick(pos, _body, 1);
		if (direction == _growth && !isPlant(neighborState)) {
			// The head's getStateForPlacement(random): a random age
			int head = c.blocks.withInt(c.blocks.defaultState(_head), c.age, level.random().nextInt(25));
			return convert(state, head);
		}
	}
	if (_fluidTicks) level.scheduleFluidTick(pos, level.fluids().water(), level.fluids().tickDelay(level.fluids().water()));
	return state;
}

void GrowingPlantBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (!_isHead) return;
	const BlockContext& c	= *_context;
	int					age = c.blocks.getInt(state, c.age);
	if (age >= 25 || level.random().nextDouble() >= _probability) return;
	BlockPos target = pos.relative(_growth);
	if (!canGrowInto(level.getBlockState(target))) return;
	int grown = c.blocks.cycle(state, c.age);
	// Cave vines: a new piece has berries 11% of the time
	if (_kind == Kind::CaveVines) grown = c.blocks.withBool(grown, _berries, level.random().nextFloat() < 0.11F);
	level.setBlock(target, grown, Level::UPDATE_ALL);
}

// ----- Grass, mycelium, podzol, nylium -----

SnowyDirtBlock::SnowyDirtBlock(std::shared_ptr<const BlockContext> context, bool spreading) : _context(std::move(context)), _spreading(spreading) {
	_snowy	   = _context->blocks.property("snowy");
	_dirt	   = _context->defaultState("minecraft:dirt");
	_snowBlock = _context->block("minecraft:snow");
	_snowTag   = _context->tag("minecraft:snow");
}

int SnowyDirtBlock::updateShape(Level&, const BlockPos&, int state, Direction direction, const BlockPos&, int neighborState) const {
	if (direction != Direction::Up) return state;
	return _context->blocks.withBool(state, _snowy, _context->inTag(_snowTag, neighborState));
}

// SpreadingSnowyDirtBlock.canBeGrass: one layer of snow, or light still getting through what is above
bool SnowyDirtBlock::canBeGrass(Level& level, const BlockPos& pos, int state) const {
	const BlockContext& c	  = *_context;
	int					above = level.getBlockState(pos.above());
	if (c.is(above, _snowBlock) && c.blocks.getInt(above, c.layers) == 1) return true;
	if (level.fluids().stateOf(above).amount == 8) return false;
	return level.getLightBlockInto(state, above, Direction::Up, c.properties(above).lightBlock) < 15;
}

void SnowyDirtBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (!_spreading) return;
	const BlockContext& c = *_context;
	if (!canBeGrass(level, pos, state)) {
		level.setBlock(pos, _dirt, Level::UPDATE_ALL);
		return;
	}
	if (level.getMaxLocalRawBrightness(pos.above()) < 9) return;
	int			spread = c.blocks.defaultState(c.blocks.blockOf(state));
	JavaRandom& random = level.random();
	for (int i = 0; i < 4; i++) {
		int		 x		= random.nextInt(3) - 1;
		int		 y		= random.nextInt(5) - 3;
		int		 z		= random.nextInt(3) - 1;
		BlockPos target = pos.offset(x, y, z);
		if (level.getBlockState(target) != _dirt) continue; // Plain dirt only (a single state)
		// canPropagate: could be grass there, and no water above
		if (!canBeGrass(level, target, spread) || level.fluids().isWater(level.getFluidState(target.above()).type)) continue;
		level.setBlock(target, c.blocks.withBool(spread, _snowy, c.inTag(_snowTag, level.getBlockState(target.above()))), Level::UPDATE_ALL);
	}
}

NyliumBlock::NyliumBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) {
	_netherrack = _context->defaultState("minecraft:netherrack");
}

void NyliumBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	int above = level.getBlockState(pos.above());
	if (level.getLightBlockInto(state, above, Direction::Up, _context->properties(above).lightBlock) >= 15) {
		level.setBlock(pos, _netherrack, Level::UPDATE_ALL);
	}
}

// ----- Farmland -----

FarmBlock::FarmBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) {
	_moisture		   = _context->blocks.property("moisture");
	_dirt			   = _context->defaultState("minecraft:dirt");
	_maintainsFarmland = _context->tag("minecraft:maintains_farmland");
}

bool FarmBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	int above = level.getBlockState(pos.above());
	int block = _context->blocks.blockOf(above);
	return !_context->properties(above).solid || _context->data.isInstanceOf(block, "FenceGateBlock") ||
		   _context->data.isInstanceOf(block, "MovingPistonBlock");
}

int FarmBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	if (direction == Direction::Up && !canSurvive(level, pos, state)) level.scheduleTick(pos, _context->blocks.blockOf(state), 1);
	return state;
}

// turnToDirt: vanilla also pushes the entities standing on it up by the height difference
void FarmBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (!canSurvive(level, pos, state)) level.setBlock(pos, _dirt, Level::UPDATE_ALL);
}

void FarmBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	int	 moisture = _context->blocks.getInt(state, _moisture);
	bool nearWater = false;
	for (int x = -4; x <= 4 && !nearWater; x++) {
		for (int y = 0; y <= 1 && !nearWater; y++) {
			for (int z = -4; z <= 4 && !nearWater; z++) nearWater = level.fluids().isWater(level.getFluidState(pos.offset(x, y, z)).type);
		}
	}
	// No weather yet: never raining on it
	if (!nearWater) {
		if (moisture > 0) {
			level.setBlock(pos, _context->blocks.withInt(state, _moisture, moisture - 1), Level::UPDATE_CLIENTS);
		} else if (!_context->inTag(_maintainsFarmland, level.getBlockState(pos.above()))) {
			level.setBlock(pos, _dirt, Level::UPDATE_ALL);
		}
	} else if (moisture < 7) {
		level.setBlock(pos, _context->blocks.withInt(state, _moisture, 7), Level::UPDATE_CLIENTS);
	}
}

// ----- Leaves -----

LeavesBlock::LeavesBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) {
	_distance	= _context->blocks.property("distance");
	_persistent = _context->blocks.property("persistent");
	_logs		= _context->tag("minecraft:logs");
	_leaves.assign(_context->data.getBlockCount(), false);
	for (size_t block = 0; block < _leaves.size(); block++) _leaves[block] = _context->data.isInstanceOf(static_cast<int>(block), "LeavesBlock");
}

int LeavesBlock::distanceAt(int state) const {
	if (_context->inTag(_logs, state)) return 0;
	return _leaves[_context->blocks.blockOf(state)] ? _context->blocks.getInt(state, _distance) : 7;
}

int LeavesBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int neighborState) const {
	level.scheduleWaterlogged(pos, state);
	int distance = distanceAt(neighborState) + 1;
	if (distance != 1 || _context->blocks.getInt(state, _distance) != distance) level.scheduleTick(pos, _context->blocks.blockOf(state), 1);
	return state;
}

void LeavesBlock::tick(Level& level, const BlockPos& pos, int state) const {
	int distance = 7;
	for (Direction direction : Directions::ALL) {
		distance = std::min(distance, distanceAt(level.getBlockState(pos.relative(direction))) + 1);
		if (distance == 1) break;
	}
	level.setBlock(pos, _context->blocks.withInt(state, _distance, distance), Level::UPDATE_ALL);
}

void LeavesBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (!_context->blocks.getBool(state, _persistent) && _context->blocks.getInt(state, _distance) == 7) {
		level.dropResources(state, pos);
		level.removeBlock(pos, false);
	}
}

// ----- Ice, redstone ore, amethyst -----

IceBlock::IceBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) { _water = _context->defaultState("minecraft:water"); }

void IceBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (level.getBlockLight(pos) <= 11 - _context->properties(state).lightBlock) return;
	// melt: nothing left in the nether
	if (level.isUltraWarm()) {
		level.removeBlock(pos, false);
		return;
	}
	level.setBlock(pos, _water, Level::UPDATE_ALL);
	level.neighborChanged(pos, _context->blocks.blockOf(_water));
}

RedStoneOreBlock::RedStoneOreBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) { _lit = _context->blocks.property("lit"); }

void RedStoneOreBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (_context->blocks.getBool(state, _lit)) level.setBlock(pos, _context->blocks.withBool(state, _lit, false), Level::UPDATE_ALL);
}

BuddingAmethystBlock::BuddingAmethystBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) {
	_small	 = _context->block("minecraft:small_amethyst_bud");
	_medium	 = _context->block("minecraft:medium_amethyst_bud");
	_large	 = _context->block("minecraft:large_amethyst_bud");
	_cluster = _context->block("minecraft:amethyst_cluster");
	_water	 = _context->block("minecraft:water");
}

void BuddingAmethystBlock::randomTick(Level& level, const BlockPos& pos, int) const {
	const BlockContext& c = *_context;
	if (level.random().nextInt(5) != 0) return;
	Direction direction = Directions::ALL[level.random().nextInt(6)];
	BlockPos  target	= pos.relative(direction);
	int		  state		= level.getBlockState(target);
	bool	  facing	= c.blocks.has(state, c.facing) && c.direction(state, c.facing) == direction;
	int		  grown		= -1;
	if (c.isAir(state) || (c.is(state, _water) && level.fluids().stateOf(state).amount == 8)) {
		grown = _small;
	} else if (c.is(state, _small) && facing) {
		grown = _medium;
	} else if (c.is(state, _medium) && facing) {
		grown = _large;
	} else if (c.is(state, _large) && facing) {
		grown = _cluster;
	}
	if (grown < 0) return;
	int bud = c.blocks.with(c.blocks.defaultState(grown), c.facing, c.directionValue(direction));
	bud		= c.blocks.withBool(bud, c.waterlogged, level.fluids().stateOf(state).type == level.fluids().water());
	level.setBlock(target, bud, Level::UPDATE_ALL);
}

bool AmethystClusterBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	Direction facing = _context->direction(state, _context->facing);
	return _context->isFaceSturdy(level.getBlockState(pos.relative(Directions::opposite(facing))), facing);
}

int AmethystClusterBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	level.scheduleWaterlogged(pos, state);
	Direction facing = _context->direction(state, _context->facing);
	if (direction == Directions::opposite(facing) && !canSurvive(level, pos, state)) return _context->defaultState("minecraft:air");
	return state;
}

// ----- Copper -----

void WeatheringBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	JavaRandom& random = level.random();
	if (random.nextFloat() >= 0.05688889F || _next < 0) return;
	const std::vector<int8_t>& ages = *_ages;
	int						   age	= ages[_context->blocks.blockOf(state)];
	int						   same = 0, more = 0;
	// Every copper within a manhattan distance of 4: none less oxidized, the more oxidized ones make it likelier
	for (int x = -4; x <= 4; x++) {
		for (int y = -4; y <= 4; y++) {
			for (int z = -4; z <= 4; z++) {
				if (std::abs(x) + std::abs(y) + std::abs(z) > 4 || (x == 0 && y == 0 && z == 0)) continue;
				int other = ages[_context->blocks.blockOf(level.getBlockState(pos.offset(x, y, z)))];
				if (other < 0) continue;
				if (other < age) return;
				if (other > age) {
					more++;
				} else {
					same++;
				}
			}
		}
	}
	float ratio	 = static_cast<float>(more + 1) / (more + same + 1);
	float chance = ratio * ratio * (age == 0 ? 0.75F : 1.0F);
	if (random.nextFloat() < chance) level.setBlock(pos, _context->blocks.withPropertiesOf(_next, state), Level::UPDATE_ALL);
}
