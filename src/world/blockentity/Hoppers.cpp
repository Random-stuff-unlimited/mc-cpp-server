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

// ===== Dispensers =====

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


// ===== Hoppers =====

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
		// Didn't fit: put back (with setItem when the slot was emptied, like vanilla)
		ItemStack restored = _items[slot].isEmpty() ? left : _items[slot];
		restored.count	   = count;
		if (count == 1) {
			setItem(slot, restored);
		} else {
			_items[slot] = restored;
		}
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
			if (count == 1) {
				source->setItem(slot, restored);
			} else {
				source->item(slot) = restored;
			}
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
