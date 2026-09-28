#include "world/Fluids.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"
#include "world/Shapes.hpp"

#include <algorithm>
#include <cmath>

namespace {
	constexpr int	LEVEL_EVENT_LAVA_FIZZ = 1501;
	constexpr float LAVA_MIN_LEVEL_CUTOFF = 0.44444445f; // LavaFluid.MIN_LEVEL_CUTOFF
	constexpr int	NO_SLOPE			  = 1000;

	// LiquidBlock.POSSIBLE_FLOW_DIRECTIONS
	constexpr Direction FLOW_DIRECTIONS[5] = {Direction::Down, Direction::South, Direction::North, Direction::East, Direction::West};

} // namespace

Fluids::Fluids(Level& level, const GameData& gameData, bool ultraWarm) : _level(level), _gameData(gameData), _ultraWarm(ultraWarm) {
	auto fluid		= [&](const char* name) { return gameData.getStaticId("minecraft:fluid", name); };
	auto block		= [&](const char* name) { return gameData.getStaticId("minecraft:block", name); };
	_water			= fluid("minecraft:water");
	_flowingWater	= fluid("minecraft:flowing_water");
	_lava			= fluid("minecraft:lava");
	_flowingLava	= fluid("minecraft:flowing_lava");
	_waterBlock		= gameData.getDefaultBlockState("minecraft:water");
	_lavaBlock		= gameData.getDefaultBlockState("minecraft:lava");
	_air			= gameData.getDefaultBlockState("minecraft:air");
	_stone			= gameData.getDefaultBlockState("minecraft:stone");
	_cobblestone	= gameData.getDefaultBlockState("minecraft:cobblestone");
	_obsidian		= gameData.getDefaultBlockState("minecraft:obsidian");
	_basalt			= gameData.getDefaultBlockState("minecraft:basalt");
	_soulSoil		= block("minecraft:soul_soil");
	_blueIce		= block("minecraft:blue_ice");
	_levelProperty	= gameData.getBlocks().property("level");
	_waterlogged	= gameData.getBlocks().property("waterlogged");

	_blockFlags.assign(gameData.getBlockCount(), 0);
	for (int id = 0; id < static_cast<int>(gameData.getBlockCount()); id++) {
		uint8_t& flags = _blockFlags[id];
		if (gameData.isInstanceOf(id, "LiquidBlockContainer")) flags |= LIQUID_CONTAINER;
		if (gameData.isInstanceOf(id, "SimpleWaterloggedBlock")) flags |= SIMPLE_WATERLOGGED;
		if (gameData.isInstanceOf(id, "LiquidBlock")) flags |= LIQUID_BLOCK;
		if (gameData.isInstanceOf(id, "IceBlock")) flags |= ICE;
		if (gameData.isInstanceOf(id, "DoorBlock") || gameData.isInTag("minecraft:block", "minecraft:signs", id)) flags |= NO_FLUID_INSIDE;
	}
	for (const char* name : {"minecraft:ladder", "minecraft:sugar_cane", "minecraft:bubble_column", "minecraft:nether_portal", "minecraft:end_portal",
							 "minecraft:end_gateway", "minecraft:structure_void"}) {
		_blockFlags[block(name)] |= NO_FLUID_INSIDE;
	}
}

FluidState Fluids::stateOf(int blockState) const {
	const GameData::StateProperties& properties = _gameData.getStateProperties(blockState);
	return {properties.fluid, properties.fluidAmount, properties.fluidFalling};
}

bool Fluids::hasFlag(int blockState, BlockFlag flag) const { return _blockFlags[_gameData.getBlocks().blockOf(blockState)] & flag; }

Fluids::Kind Fluids::kindOf(int type) const {
	if (isLava(type)) return {_lava, _flowingLava, true};
	return {_water, _flowingWater, false};
}

// FlowingFluid.getLegacyLevel: 0 for a source, 8 - amount for a flowing fluid, + 8 when falling
int Fluids::legacyBlock(const FluidState& fluid) const {
	if (fluid.isEmpty()) return _air;
	int level = isSource(fluid) ? 0 : 8 - std::min(fluid.amount, 8) + (fluid.falling ? 8 : 0);
	return _gameData.getBlocks().withInt(isLava(fluid.type) ? _lavaBlock : _waterBlock, _levelProperty, level);
}

int Fluids::tickDelay(int type) const {
	if (isLava(type)) return _ultraWarm ? 10 : 30;
	return 5;
}

// FlowingFluid.getHeight: full when the same fluid is above
float Fluids::height(const FluidState& fluid, const BlockPos& pos) {
	if (fluid.isEmpty()) return 0.0f;
	FluidState above = stateOf(_level.getBlockState(pos.above()));
	if (kindOf(fluid.type).source == kindOf(above.type).source && !above.isEmpty()) return 1.0f;
	return fluid.amount / 9.0f;
}

Vec3 Fluids::flow(const BlockPos& pos, const FluidState& fluid) {
	Kind  kind		= kindOf(fluid.type);
	float ownHeight = fluid.amount / 9.0f;
	auto  affectsFlow = [&](const FluidState& other) { return other.isEmpty() || sameKind(kind, other); };
	double x = 0.0, z = 0.0;
	for (Direction direction : Directions::HORIZONTAL) {
		BlockPos   neighbor		 = pos.relative(direction);
		int		   neighborState = _level.getBlockState(neighbor);
		FluidState other		 = stateOf(neighborState);
		if (!affectsFlow(other)) continue;
		float otherHeight = other.isEmpty() ? 0.0f : other.amount / 9.0f;
		float difference  = 0.0f;
		if (otherHeight == 0.0f) {
			if (!_gameData.getStateProperties(neighborState).blocksMotion) {
				FluidState below = stateOf(_level.getBlockState(neighbor.below()));
				if (affectsFlow(below)) {
					otherHeight = below.isEmpty() ? 0.0f : below.amount / 9.0f;
					if (otherHeight > 0.0f) difference = ownHeight - (otherHeight - 0.8888889f);
				}
			}
		} else if (otherHeight > 0.0f) {
			difference = ownHeight - otherHeight;
		}
		if (difference != 0.0f) {
			x += Directions::OFFSETS[static_cast<int>(direction)][0] * difference;
			z += Directions::OFFSETS[static_cast<int>(direction)][2] * difference;
		}
	}
	Vec3 flow{x, 0.0, z};
	if (fluid.falling) {
		for (Direction direction : Directions::HORIZONTAL) {
			BlockPos neighbor = pos.relative(direction);
			if (isSolidFace(kind, neighbor, direction) || isSolidFace(kind, neighbor.above(), direction)) {
				flow	= flow.normalize();
				flow.y -= 6.0;
				break;
			}
		}
	}
	return flow.normalize();
}

bool Fluids::isSolidFace(const Kind& kind, const BlockPos& pos, Direction direction) {
	int state = _level.getBlockState(pos);
	if (sameKind(kind, stateOf(state))) return false;
	if (direction == Direction::Up) return true;
	if (hasFlag(state, ICE)) return false;
	return _gameData.isFaceSturdy(state, static_cast<int>(direction), GameData::SupportType::Full);
}

// ----- Tick (FlowingFluid.tick) -----

void Fluids::tick(const BlockPos& pos, int blockState, FluidState fluid) {
	Kind kind = kindOf(fluid.type);
	if (!isSource(fluid)) {
		FluidState next	 = newLiquid(kind, pos, _level.getBlockState(pos));
		int		   delay = spreadDelay(kind, pos, fluid, next);
		if (next.isEmpty()) {
			fluid	   = next;
			blockState = _air;
			_level.setBlock(pos, blockState, Level::UPDATE_ALL);
		} else if (next != fluid) {
			fluid	   = next;
			blockState = legacyBlock(next);
			_level.setBlock(pos, blockState, Level::UPDATE_ALL);
			_level.scheduleFluidTick(pos, next.type, delay);
		}
	}
	spread(kind, pos, blockState, fluid);
}

int Fluids::spreadDelay(const Kind& kind, const BlockPos& pos, const FluidState& current, const FluidState& next) {
	int delay = tickDelay(kind.source);
	// LavaFluid.getSpreadDelay: lava rising slows down 3 times out of 4
	if (kind.lava && !current.isEmpty() && !next.isEmpty() && !current.falling && !next.falling && height(next, pos) > height(current, pos) &&
		_level.random().nextInt(4) != 0) {
		delay *= 4;
	}
	return delay;
}

void Fluids::spread(const Kind& kind, const BlockPos& pos, int blockState, const FluidState& fluid) {
	if (fluid.isEmpty()) return;
	BlockPos   below	  = pos.below();
	int		   belowState = _level.getBlockState(below);
	FluidState belowFluid = stateOf(belowState);
	if (canMaybePassThrough(kind, blockState, Direction::Down, belowState, belowFluid)) {
		FluidState next = newLiquid(kind, below, belowState);
		if (canBeReplacedWith(belowFluid, below, next.type, Direction::Down) && canPlaceLiquid(belowState, next.type)) {
			spreadTo(kind, below, belowState, Direction::Down, next);
			if (sourceNeighborCount(kind, pos) >= 3) spreadToSides(kind, pos, fluid, blockState);
			return;
		}
	}
	if (isSource(fluid) || !isWaterHole(kind, pos, blockState, below, belowState)) spreadToSides(kind, pos, fluid, blockState);
}

void Fluids::spreadToSides(const Kind& kind, const BlockPos& pos, const FluidState& fluid, int blockState) {
	int amount = fluid.falling ? 7 : fluid.amount - dropOff(kind);
	if (amount <= 0) return;
	for (const auto& [direction, next] : getSpread(kind, pos, blockState)) {
		BlockPos target = pos.relative(direction);
		spreadTo(kind, target, _level.getBlockState(target), direction, next);
	}
}

FluidState Fluids::newLiquid(const Kind& kind, const BlockPos& pos, int blockState) {
	int maxAmount = 0, sources = 0;
	for (Direction direction : Directions::HORIZONTAL) {
		BlockPos   neighbor = pos.relative(direction);
		int		   state	= _level.getBlockState(neighbor);
		FluidState fluid	= stateOf(state);
		if (sameKind(kind, fluid) && canPassThroughWall(direction, blockState, state)) {
			if (isSource(fluid)) sources++;
			maxAmount = std::max(maxAmount, fluid.amount);
		}
	}
	if (sources >= 2 && canConvertToSource(kind)) {
		int belowState = _level.getBlockState(pos.below());
		if (_gameData.getStateProperties(belowState).solid || isSourceOf(kind, stateOf(belowState))) return {kind.source, 8, false};
	}
	BlockPos   above	  = pos.above();
	int		   aboveState = _level.getBlockState(above);
	FluidState aboveFluid = stateOf(aboveState);
	if (!aboveFluid.isEmpty() && sameKind(kind, aboveFluid) && canPassThroughWall(Direction::Up, blockState, aboveState)) return {kind.flowing, 8, true};
	int amount = maxAmount - dropOff(kind);
	return amount <= 0 ? FluidState{} : FluidState{kind.flowing, amount, false};
}

void Fluids::spreadTo(const Kind& kind, const BlockPos& pos, int blockState, Direction direction, const FluidState& fluid) {
	// LavaFluid.spreadTo: lava falling on water makes stone
	if (kind.lava && direction == Direction::Down && isWater(stateOf(_level.getBlockState(pos)).type)) {
		if (hasFlag(blockState, LIQUID_BLOCK)) _level.setBlock(pos, _stone, Level::UPDATE_ALL);
		fizz(pos);
		return;
	}
	if (hasFlag(blockState, LIQUID_CONTAINER)) {
		// placeLiquid: only SimpleWaterloggedBlock takes water in
		if (hasFlag(blockState, SIMPLE_WATERLOGGED) && !_gameData.getBlocks().getBool(blockState, _waterlogged) && fluid.type == _water) {
			_level.setBlock(pos, _gameData.getBlocks().withBool(blockState, _waterlogged, true), Level::UPDATE_ALL);
			_level.scheduleFluidTick(pos, fluid.type, tickDelay(fluid.type));
		}
		return;
	}
	if (!_gameData.getBlocks().isAir(blockState)) {
		// beforeDestroyingBlock
		if (kind.lava) {
			fizz(pos);
		} else {
			_level.dropResources(blockState, pos);
		}
	}
	_level.setBlock(pos, legacyBlock(fluid), Level::UPDATE_ALL);
}

std::vector<std::pair<Direction, FluidState>> Fluids::getSpread(const Kind& kind, const BlockPos& pos, int blockState) {
	int											  best = NO_SLOPE;
	std::vector<std::pair<Direction, FluidState>> spread; // In Direction order, like vanilla's EnumMap
	SpreadContext								  context{pos, {}, {}};
	for (Direction direction : Directions::HORIZONTAL) {
		BlockPos   neighbor = pos.relative(direction);
		int		   state	= _level.getBlockState(neighbor);
		FluidState fluid	= stateOf(state);
		if (!canMaybePassThrough(kind, blockState, direction, state, fluid)) continue;
		FluidState next = newLiquid(kind, neighbor, state);
		if (!canPlaceLiquid(state, next.type)) continue;
		int distance = isHole(kind, context, neighbor) ? 0 : slopeDistance(kind, neighbor, 1, Directions::opposite(direction), state, context);
		if (distance < best) spread.clear();
		if (distance <= best) {
			if (canBeReplacedWith(fluid, neighbor, next.type, direction)) spread.emplace_back(direction, next);
			best = distance;
		}
	}
	std::sort(spread.begin(), spread.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	return spread;
}

int Fluids::slopeDistance(const Kind& kind, const BlockPos& pos, int depth, Direction from, int blockState, SpreadContext& context) {
	int best = NO_SLOPE;
	for (Direction direction : Directions::HORIZONTAL) {
		if (direction == from) continue;
		BlockPos neighbor = pos.relative(direction);
		int		 state	  = contextState(context, neighbor);
		if (!canPassThrough(kind, kind.flowing, blockState, direction, state, stateOf(state))) continue;
		if (isHole(kind, context, neighbor)) return depth;
		if (depth < slopeFindDistance(kind)) best = std::min(best, slopeDistance(kind, neighbor, depth + 1, Directions::opposite(direction), state, context));
	}
	return best;
}

int Fluids::contextState(SpreadContext& context, const BlockPos& pos) {
	int16_t key = static_cast<int16_t>(((pos.x - context.origin.x + 128) & 0xFF) << 8 | ((pos.z - context.origin.z + 128) & 0xFF));
	auto	it	= context.states.find(key);
	if (it != context.states.end()) return it->second;
	int state			= _level.getBlockState(pos);
	context.states[key] = state;
	return state;
}

bool Fluids::isHole(const Kind& kind, SpreadContext& context, const BlockPos& pos) {
	int16_t key = static_cast<int16_t>(((pos.x - context.origin.x + 128) & 0xFF) << 8 | ((pos.z - context.origin.z + 128) & 0xFF));
	auto	it	= context.holes.find(key);
	if (it != context.holes.end()) return it->second;
	int	 state			= contextState(context, pos);
	BlockPos below		= pos.below();
	bool hole			= isWaterHole(kind, pos, state, below, _level.getBlockState(below));
	context.holes[key]	= hole;
	return hole;
}

bool Fluids::isWaterHole(const Kind& kind, const BlockPos&, int blockState, const BlockPos&, int belowState) {
	if (!canPassThroughWall(Direction::Down, blockState, belowState)) return false;
	FluidState belowFluid = stateOf(belowState);
	return sameKind(kind, belowFluid) ? true : canHoldFluid(belowState, kind.flowing);
}

int Fluids::sourceNeighborCount(const Kind& kind, const BlockPos& pos) {
	int count = 0;
	for (Direction direction : Directions::HORIZONTAL) {
		if (isSourceOf(kind, stateOf(_level.getBlockState(pos.relative(direction))))) count++;
	}
	return count;
}

// ----- What fluids can go through -----

// FlowingFluid.canPassThroughWall: the collision shapes of both blocks must not close their shared face together
bool Fluids::canPassThroughWall(Direction direction, int blockState, int neighborState) {
	uint16_t neighborShape = _gameData.getStateProperties(neighborState).collisionShape;
	uint16_t shape		   = _gameData.getStateProperties(blockState).collisionShape;
	uint64_t key		   = static_cast<uint64_t>(shape) | static_cast<uint64_t>(neighborShape) << 16 | static_cast<uint64_t>(direction) << 32;
	auto	 cached		   = _wallCache.find(key);
	if (cached != _wallCache.end()) return cached->second;

	const std::vector<GameData::Box>& neighborBoxes = _gameData.getCollisionShape(neighborState);
	const std::vector<GameData::Box>& boxes			= _gameData.getCollisionShape(blockState);
	bool							  passes;
	if (Shapes::isFullBlock(neighborBoxes) || Shapes::isFullBlock(boxes)) {
		passes = false;
	} else if (boxes.empty() && neighborBoxes.empty()) {
		passes = true;
	} else {
		// Shapes.mergedFaceOccludes: the side of each block facing the other, together
		int								 axis	 = Shapes::axisOf(direction);
		bool							 positive = Shapes::isPositive(direction);
		std::vector<std::array<double, 4>> face;
		Shapes::faceRectangles(positive ? boxes : neighborBoxes, axis, true, face);
		Shapes::faceRectangles(positive ? neighborBoxes : boxes, axis, false, face);
		passes = !Shapes::coversSquare(face);
	}
	_wallCache[key] = passes;
	return passes;
}

bool Fluids::canMaybePassThrough(const Kind& kind, int blockState, Direction direction, int neighborState, const FluidState& neighborFluid) {
	return !isSourceOf(kind, neighborFluid) && canHoldAnyFluid(neighborState) && canPassThroughWall(direction, blockState, neighborState);
}

bool Fluids::canPassThrough(const Kind& kind, int type, int blockState, Direction direction, int neighborState, const FluidState& neighborFluid) {
	return canMaybePassThrough(kind, blockState, direction, neighborState, neighborFluid) && canPlaceLiquid(neighborState, type);
}

bool Fluids::canHoldAnyFluid(int blockState) const {
	if (hasFlag(blockState, LIQUID_CONTAINER)) return true;
	return !_gameData.getStateProperties(blockState).blocksMotion && !hasFlag(blockState, NO_FLUID_INSIDE);
}

// canHoldSpecificFluid: containers accept only what their canPlaceLiquid allows (waterloggable blocks: water source)
bool Fluids::canPlaceLiquid(int blockState, int type) const {
	if (!hasFlag(blockState, LIQUID_CONTAINER)) return true;
	return hasFlag(blockState, SIMPLE_WATERLOGGED) && type == _water;
}

bool Fluids::canBeReplacedWith(const FluidState& fluid, const BlockPos& pos, int type, Direction direction) {
	if (fluid.isEmpty()) return true;
	if (isWater(fluid.type)) return direction == Direction::Down && !isWater(type);
	return height(fluid, pos) >= LAVA_MIN_LEVEL_CUTOFF && isWater(type);
}

// ----- LiquidBlock -----

bool Fluids::shouldSpreadLiquid(const BlockPos& pos, int blockState) {
	if (!isLava(stateOf(blockState).type)) return true;
	bool onSoulSoil = _gameData.getBlocks().blockOf(_level.getBlockState(pos.below())) == _soulSoil;
	for (Direction direction : FLOW_DIRECTIONS) {
		BlockPos neighbor = pos.relative(Directions::opposite(direction));
		int		 state	  = _level.getBlockState(neighbor);
		if (isWater(stateOf(state).type)) {
			_level.setBlock(pos, isSource(stateOf(_level.getBlockState(pos))) ? _obsidian : _cobblestone, Level::UPDATE_ALL);
			fizz(pos);
			return false;
		}
		if (onSoulSoil && _gameData.getBlocks().blockOf(state) == _blueIce) {
			_level.setBlock(pos, _basalt, Level::UPDATE_ALL);
			fizz(pos);
			return false;
		}
	}
	return true;
}

void Fluids::fizz(const BlockPos& pos) { _level.levelEvent(nullptr, LEVEL_EVENT_LAVA_FIZZ, pos, 0); }
