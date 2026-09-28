#include "world/blockentity/ContainerEntities.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Shapes.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/inventory/Menu.hpp"

#include <algorithm>
#include <cmath>

namespace {
	int step(Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; }
	// A property of the block at the entity's position
	int propertyValue(Level& level, const BlockPos& pos, const char* property) {
		return level.blocks().get(level.getBlockState(pos), level.blocks().property(property));
	}
	Direction facingAt(Level& level, const BlockPos& pos) {
		int value = propertyValue(level, pos, "facing");
		for (Direction direction : Directions::ALL) {
			if (level.blocks().valueName(value) == std::string(std::array<const char*, 6>{"down", "up", "north", "south", "west", "east"}[static_cast<int>(direction)])) {
				return direction;
			}
		}
		return Direction::North;
	}
	float openPitch(Level& level) { return level.random().nextFloat() * 0.1F + 0.9F; }
} // namespace

std::unique_ptr<BlockEntity> createContainerBlockEntity(const std::string& type, const BlockPos& pos) {
	if (type == "minecraft:chest" || type == "minecraft:trapped_chest") return std::make_unique<ChestBlockEntity>(type, pos);
	if (type == "minecraft:barrel") return std::make_unique<BarrelBlockEntity>(pos);
	if (type == "minecraft:shulker_box") return std::make_unique<ShulkerBoxBlockEntity>(pos);
	if (type == "minecraft:ender_chest") return std::make_unique<EnderChestBlockEntity>(pos);
	if (type == "minecraft:hopper") return std::make_unique<HopperBlockEntity>(pos);
	if (type == "minecraft:dispenser") return std::make_unique<DispenserBlockEntity>(pos, false);
	if (type == "minecraft:dropper") return std::make_unique<DispenserBlockEntity>(pos, true);
	return createProcessingBlockEntity(type, pos);
}

// ----- ContainerBlockEntity -----

void ContainerBlockEntity::setItem(int slot, ItemStack stack) {
	if (stack.count <= 0) stack = ItemStack();
	// ItemStack.limitSize
	if (level() && !stack.isEmpty()) stack.count = std::min(stack.count, maxStackSizeFor(stack, level()->gameData()));
	_items.at(slot) = std::move(stack);
	setChanged();
}

bool ContainerBlockEntity::stillValid(Player& player) {
	Level* at = level();
	if (!at || isRemoved() || at->getBlockEntity(pos()) != this) return false;
	// Player.canInteractWithBlock(pos, 4.0): the block's box within the interaction range + 4
	double range = (player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 4.0;
	double eyeY	 = player.getY() + 1.62;
	double dx	 = std::max({pos().x - player.getX(), 0.0, player.getX() - (pos().x + 1.0)});
	double dy	 = std::max({pos().y - eyeY, 0.0, eyeY - (pos().y + 1.0)});
	double dz	 = std::max({pos().z - player.getZ(), 0.0, player.getZ() - (pos().z + 1.0)});
	return dx * dx + dy * dy + dz * dz < range * range;
}

void dropItemStack(Level& level, double x, double y, double z, ItemStack stack) {
	JavaRandom& random = level.random();
	double		width  = 0.25; // EntityType.ITEM
	double		px	   = std::floor(x) + random.nextDouble() * (1.0 - width) + width / 2.0;
	double		py	   = std::floor(y) + random.nextDouble() * (1.0 - width);
	double		pz	   = std::floor(z) + random.nextDouble() * (1.0 - width) + width / 2.0;
	auto		triangle = [&random](double mode, double deviation) { return mode + deviation * (random.nextDouble() - random.nextDouble()); };
	while (!stack.isEmpty()) {
		int		  count = std::min(stack.count, random.nextInt(21) + 10);
		ItemStack piece = stack.copyWithCount(count);
		stack.shrink(count);
		auto   item = ItemEntity::create(level, {px, py, pz}, std::move(piece));
		double dx	= triangle(0.0, 0.11485000171139836);
		double dy	= triangle(0.2, 0.11485000171139836);
		double dz	= triangle(0.0, 0.11485000171139836);
		item->setDeltaMovement({dx, dy, dz});
		level.entities().add(std::move(item));
	}
}

void ContainerBlockEntity::preRemoveSideEffects(Level& level) {
	for (ItemStack& stack : _items) {
		if (!stack.isEmpty()) dropItemStack(level, pos().x, pos().y, pos().z, std::move(stack));
		stack = ItemStack();
	}
}

void ContainerBlockEntity::save(BlockEntityWriter& out) const {
	out.varint(static_cast<uint32_t>(_items.size()));
	for (const ItemStack& stack : _items) out.item(stack);
	out.bytes(customName);
	saveExtra(out);
}

void ContainerBlockEntity::load(BlockEntityReader& in) {
	uint32_t count = in.varint();
	for (uint32_t i = 0; i < count; i++) {
		ItemStack stack = in.item();
		if (i < _items.size()) _items[i] = std::move(stack);
	}
	customName = in.bytes();
	loadExtra(in);
}

// ----- Openers -----

void OpenersCounter::increment(Level& level, const BlockPos& pos, OpenersListener& listener) {
	int before = _count++;
	if (before == 0) {
		listener.onOpen(level);
		level.scheduleTick(pos, level.blocks().blockOf(level.getBlockState(pos)), 5);
	}
	listener.openerCountChanged(level, before, _count);
}

void OpenersCounter::decrement(Level& level, const BlockPos&, OpenersListener& listener) {
	int before = _count--;
	if (_count == 0) listener.onClose(level);
	listener.openerCountChanged(level, before, _count);
}

// recheckOpeners: the players around that really have it open (one may have left without closing it)
void OpenersCounter::recheck(Level& level, const BlockPos& pos, OpenersListener& listener) {
	int now = 0;
	for (const auto& player : level.server().getGamePlayers()) {
		if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator) continue;
		double dx = player->getX() - (pos.x + 0.5), dy = player->getY() - (pos.y + 0.5), dz = player->getZ() - (pos.z + 0.5);
		if (std::abs(dx) > 9.5 || std::abs(dy) > 9.5 || std::abs(dz) > 9.5) continue;
		if (listener.isOwnContainer(*player)) now++;
	}
	int before = _count;
	if (before != now) {
		if (now != 0 && before == 0) {
			listener.onOpen(level);
		} else if (now == 0) {
			listener.onClose(level);
		}
		_count = now;
	}
	listener.openerCountChanged(level, before, now);
	if (now > 0) level.scheduleTick(pos, level.blocks().blockOf(level.getBlockState(pos)), 5);
}

namespace {
	// The player's open menu shows this container (or a double chest with it)
	bool menuShows(Player& player, const Container* container) {
		if (!player.openMenuSlot()) return false;
		for (const Slot& slot : player.openMenuSlot()->slots()) {
			if (slot.container->contains(container)) return true;
		}
		return false;
	}
} // namespace

// ----- CompoundContainer -----

CompoundContainer::CompoundContainer(std::shared_ptr<BlockEntity> first, std::shared_ptr<BlockEntity> second)
	: _keepFirst(std::move(first)), _keepSecond(std::move(second)), _first(dynamic_cast<Container*>(_keepFirst.get())),
	  _second(dynamic_cast<Container*>(_keepSecond.get())) {}

void CompoundContainer::setItem(int slot, ItemStack stack) {
	if (slot < _first->size()) {
		_first->setItem(slot, std::move(stack));
	} else {
		_second->setItem(slot - _first->size(), std::move(stack));
	}
}

void CompoundContainer::setChanged() {
	_first->setChanged();
	_second->setChanged();
}

void CompoundContainer::startOpen(Player& player) {
	_first->startOpen(player);
	_second->startOpen(player);
}

void CompoundContainer::stopOpen(Player& player) {
	_first->stopOpen(player);
	_second->stopOpen(player);
}

// ----- Chests -----

bool isChestBlocked(Level& level, const BlockPos& pos) { return level.isRedstoneConductor(level.getBlockState(pos.above())); }

std::shared_ptr<BlockEntity> chestPartner(Level& level, const BlockPos& pos, bool ignoreBlocked, bool& first) {
	const BlockRegistry& blocks = level.blocks();
	int					 state	= level.getBlockState(pos);
	int					 typeId = blocks.property("type");
	std::string			 type	= blocks.valueName(blocks.get(state, typeId));
	if (type == "single" || (!ignoreBlocked && isChestBlocked(level, pos))) return nullptr;
	// getConnectedDirection: clockwise of the facing for the left half, counterclockwise for the right one
	Direction facing = facingAt(level, pos);
	auto	  turn	 = [](Direction d, bool clockwise) {
		   constexpr Direction order[4] = {Direction::North, Direction::East, Direction::South, Direction::West};
		   int				   i		= 0;
		   while (order[i] != d) i++;
		   return order[(i + (clockwise ? 1 : 3)) % 4];
	};
	BlockPos otherPos = pos.relative(turn(facing, type == "left"));
	int		 other	  = level.getBlockState(otherPos);
	if (blocks.blockOf(other) != blocks.blockOf(state)) return nullptr;
	std::string otherType = blocks.valueName(blocks.get(other, typeId));
	if (otherType == "single" || otherType == type || facingAt(level, otherPos) != facing) return nullptr;
	if (!ignoreBlocked && isChestBlocked(level, otherPos)) return nullptr;
	first = type == "right";
	return level.getSharedBlockEntity(otherPos);
}

void ChestBlockEntity::startOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.increment(*level(), pos(), *this);
}

void ChestBlockEntity::stopOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.decrement(*level(), pos(), *this);
}

// ChestBlockEntity.playSound: from the middle of a double chest (the left half stays quiet)
void ChestBlockEntity::playSound(Level& level, bool open) {
	int			state = level.getBlockState(pos());
	std::string type  = level.blocks().valueName(level.blocks().get(state, level.blocks().property("type")));
	if (type == "left") return;
	std::string name  = level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(state));
	std::string sound = "minecraft:block.chest";
	if (name.find("copper_chest") != std::string::npos) {
		sound = name.find("oxidized") != std::string::npos	  ? "minecraft:block.copper_chest_oxidized"
				: name.find("weathered") != std::string::npos ? "minecraft:block.copper_chest_weathered"
															  : "minecraft:block.copper_chest";
	}
	double x = pos().x + 0.5, y = pos().y + 0.5, z = pos().z + 0.5;
	if (type == "right") {
		// getConnectedDirection: counterclockwise of the facing for the right half
		Direction facing	= facingAt(level, pos());
		Direction connected = facing == Direction::North ? Direction::West
							  : facing == Direction::West ? Direction::South
							  : facing == Direction::South ? Direction::East
														   : Direction::North;
		x += step(connected, 0) * 0.5;
		z += step(connected, 2) * 0.5;
	}
	level.playSoundAt(nullptr, x, y, z, sound + (open ? ".open" : ".close"), Level::SoundSource::Blocks, 0.5F, openPitch(level));
}

void ChestBlockEntity::onOpen(Level& level) { playSound(level, true); }
void ChestBlockEntity::onClose(Level& level) { playSound(level, false); }

// signalOpenCount: the lid (a block event for the clients); a trapped chest also powers what is around and below
void ChestBlockEntity::openerCountChanged(Level& level, int before, int now) {
	int block = level.blocks().blockOf(level.getBlockState(pos()));
	level.blockEvent(pos(), block, 1, now);
	if (type() == "minecraft:trapped_chest" && before != now) {
		level.updateNeighborsAt(pos(), block);
		level.updateNeighborsAt(pos().below(), block);
	}
}

bool ChestBlockEntity::isOwnContainer(Player& player) { return menuShows(player, this); }

// ----- Barrel -----

void BarrelBlockEntity::startOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.increment(*level(), pos(), *this);
}

void BarrelBlockEntity::stopOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.decrement(*level(), pos(), *this);
}

void BarrelBlockEntity::setOpen(Level& level, bool open) {
	// The sound, from the side it faces
	Direction facing = facingAt(level, pos());
	double	  x		 = pos().x + 0.5 + step(facing, 0) / 2.0;
	double	  y		 = pos().y + 0.5 + step(facing, 1) / 2.0;
	double	  z		 = pos().z + 0.5 + step(facing, 2) / 2.0;
	level.playSoundAt(nullptr, x, y, z, open ? "minecraft:block.barrel.open" : "minecraft:block.barrel.close", Level::SoundSource::Blocks, 0.5F,
					  openPitch(level));
	int state = level.getBlockState(pos());
	level.setBlock(pos(), level.blocks().withBool(state, level.blocks().property("open"), open), Level::UPDATE_ALL);
}

void BarrelBlockEntity::onOpen(Level& level) { setOpen(level, true); }
void BarrelBlockEntity::onClose(Level& level) { setOpen(level, false); }
bool BarrelBlockEntity::isOwnContainer(Player& player) { return menuShows(player, this); }

// ----- Shulker box -----

bool ShulkerBoxBlockEntity::isShulkerBoxItem(const ItemStack& stack) const {
	// Item.canFitInsideContainerItems: not a shulker box (the block the item places)
	if (!level()) return false;
	const GameData& data  = level()->gameData();
	int				state = data.getPlacedBlockState(stack.item);
	return state >= 0 && data.isInstanceOf(data.getBlocks().blockOf(state), "ShulkerBoxBlock");
}

bool ShulkerBoxBlockEntity::canPlaceItemThroughFace(int, const ItemStack& stack, const Direction*) { return !isShulkerBoxItem(stack); }

void ShulkerBoxBlockEntity::startOpen(Player& player) {
	if (isRemoved() || player.getGameMode() == GameMode::Spectator || !level()) return;
	Level& at = *level();
	if (_openCount < 0) _openCount = 0;
	_openCount++;
	at.blockEvent(pos(), at.blocks().blockOf(at.getBlockState(pos())), 1, _openCount);
	if (_openCount == 1) {
		_animation = Animation::Opening;
		at.playSound(nullptr, pos(), "minecraft:block.shulker_box.open", Level::SoundSource::Blocks, 0.5F, openPitch(at));
	}
}

void ShulkerBoxBlockEntity::stopOpen(Player& player) {
	if (isRemoved() || player.getGameMode() == GameMode::Spectator || !level()) return;
	Level& at = *level();
	_openCount--;
	at.blockEvent(pos(), at.blocks().blockOf(at.getBlockState(pos())), 1, _openCount);
	if (_openCount <= 0) {
		_animation = Animation::Closing;
		at.playSound(nullptr, pos(), "minecraft:block.shulker_box.close", Level::SoundSource::Blocks, 0.5F, openPitch(at));
	}
}

void ShulkerBoxBlockEntity::tick(Level& level) {
	auto neighborUpdates = [&] {
		int state = level.getBlockState(pos());
		level.updateNeighbourShapes(pos(), state, Level::UPDATE_ALL);
		level.updateNeighborsAt(pos(), level.blocks().blockOf(state));
	};
	_progressOld = _progress;
	switch (_animation) {
	case Animation::Closed:
		_progress = 0.0F;
		break;
	case Animation::Opening: {
		_progress += 0.1F;
		if (_progressOld == 0.0F) neighborUpdates();
		if (_progress >= 1.0F) {
			_animation = Animation::Opened;
			_progress  = 1.0F;
			neighborUpdates();
		}
		// moveCollidedEntities: what is in the way of the lid is pushed along
		Direction facing = facingAt(level, pos());
		double	  from = 0.5 * _progressOld, to = 0.5 * _progress;
		double	  low = std::min(from, to), high = std::max(from, to);
		// Shulker.getProgressDeltaAabb: the slice the lid sweeps this tick, above the box in its facing
		AABB delta{pos().x + 0.0, pos().y + 0.0, pos().z + 0.0, pos().x + 1.0, pos().y + 1.0, pos().z + 1.0};
		for (int axis = 0; axis < 3; axis++) {
			int s = step(facing, axis);
			if (s == 0) continue;
			double base = axis == 0 ? pos().x : axis == 1 ? pos().y : pos().z;
			double lo	= s > 0 ? base + 1.0 + low : base - high;
			double hi	= s > 0 ? base + 1.0 + high : base - low;
			if (axis == 0) delta.minX = lo, delta.maxX = hi;
			if (axis == 1) delta.minY = lo, delta.maxY = hi;
			if (axis == 2) delta.minZ = lo, delta.maxZ = hi;
		}
		for (Entity* entity : level.entities().entitiesIn(delta)) {
			double sx = delta.maxX - delta.minX + 0.01, sy = delta.maxY - delta.minY + 0.01, sz = delta.maxZ - delta.minZ + 0.01;
			entity->moveByShulker({sx * step(facing, 0), sy * step(facing, 1), sz * step(facing, 2)});
		}
		break;
	}
	case Animation::Opened:
		_progress = 1.0F;
		break;
	case Animation::Closing:
		_progress -= 0.1F;
		if (_progressOld == 1.0F) neighborUpdates();
		if (_progress <= 0.0F) {
			_animation = Animation::Closed;
			_progress  = 0.0F;
			neighborUpdates();
		}
		break;
	}
}

// ----- Ender chest -----

void EnderChestBlockEntity::startOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.increment(*level(), pos(), *this);
}

void EnderChestBlockEntity::stopOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.decrement(*level(), pos(), *this);
}

bool EnderChestBlockEntity::stillValid(Player& player) {
	Level* at = level();
	if (!at || isRemoved() || at->getBlockEntity(pos()) != this) return false;
	double range = (player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 4.0;
	double dx = pos().x + 0.5 - player.getX(), dy = pos().y + 0.5 - (player.getY() + 1.62), dz = pos().z + 0.5 - player.getZ();
	return dx * dx + dy * dy + dz * dz < (range + 0.87) * (range + 0.87);
}

void EnderChestBlockEntity::onOpen(Level& level) {
	level.playSound(nullptr, pos(), "minecraft:block.ender_chest.open", Level::SoundSource::Blocks, 0.5F, openPitch(level));
}

void EnderChestBlockEntity::onClose(Level& level) {
	level.playSound(nullptr, pos(), "minecraft:block.ender_chest.close", Level::SoundSource::Blocks, 0.5F, openPitch(level));
}

void EnderChestBlockEntity::openerCountChanged(Level& level, int, int now) {
	level.blockEvent(pos(), level.blocks().blockOf(level.getBlockState(pos())), 1, now);
}

bool EnderChestBlockEntity::isOwnContainer(Player& player) {
	if (!player.openMenuSlot()) return false;
	for (const Slot& slot : player.openMenuSlot()->slots()) {
		if (auto* ender = dynamic_cast<EnderChestContainer*>(slot.container); ender && ender->chest() == this) return true;
	}
	return false;
}

EnderChestContainer::EnderChestContainer(Player& player, std::shared_ptr<BlockEntity> chest)
	: _player(player), _keepAlive(std::move(chest)), _chest(dynamic_cast<EnderChestBlockEntity*>(_keepAlive.get())) {}

ItemStack& EnderChestContainer::item(int slot) { return _player.enderChest().at(slot); }

// ----- Dispensers -----

int DispenserBlockEntity::getRandomSlot(JavaRandom& random) {
	int chosen = -1, seen = 1;
	for (int i = 0; i < 9; i++) {
		if (!_items[i].isEmpty() && random.nextInt(seen++) == 0) chosen = i;
	}
	return chosen;
}

ItemStack DispenserBlockEntity::insertItem(ItemStack stack, const GameData& gameData) {
	int max = maxStackSizeFor(stack, gameData);
	for (int i = 0; i < 9 && !stack.isEmpty(); i++) {
		ItemStack& slot = _items[i];
		if (!slot.isEmpty() && !slot.sameItemSameComponents(stack)) continue;
		int moved = std::min(stack.count, max - (slot.isEmpty() ? 0 : slot.count));
		if (moved <= 0) continue;
		if (slot.isEmpty()) {
			setItem(i, stack.copyWithCount(moved));
		} else {
			slot.grow(moved);
		}
		stack.shrink(moved);
	}
	if (stack.count <= 0) stack = ItemStack();
	return stack;
}

// ----- Hoppers -----

namespace Hoppers {
	std::shared_ptr<Container> containerAt(Level& level, const BlockPos& pos) {
		std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
		auto*						 container = dynamic_cast<Container*>(entity.get());
		if (!container) return nullptr;
		// A chest: the double chest, blocked or not (ChestBlock.getContainer with ignoreBlocked)
		if (dynamic_cast<ChestBlockEntity*>(entity.get())) {
			bool						 first = false;
			std::shared_ptr<BlockEntity> other = chestPartner(level, pos, true, first);
			if (other) return std::make_shared<CompoundContainer>(first ? entity : other, first ? other : entity);
		}
		return std::shared_ptr<Container>(entity, container);
	}

	// canPlaceItemInContainer and tryMoveInItem
	ItemStack tryMoveInItem(Level& level, Container* from, Container& to, ItemStack stack, int slot, const Direction* face) {
		if (!to.canPlaceItem(slot, stack) || (to.isWorldly() && !to.canPlaceItemThroughFace(slot, stack, face))) return stack;
		ItemStack& there   = to.item(slot);
		bool	   moved   = false;
		bool	   wasEmpty = to.isEmpty();
		if (there.isEmpty()) {
			to.setItem(slot, stack);
			stack = ItemStack();
			moved = true;
		} else {
			const GameData::ItemProperties* item = level.gameData().getItemProperties(stack.item);
			int								max	 = item ? item->maxStackSize : 64;
			if (there.count <= max && there.sameItemSameComponents(stack)) {
				int count = std::min(stack.count, max - there.count);
				stack.shrink(count);
				there.grow(count);
				moved = count > 0;
			}
		}
		if (moved) {
			// A hopper that was empty waits a full cooldown (one tick less if the source hopper ticked first)
			if (auto* hopper = dynamic_cast<HopperBlockEntity*>(&to); hopper && wasEmpty && hopper->cooldown <= 8) {
				int less = 0;
				if (auto* source = dynamic_cast<HopperBlockEntity*>(from); source && hopper->tickedGameTime >= source->tickedGameTime) less = 1;
				hopper->cooldown = 8 - less;
			}
			to.setChanged();
		}
		if (stack.count <= 0) stack = ItemStack();
		return stack;
	}

	ItemStack addItem(Level& level, Container* from, Container& to, ItemStack stack, const Direction* face) {
		if (to.isWorldly() && face) {
			for (int slot : to.slotsForFace(*face)) {
				if (stack.isEmpty()) break;
				stack = tryMoveInItem(level, from, to, std::move(stack), slot, face);
			}
		} else {
			for (int slot = 0; slot < to.size() && !stack.isEmpty(); slot++) stack = tryMoveInItem(level, from, to, std::move(stack), slot, face);
		}
		return stack;
	}

	std::vector<int> slotsOf(Container& container, Direction face) {
		if (container.isWorldly()) return container.slotsForFace(face);
		std::vector<int> slots(static_cast<size_t>(container.size()));
		for (int i = 0; i < container.size(); i++) slots[i] = i;
		return slots;
	}

	bool isFullContainer(Level& level, Container& container, Direction face) {
		for (int slot : slotsOf(container, face)) {
			const ItemStack& stack = container.item(slot);
			const auto*		 item  = level.gameData().getItemProperties(stack.item);
			if (stack.isEmpty() || stack.count < (item ? item->maxStackSize : 64)) return false;
		}
		return true;
	}
} // namespace Hoppers

void HopperBlockEntity::tick(Level& level) {
	cooldown--;
	tickedGameTime = level.getGameTime();
	if (cooldown > 0) return;
	cooldown = 0;
	tryMoveItems(level, nullptr);
}

void HopperBlockEntity::entityInside(Level& level, ItemEntity& item) {
	if (item.item().isEmpty()) return;
	// The item's box, relative to the hopper, touches SUCK_AABB (the top of the hopper and the block above)
	AABB box = item.boundingBox().move(-pos().x, -pos().y, -pos().z);
	AABB suck{0.0, 11.0 / 16.0, 0.0, 1.0, 2.0, 1.0};
	if (box.intersects(suck)) tryMoveItems(level, &item);
}

// tryMoveItems: push first, then pull (from above, or the given item entity)
bool HopperBlockEntity::tryMoveItems(Level& level, ItemEntity* only) {
	int state = level.getBlockState(pos());
	if (cooldown > 0 || !level.blocks().getBool(state, level.blocks().property("enabled"))) return false;
	bool moved = false;
	if (!isEmpty()) moved = ejectItems(level);
	if (!inventoryFull(level)) moved |= only ? suckItem(level, *only) : suckInItems(level);
	if (moved) {
		cooldown = 8;
		markChanged();
		return true;
	}
	return false;
}

bool HopperBlockEntity::inventoryFull(Level& level) {
	for (const ItemStack& stack : _items) {
		const auto* item = level.gameData().getItemProperties(stack.item);
		if (stack.isEmpty() || stack.count != (item ? item->maxStackSize : 64)) return false;
	}
	return true;
}

bool HopperBlockEntity::ejectItems(Level& level) {
	Direction				   facing = facingAt(level, pos());
	std::shared_ptr<Container> target = Hoppers::containerAt(level, pos().relative(facing));
	if (!target) return false;
	Direction face = Directions::opposite(facing);
	if (Hoppers::isFullContainer(level, *target, face)) return false;
	for (int slot = 0; slot < size(); slot++) {
		ItemStack& stack = _items[slot];
		if (stack.isEmpty()) continue;
		int		  count = stack.count;
		ItemStack left	= Hoppers::addItem(level, this, *target, removeItem(slot, 1), &face);
		if (left.isEmpty()) {
			target->setChanged();
			return true;
		}
		// Didn't fit: put back
		ItemStack restored = _items[slot].isEmpty() ? left : _items[slot];
		restored.count	   = count;
		_items[slot]	   = restored;
	}
	return false;
}

bool HopperBlockEntity::suckInItems(Level& level) {
	BlockPos				   above  = pos().above();
	std::shared_ptr<Container> source = Hoppers::containerAt(level, above);
	if (source) {
		Direction down = Direction::Down;
		for (int slot : Hoppers::slotsOf(*source, down)) {
			ItemStack& stack = source->item(slot);
			if (stack.isEmpty() || !source->canTakeItem(*this, slot, stack) || (source->isWorldly() && !source->canTakeItemThroughFace(slot, stack, down))) {
				continue;
			}
			int		  count = stack.count;
			ItemStack left	= Hoppers::addItem(level, source.get(), *this, source->removeItem(slot, 1), nullptr);
			if (left.isEmpty()) {
				source->setChanged();
				return true;
			}
			ItemStack restored = source->item(slot).isEmpty() ? left : source->item(slot);
			restored.count	   = count;
			source->item(slot) = restored;
		}
		return false;
	}
	// No item entity through a full block above (unless it lets them through, like a composter)
	int	 aboveState = level.getBlockState(above);
	bool blocked	= Shapes::isFullBlock(level.gameData().getCollisionShape(aboveState)) &&
				   !level.gameData().isInTag("minecraft:block", "minecraft:does_not_block_hoppers", level.blocks().blockOf(aboveState));
	if (blocked) return false;
	AABB area{pos().x + 0.0, pos().y + 11.0 / 16.0, pos().z + 0.0, pos().x + 1.0, pos().y + 2.0, pos().z + 1.0};
	for (ItemEntity* item : level.entities().itemsIn(area)) {
		if (suckItem(level, *item)) return true;
	}
	return false;
}

// addItem(container, itemEntity)
bool HopperBlockEntity::suckItem(Level& level, ItemEntity& item) {
	ItemStack left = Hoppers::addItem(level, nullptr, *this, item.item(), nullptr);
	if (left.isEmpty()) {
		item.setItem(ItemStack());
		item.discard();
		return true;
	}
	item.setItem(left);
	return false;
}
