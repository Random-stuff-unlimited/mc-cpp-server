#include "world/blocks/Dispensers.hpp"

#include "lib/JavaRandom.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/inventory/Menu.hpp"

#include <algorithm>
#include <cmath>

namespace {
	constexpr int LEVEL_EVENT_DISPENSE		= 1000; // Click
	constexpr int LEVEL_EVENT_DISPENSE_FAIL = 1001; // Empty: a lighter click
	constexpr int LEVEL_EVENT_SMOKE			= 2000; // Smoke in the facing direction

	int step(Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; }
	// RandomSource.triangle: around mode, within deviation
	double triangle(JavaRandom& random, double mode, double deviation) { return mode + deviation * (random.nextDouble() - random.nextDouble()); }
} // namespace

DispenserBlock::DispenserBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool dropper)
	: RedstoneBehavior(std::move(context), std::move(ids)), _dropper(dropper) {
	_triggered		  = blocks().property("triggered");
	auto item		  = [this](const char* name) { return _context->data.getStaticId("minecraft:item", name); };
	_bucket			  = item("minecraft:bucket");
	_waterBucket	  = item("minecraft:water_bucket");
	_lavaBucket		  = item("minecraft:lava_bucket");
	_powderSnowBucket = item("minecraft:powder_snow_bucket");
}

int DispenserBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	return blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(Directions::opposite(context.nearestLookingDirection())));
}

std::unique_ptr<BlockEntity> DispenserBlock::newBlockEntity(const BlockPos& pos, int) const { return std::make_unique<DispenserBlockEntity>(pos, _dropper); }

bool DispenserBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::shared_ptr<BlockEntity> entity	   = level.getSharedBlockEntity(pos);
	auto*						 dispenser = dynamic_cast<DispenserBlockEntity*>(entity.get());
	if (dispenser) {
		Menus::openContainer(player, level, std::shared_ptr<Container>(entity, dispenser), "minecraft:generic_3x3", dispenser->customName,
							 dispenser->defaultName());
	}
	return true;
}

// Powered (or quasi-connected from above): fires 4 ticks later, once per rising edge
void DispenserBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	bool powered   = level.hasNeighborSignal(pos) || level.hasNeighborSignal(pos.above());
	bool triggered = blocks().getBool(state, _triggered);
	if (powered && !triggered) {
		level.scheduleTick(pos, blockOf(state), 4);
		level.setBlock(pos, blocks().withBool(state, _triggered, true), Level::UPDATE_CLIENTS);
	} else if (!powered && triggered) {
		level.setBlock(pos, blocks().withBool(state, _triggered, false), Level::UPDATE_CLIENTS);
	}
}

void DispenserBlock::tick(Level& level, const BlockPos& pos, int state) const { dispenseFrom(level, pos, state); }

void DispenserBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool) const {
	level.updateNeighbourForOutputSignal(pos, blockOf(state)); // Containers.updateNeighboursAfterDestroy
}

int DispenserBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int, Direction) const {
	auto* dispenser = level.getBlockEntity<DispenserBlockEntity>(pos);
	return dispenser ? redstoneSignalFromContainer(*dispenser, level.gameData()) : 0;
}

void DispenserBlock::dispenseFrom(Level& level, const BlockPos& pos, int state) const {
	auto* dispenser = level.getBlockEntity<DispenserBlockEntity>(pos);
	if (!dispenser) return;
	int slot = dispenser->getRandomSlot(level.random());
	if (slot < 0) {
		level.levelEvent(nullptr, LEVEL_EVENT_DISPENSE_FAIL, pos, 0);
		return;
	}
	ItemStack stack = dispenser->item(slot);
	if (!_dropper) {
		dispenser->setItem(slot, dispense(level, pos, state, *dispenser, stack));
		return;
	}
	// DropperBlock: into the container in front (one item), or dropped like a dispenser would
	Direction	facing	= _context->direction(level.getBlockState(pos), _context->facing);
	BlockPos	front	= pos.relative(facing);
	std::shared_ptr<Container> target = Hoppers::containerAt(level, front);
	ItemStack				   result;
	if (!target) {
		result = dispenseDefault(level, pos, state, stack);
	} else {
		// HopperBlockEntity.addItem, one item, through the side facing the dropper
		Direction face = Directions::opposite(facing);
		ItemStack left = Hoppers::addItem(level, dispenser, *target, stack.copyWithCount(1), &face);
		result		   = stack;
		if (left.isEmpty()) result.shrink(1);
	}
	dispenser->setItem(slot, result.count > 0 ? result : ItemStack());
}

ItemStack DispenserBlock::dispense(Level& level, const BlockPos& pos, int state, DispenserBlockEntity& dispenser, ItemStack stack) const {
	Direction facing = _context->direction(state, _context->facing);
	BlockPos  front	 = pos.relative(facing);
	int		  item	 = stack.item;
	if (item == _waterBucket || item == _lavaBucket) {
		// Fluid buckets: pour it in front, the empty bucket stays
		if (emptyBucket(level, front, item == _waterBucket ? level.fluids().water() : level.fluids().lava())) {
			return consumeWithRemainder(level, pos, state, dispenser, stack, ItemStack(_bucket, 1));
		}
		return dispenseDefault(level, pos, state, stack);
	}
	if (item == _bucket) {
		// Empty bucket: takes the fluid source in front
		ItemStack filled = fillBucket(level, front);
		if (filled.isEmpty()) return dispenseDefault(level, pos, state, stack);
		return consumeWithRemainder(level, pos, state, dispenser, stack, filled);
	}
	return dispenseDefault(level, pos, state, stack);
}

ItemStack DispenserBlock::dispenseDefault(Level& level, const BlockPos& pos, int state, ItemStack stack) const {
	Direction facing = _context->direction(state, _context->facing);
	ItemStack one	 = stack.copyWithCount(1);
	stack.shrink(1);
	spawnItem(level, pos, facing, std::move(one));
	playDefault(level, pos, facing);
	return stack.count > 0 ? stack : ItemStack();
}

// DefaultDispenseItemBehavior.spawnItem: from 0.7 blocks out of the center, a little lower, flying out
void DispenserBlock::spawnItem(Level& level, const BlockPos& pos, Direction facing, ItemStack stack) const {
	double x = pos.x + 0.5 + 0.7 * step(facing, 0);
	double y = pos.y + 0.5 + 0.7 * step(facing, 1);
	double z = pos.z + 0.5 + 0.7 * step(facing, 2);
	y -= facing == Direction::Up || facing == Direction::Down ? 0.125 : 0.15625;
	auto		item   = ItemEntity::create(level, {x, y, z}, std::move(stack));
	JavaRandom& random = level.random();
	double		speed  = random.nextDouble() * 0.1 + 0.2;
	double		dx	   = triangle(random, step(facing, 0) * speed, 0.0172275 * 6);
	double		dy	   = triangle(random, 0.2, 0.0172275 * 6);
	double		dz	   = triangle(random, step(facing, 2) * speed, 0.0172275 * 6);
	item->setDeltaMovement({dx, dy, dz});
	level.entities().add(std::move(item));
}

void DispenserBlock::playDefault(Level& level, const BlockPos& pos, Direction facing) const {
	level.levelEvent(nullptr, LEVEL_EVENT_DISPENSE, pos, 0);
	level.levelEvent(nullptr, LEVEL_EVENT_SMOKE, pos, static_cast<int>(facing));
}

ItemStack DispenserBlock::consumeWithRemainder(Level& level, const BlockPos& pos, int state, DispenserBlockEntity& dispenser, ItemStack stack,
											   ItemStack remainder) const {
	stack.shrink(1);
	if (stack.count <= 0) return remainder;
	// The remainder into the dispenser, or out if it is full
	ItemStack left = dispenser.insertItem(std::move(remainder), level.gameData());
	if (!left.isEmpty()) {
		Direction facing = _context->direction(state, _context->facing);
		spawnItem(level, pos, facing, std::move(left));
		playDefault(level, pos, facing);
	}
	return stack;
}

bool DispenserBlock::emptyBucket(Level& level, const BlockPos& at, int fluid) const {
	int								 state		= level.getBlockState(at);
	const GameData::StateProperties& properties = _context->properties(state);
	bool							 replaceable = properties.replaceable || !properties.solid; // canBeReplaced(fluid)
	bool container = _context->data.isInstanceOf(blockOf(state), "LiquidBlockContainer") && level.fluids().canPlaceLiquid(state, fluid);
	bool							 water		 = fluid == level.fluids().water();
	if (!_context->isAir(state) && !replaceable && !container) return false;
	if (level.isUltraWarm() && water) {
		JavaRandom& random = level.random();
		float		pitch  = 2.6F + (random.nextFloat() - random.nextFloat()) * 0.8F;
		level.playSound(nullptr, at, "minecraft:block.fire.extinguish", Level::SoundSource::Blocks, 0.5F, pitch);
		return true;
	}
	const char* sound = water ? "minecraft:item.bucket.empty" : "minecraft:item.bucket.empty_lava";
	if (_context->data.isInstanceOf(blockOf(state), "LiquidBlockContainer") && water) {
		// SimpleWaterloggedBlock.placeLiquid
		if (blocks().has(state, _context->waterlogged) && !blocks().getBool(state, _context->waterlogged)) {
			level.setBlock(at, blocks().withBool(state, _context->waterlogged, true), Level::UPDATE_ALL);
			level.scheduleFluidTick(at, fluid, level.fluids().tickDelay(fluid));
		}
		level.playSound(nullptr, at, sound, Level::SoundSource::Blocks);
		return true;
	}
	if (replaceable && !properties.liquid) level.destroyBlock(at, true);
	int source = level.fluids().legacyBlock({fluid, 8, false});
	if (!level.setBlock(at, source, Level::UPDATE_ALL_IMMEDIATE) && !level.fluids().isSource(level.fluids().stateOf(state))) return false;
	level.playSound(nullptr, at, sound, Level::SoundSource::Blocks);
	return true;
}

ItemStack DispenserBlock::fillBucket(Level& level, const BlockPos& at) const {
	int		   state = level.getBlockState(at);
	FluidState fluid = level.fluids().stateOf(state);
	if (_context->properties(state).liquid && _context->data.isInstanceOf(blockOf(state), "LiquidBlock")) {
		// LiquidBlock.pickupBlock: sources only
		if (fluid.isEmpty() || !level.fluids().isSource(fluid) || fluid.falling) return {};
		level.setBlock(at, _ids->air, Level::UPDATE_ALL_IMMEDIATE);
		return ItemStack(fluid.type == level.fluids().water() ? _waterBucket : _lavaBucket, 1);
	}
	if (blocks().has(state, _context->waterlogged) && blocks().getBool(state, _context->waterlogged) &&
		_context->data.isInstanceOf(blockOf(state), "SimpleWaterloggedBlock")) {
		// SimpleWaterloggedBlock.pickupBlock
		level.setBlock(at, blocks().withBool(state, _context->waterlogged, false), Level::UPDATE_ALL);
		int after = level.getBlockState(at);
		if (!level.behavior(after).canSurvive(level, at, after)) level.destroyBlock(at, true);
		return ItemStack(_waterBucket, 1);
	}
	return {};
}
