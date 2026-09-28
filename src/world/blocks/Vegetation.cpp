#include "world/blocks/Vegetation.hpp"

#include "world/Level.hpp"
#include "world/Shapes.hpp"

VegetationBlock::VegetationBlock(std::shared_ptr<const BlockContext> context, Soil soil) : _context(std::move(context)), _soil(soil) {
	const BlockContext& c = *_context;
	_dirt				  = c.tag("minecraft:dirt");
	_nylium				  = c.tag("minecraft:nylium");
	_dryVegetationSoil	  = c.tag("minecraft:dry_vegetation_may_place_on");
	_mushroomGrowBlocks	  = c.tag("minecraft:mushroom_grow_block");
	_farmland			  = c.block("minecraft:farmland");
	_clay				  = c.block("minecraft:clay");
	_soulSoil			  = c.block("minecraft:soul_soil");
	_soulSand			  = c.block("minecraft:soul_sand");
	_mycelium			  = c.block("minecraft:mycelium");
	_netherrack			  = c.block("minecraft:netherrack");
	_cactus				  = c.block("minecraft:cactus");
	_magma				  = c.block("minecraft:magma_block");
	_mangroveLeaves		  = c.block("minecraft:mangrove_leaves");
}

bool VegetationBlock::mayPlaceOn(Level& level, const BlockPos& belowPos, int below) const {
	const BlockContext& c	 = *_context;
	bool				dirt = c.inTag(_dirt, below) || c.is(below, _farmland);
	switch (_soil) {
	case Soil::Dirt:
		return dirt;
	case Soil::DirtOrClay:
		return c.is(below, _clay) || dirt;
	case Soil::Nether:
		return c.inTag(_nylium, below) || c.is(below, _soulSoil) || dirt;
	case Soil::Fungus:
		return c.inTag(_nylium, below) || c.is(below, _mycelium) || c.is(below, _soulSoil) || dirt;
	case Soil::WitherRose:
		return dirt || c.is(below, _netherrack) || c.is(below, _soulSand) || c.is(below, _soulSoil);
	case Soil::Farmland:
		return c.is(below, _farmland);
	case Soil::SoulSand:
		return c.is(below, _soulSand);
	case Soil::DryVegetation:
		return c.inTag(_dryVegetationSoil, below);
	case Soil::CactusFlower:
		return c.is(below, _cactus) || c.is(below, _farmland) || c.isFaceSturdy(below, Direction::Up, GameData::SupportType::Center);
	case Soil::Mushroom:
		return c.properties(below).solidRender;
	case Soil::Seagrass:
		return c.isFaceSturdy(below, Direction::Up) && !c.is(below, _magma);
	case Soil::Waterlily: {
		FluidState fluid = level.getFluidState(belowPos);
		return (fluid.type == level.fluids().water() || c.data.isInstanceOf(c.blocks.blockOf(below), "IceBlock")) &&
			   level.getFluidState(belowPos.above()).isEmpty();
	}
	case Soil::SeaPickle:
		return Shapes::hasFace(c.data.getCollisionShape(below), Direction::Up) || c.isFaceSturdy(below, Direction::Up);
	case Soil::SturdyTop:
		return c.isFaceSturdy(below, Direction::Up);
	}
	return false;
}

bool VegetationBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	BlockPos below = pos.below();
	return mayPlaceOn(level, below, level.getBlockState(below));
}

int VegetationBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	return canSurvive(level, pos, state) ? state : _context->defaultState("minecraft:air");
}

// ----- Crops -----

CropBlock::CropBlock(std::shared_ptr<const BlockContext> context, int block, Kind kind) : VegetationBlock(std::move(context), Soil::Farmland), _kind(kind) {
	// The torchflower crop has ages 0 and 1: at 2 it is a torchflower
	_maxAge		 = kind == Kind::Torchflower ? 2 : _context->blocks.maxInt(block, _context->age);
	_torchflower = _context->defaultState("minecraft:torchflower");
}

bool CropBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	return level.getRawBrightness(pos, 0) >= 8 && VegetationBlock::canSurvive(level, pos, state);
}

void CropBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	const BlockContext& c = *_context;
	// BeetrootBlock and TorchflowerCropBlock grow 3 times slower
	if (_kind != Kind::Crop && level.random().nextInt(3) == 0) return;
	if (level.getRawBrightness(pos, 0) < 9) return;
	int age = c.blocks.getInt(state, c.age);
	if (age >= _maxAge) return;
	float speed = growthSpeed(c, level, c.blocks.blockOf(state), pos, _farmland);
	if (level.random().nextInt(static_cast<int>(25.0F / speed) + 1) != 0) return;
	int grown = _kind == Kind::Torchflower && age + 1 == 2 ? _torchflower : c.blocks.withInt(state, c.age, age + 1);
	level.setBlock(pos, grown, Level::UPDATE_CLIENTS);
}

float CropBlock::growthSpeed(const BlockContext& c, Level& level, int block, const BlockPos& pos, int farmland) {
	float			 speed	  = 1.0F;
	BlockPos		 below	  = pos.below();
	for (int x = -1; x <= 1; x++) {
		for (int z = -1; z <= 1; z++) {
			float soil	= 0.0F;
			int	  state = level.getBlockState(below.offset(x, 0, z));
			if (c.is(state, farmland)) soil = c.blocks.getInt(state, c.moisture) > 0 ? 3.0F : 1.0F;
			if (x != 0 || z != 0) soil /= 4.0F;
			speed += soil;
		}
	}
	auto	 same  = [&](const BlockPos& at) { return c.is(level.getBlockState(at), block); };
	BlockPos north = pos.north(), south = pos.south(), west = pos.west(), east = pos.east();
	bool	 alongX = same(west) || same(east);
	bool	 alongZ = same(north) || same(south);
	if (alongX && alongZ) {
		speed /= 2.0F;
	} else if (same(west.north()) || same(east.north()) || same(east.south()) || same(west.south())) {
		speed /= 2.0F;
	}
	return speed;
}

StemBlock::StemBlock(std::shared_ptr<const BlockContext> context, int fruit, int attachedStem)
	: VegetationBlock(std::move(context), Soil::Farmland), _fruit(fruit), _attachedStem(attachedStem) {
	_dirtTag = _context->tag("minecraft:dirt");
}

void StemBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	const BlockContext& c = *_context;
	if (level.getRawBrightness(pos, 0) < 9) return;
	float speed = CropBlock::growthSpeed(c, level, c.blocks.blockOf(state), pos, _farmland);
	if (level.random().nextInt(static_cast<int>(25.0F / speed) + 1) != 0) return;
	int age = c.blocks.getInt(state, c.age);
	if (age < 7) {
		level.setBlock(pos, c.blocks.withInt(state, c.age, age + 1), Level::UPDATE_CLIENTS);
		return;
	}
	Direction direction = Directions::HORIZONTAL[level.random().nextInt(4)];
	BlockPos  fruitPos	= pos.relative(direction);
	int		  soil		= level.getBlockState(fruitPos.below());
	if (c.isAir(level.getBlockState(fruitPos)) && (c.is(soil, _farmland) || c.inTag(_dirtTag, soil))) {
		level.setBlock(fruitPos, c.blocks.defaultState(_fruit), Level::UPDATE_ALL);
		level.setBlock(pos, c.blocks.with(c.blocks.defaultState(_attachedStem), c.facing, c.directionValue(direction)), Level::UPDATE_ALL);
	}
}

void NetherWartBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	int age = _context->blocks.getInt(state, _context->age);
	if (age < 3 && level.random().nextInt(10) == 0) level.setBlock(pos, _context->blocks.withInt(state, _context->age, age + 1), Level::UPDATE_CLIENTS);
}

void SweetBerryBushBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	int age = _context->blocks.getInt(state, _context->age);
	if (age < 3 && level.random().nextInt(5) == 0 && level.getRawBrightness(pos.above(), 0) >= 9) {
		level.setBlock(pos, _context->blocks.withInt(state, _context->age, age + 1), Level::UPDATE_CLIENTS);
	}
}

bool MushroomBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	BlockPos below		= pos.below();
	int		 belowState = level.getBlockState(below);
	if (_context->inTag(_mushroomGrowBlocks, belowState)) return true;
	return level.getRawBrightness(pos, 0) < 13 && mayPlaceOn(level, below, belowState);
}

void MushroomBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	JavaRandom& random = level.random();
	if (random.nextInt(25) != 0) return;
	// At most 4 other mushrooms of the same kind around
	int left = 5;
	for (int x = -4; x <= 4; x++) {
		for (int y = -1; y <= 1; y++) {
			for (int z = -4; z <= 4; z++) {
				if (_context->is(level.getBlockState(pos.offset(x, y, z)), _context->blocks.blockOf(state)) && --left <= 0) return;
			}
		}
	}
	// Java evaluates the offsets from left to right
	auto randomOffset = [&random](const BlockPos& from) {
		int x = random.nextInt(3) - 1;
		int y = random.nextInt(2);
		y -= random.nextInt(2);
		int z = random.nextInt(3) - 1;
		return from.offset(x, y, z);
	};
	BlockPos origin = pos;
	BlockPos target = randomOffset(origin);
	for (int i = 0; i < 4; i++) {
		if (_context->isAir(level.getBlockState(target)) && canSurvive(level, target, state)) origin = target;
		target = randomOffset(origin);
	}
	if (_context->isAir(level.getBlockState(target)) && canSurvive(level, target, state)) level.setBlock(target, state, Level::UPDATE_CLIENTS);
}

// ----- Saplings -----

SaplingBlock::SaplingBlock(std::shared_ptr<const BlockContext> context, Soil soil, std::shared_ptr<const TreeGrower> grower)
	: VegetationBlock(std::move(context), soil), _grower(std::move(grower)) {
	_stage = _context->blocks.property("stage");
}

void SaplingBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (level.getMaxLocalRawBrightness(pos.above()) >= 9 && level.random().nextInt(7) == 0) advanceTree(level, pos, state);
}

void SaplingBlock::advanceTree(Level& level, const BlockPos& pos, int state) const {
	if (_context->blocks.getInt(state, _stage) == 0) {
		level.setBlock(pos, _context->blocks.cycle(state, _stage), Level::UPDATE_INVISIBLE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
	} else if (_grower) {
		_grower->growTree(level, pos, state);
	}
}

int SeagrassBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const {
	int updated = VegetationBlock::updateShape(level, pos, state, direction, neighborPos, neighborState);
	if (!_context->isAir(updated)) level.scheduleFluidTick(pos, level.fluids().water(), level.fluids().tickDelay(level.fluids().water()));
	return updated;
}

int SeaPickleBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	if (!canSurvive(level, pos, state)) return _context->defaultState("minecraft:air");
	level.scheduleWaterlogged(pos, state);
	return state;
}

bool MangrovePropaguleBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	if (_context->blocks.getBool(state, _context->hanging)) return _context->is(level.getBlockState(pos.above()), _mangroveLeaves);
	return VegetationBlock::canSurvive(level, pos, state);
}

void MangrovePropaguleBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (!_context->blocks.getBool(state, _context->hanging)) {
		if (level.random().nextInt(7) == 0) advanceTree(level, pos, state);
	} else if (_context->blocks.getInt(state, _context->age) != 4) {
		level.setBlock(pos, _context->blocks.cycle(state, _context->age), Level::UPDATE_CLIENTS);
	}
}

int MangrovePropaguleBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos,
										int neighborState) const {
	level.scheduleWaterlogged(pos, state);
	if (direction == Direction::Up && !canSurvive(level, pos, state)) return _context->defaultState("minecraft:air");
	return VegetationBlock::updateShape(level, pos, state, direction, neighborPos, neighborState);
}

AttachedStemBlock::AttachedStemBlock(std::shared_ptr<const BlockContext> context, int fruit, int stem)
	: VegetationBlock(std::move(context), Soil::Farmland), _fruit(fruit), _stem(stem) {}

int AttachedStemBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const {
	const BlockContext& c = *_context;
	if (!c.is(neighborState, _fruit) && direction == c.direction(state, c.facing)) {
		return c.blocks.withInt(c.blocks.defaultState(_stem), c.age, 7);
	}
	return VegetationBlock::updateShape(level, pos, state, direction, neighborPos, neighborState);
}

DoublePlantBlock::DoublePlantBlock(std::shared_ptr<const BlockContext> context, Soil soil, bool seagrass)
	: VegetationBlock(std::move(context), soil), _seagrass(seagrass) {}

bool DoublePlantBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	const BlockContext& c = *_context;
	if (c.blocks.get(state, c.half) == c.upper) {
		int below = level.getBlockState(pos.below());
		return c.blocks.blockOf(below) == c.blocks.blockOf(state) && c.blocks.get(below, c.half) == c.lower;
	}
	if (_seagrass) {
		FluidState fluid = level.getFluidState(pos);
		if (!level.fluids().isWater(fluid.type) || fluid.amount != 8) return false;
	}
	return VegetationBlock::canSurvive(level, pos, state);
}

int DoublePlantBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const {
	const BlockContext& c		   = *_context;
	bool				lower	   = c.blocks.get(state, c.half) == c.lower;
	bool				vertical   = direction == Direction::Up || direction == Direction::Down;
	bool				otherHalf  = c.blocks.blockOf(neighborState) == c.blocks.blockOf(state) && c.blocks.get(neighborState, c.half) != c.blocks.get(state, c.half);
	if (!vertical || lower != (direction == Direction::Up) || otherHalf) {
		if (lower && direction == Direction::Down && !canSurvive(level, pos, state)) return c.defaultState("minecraft:air");
		return VegetationBlock::updateShape(level, pos, state, direction, neighborPos, neighborState);
	}
	return c.defaultState("minecraft:air");
}
