#include "world/blockentity/ContainerEntities.hpp"
#include "world/blockentity/StorageEntities.hpp"

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
	// ----- Container-like blocks (chiseled bookshelves, pots, jukeboxes, lecterns, crafters, shelves) -----
	if (std::unique_ptr<BlockEntity> storage = createStorageBlockEntity(type, pos)) return storage;
	// ----- End of container-like blocks -----
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

double blockInteractionRange(const Player& player) { return player.getGameMode() == GameMode::Creative ? 5.0 : 4.5; }

void OpenersCounter::increment(Level& level, const BlockPos& pos, OpenersListener& listener, double range) {
	int before = _count++;
	if (before == 0) {
		listener.onOpen(level);
		level.scheduleTick(pos, level.blocks().blockOf(level.getBlockState(pos)), 5);
	}
	listener.openerCountChanged(level, before, _count);
	_maxInteractionRange = std::max(range, _maxInteractionRange);
}

void OpenersCounter::decrement(Level& level, const BlockPos&, OpenersListener& listener) {
	int before = _count--;
	if (_count == 0) {
		listener.onClose(level);
		_maxInteractionRange = 0.0;
	}
	listener.openerCountChanged(level, before, _count);
}

// The players (not spectating) whose box touches the block's inflated by the largest range + 4, and that have it open
int OpenersCounter::playersWithContainerOpen(Level& level, const BlockPos& pos, OpenersListener& listener, double* maxRange) const {
	double reach = _maxInteractionRange + 4.0;
	AABB   area{pos.x - reach, pos.y - reach, pos.z - reach, pos.x + 1.0 + reach, pos.y + 1.0 + reach, pos.z + 1.0 + reach};
	int	   count = 0;
	for (const auto& player : level.players()) {
		if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator) continue;
		double half = Player::BB_WIDTH / 2.0;
		AABB   box{player->getX() - half, player->getY(), player->getZ() - half, player->getX() + half, player->getY() + Player::BB_HEIGHT, player->getZ() + half};
		if (!box.intersects(area) || !listener.isOwnContainer(*player)) continue;
		count++;
		if (maxRange) *maxRange = std::max(*maxRange, blockInteractionRange(*player));
	}
	return count;
}

// recheckOpeners: the players around that really have it open (one may have left without closing it)
void OpenersCounter::recheck(Level& level, const BlockPos& pos, OpenersListener& listener) {
	double range = 0.0;
	int	   now	 = playersWithContainerOpen(level, pos, listener, &range);
	_maxInteractionRange = range;
	int before			 = _count;
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
