#include "world/blockentity/StorageEntities.hpp"

#include "data/GameData.hpp"
#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "lib/JavaRandom.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/item/Components.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
	constexpr int LEVEL_EVENT_JUKEBOX_START = 1010;
	constexpr int LEVEL_EVENT_JUKEBOX_STOP	= 1011;

	int step(Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; }

	// Container.stillValidBlockEntity: still there, and the player within its interaction range + 4
	bool stillValidBlockEntity(BlockEntity& entity, Player& player) {
		Level* at = entity.level();
		if (!at || entity.isRemoved() || at->getBlockEntity(entity.pos()) != &entity) return false;
		const BlockPos& pos	  = entity.pos();
		double			range = blockInteractionRange(player) + 4.0;
		double			eyeY  = player.getY() + 1.62;
		double			dx	  = std::max({pos.x - player.getX(), 0.0, player.getX() - (pos.x + 1.0)});
		double			dy	  = std::max({pos.y - eyeY, 0.0, eyeY - (pos.y + 1.0)});
		double			dz	  = std::max({pos.z - player.getZ(), 0.0, player.getZ() - (pos.z + 1.0)});
		return dx * dx + dy * dy + dz * dz < range * range;
	}

	// ContainerHelper.removeItem: splits the slot
	ItemStack splitSlot(ItemStack& slot, int count) {
		if (slot.isEmpty() || count <= 0) return {};
		int		  taken = std::min(count, slot.count);
		ItemStack split = slot.copyWithCount(taken);
		slot.shrink(taken);
		if (slot.count <= 0) slot = ItemStack();
		return split;
	}

	// Container.hasAnyMatching
	template <typename Predicate> bool hasAnyMatching(Container& container, Predicate predicate) {
		for (int i = 0; i < container.size(); i++) {
			if (predicate(container.item(i))) return true;
		}
		return false;
	}

	// ----- Network NBT -----
	void u16(std::vector<uint8_t>& out, size_t value) {
		out.push_back(static_cast<uint8_t>(value >> 8));
		out.push_back(static_cast<uint8_t>(value));
	}
	void i32(std::vector<uint8_t>& out, int32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> shift));
	}
	void name(std::vector<uint8_t>& out, uint8_t type, const char* tag) {
		out.push_back(type);
		u16(out, std::strlen(tag));
		out.insert(out.end(), tag, tag + std::strlen(tag));
	}
	void stringPayload(std::vector<uint8_t>& out, const std::string& value) {
		u16(out, value.size());
		out.insert(out.end(), value.begin(), value.end());
	}
	// ItemStack.CODEC: {id, count}
	void stackFields(std::vector<uint8_t>& out, const ItemStack& stack, const GameData& gameData) {
		name(out, 8, "id");
		stringPayload(out, gameData.getStaticName("minecraft:item", stack.item));
		name(out, 3, "count");
		i32(out, stack.count);
	}

	// Saved single stack (empty string for none)
	void writeStacks(BlockEntityWriter& out, const std::vector<ItemStack>& items) {
		out.varint(static_cast<uint32_t>(items.size()));
		for (const ItemStack& stack : items) out.item(stack);
	}
	void readStacks(BlockEntityReader& in, std::vector<ItemStack>& items) {
		std::fill(items.begin(), items.end(), ItemStack());
		uint32_t count = in.varint();
		for (uint32_t i = 0; i < count; i++) {
			ItemStack stack = in.item();
			if (i < items.size()) items[i] = std::move(stack);
		}
	}

	// ItemEntity at a point, with the default pickup delay (10 ticks) and its random throw
	void dropAt(Level& level, double x, double y, double z, ItemStack stack) {
		auto entity = ItemEntity::create(level, {x, y, z}, std::move(stack));
		entity->setPickupDelay(10);
		level.entities().add(std::move(entity));
	}

} // namespace

// ServerLevel.sendParticles: to the players whose block center is within 32 blocks
void sendParticles(Level& level, const char* particle, double x, double y, double z, int count, double dx, double dy, double dz, double speed) {
	int id = level.gameData().getStaticId("minecraft:particle_type", particle);
	if (id < 0) return;
	Buffer data;
	data.writeBool(false); // Override limiter
	data.writeBool(false); // Always show
	data.writeDouble(x);
	data.writeDouble(y);
	data.writeDouble(z);
	data.writeFloat(static_cast<float>(dx));
	data.writeFloat(static_cast<float>(dy));
	data.writeFloat(static_cast<float>(dz));
	data.writeFloat(static_cast<float>(speed));
	data.writeInt(count);
	data.writeVarInt(id); // A particle type without options
	for (const auto& player : level.players()) {
		double px = std::floor(player->getX()) + 0.5 - x, py = std::floor(player->getY()) + 0.5 - y, pz = std::floor(player->getZ()) + 0.5 - z;
		if (px * px + py * py + pz * pz < 32.0 * 32.0) Packet::send(player, PacketId::Play::Clientbound::LEVEL_PARTICLES, data, level.server());
	}
}

void dropContents(Level& level, const BlockPos& pos, Container& container) {
	for (int i = 0; i < container.size(); i++) {
		ItemStack& stack = container.item(i);
		if (!stack.isEmpty()) dropItemStack(level, pos.x, pos.y, pos.z, stack);
	}
}

namespace UpdateTag {
	void items(std::vector<uint8_t>& out, const std::vector<ItemStack>& stacks, const GameData& gameData) {
		int count = 0;
		for (const ItemStack& stack : stacks) count += !stack.isEmpty();
		name(out, 9, "Items");
		out.push_back(10); // Compounds
		i32(out, count);
		for (size_t slot = 0; slot < stacks.size(); slot++) {
			if (stacks[slot].isEmpty()) continue;
			name(out, 1, "Slot");
			out.push_back(static_cast<uint8_t>(slot));
			stackFields(out, stacks[slot], gameData);
			out.push_back(0);
		}
	}
} // namespace UpdateTag

std::unique_ptr<BlockEntity> createStorageBlockEntity(const std::string& type, const BlockPos& pos) {
	if (type == "minecraft:chiseled_bookshelf") return std::make_unique<ChiseledBookShelfBlockEntity>(pos);
	if (type == "minecraft:decorated_pot") return std::make_unique<DecoratedPotBlockEntity>(pos);
	if (type == "minecraft:jukebox") return std::make_unique<JukeboxBlockEntity>(pos);
	if (type == "minecraft:lectern") return std::make_unique<LecternBlockEntity>(pos);
	if (type == "minecraft:crafter") return std::make_unique<CrafterBlockEntity>(pos);
	if (type == "minecraft:shelf") return std::make_unique<ShelfBlockEntity>(pos);
	return nullptr;
}

// ===== Chiseled bookshelf =====

bool ChiseledBookShelfBlockEntity::acceptsItemType(const ItemStack& stack) const {
	return level() && !stack.isEmpty() && level()->gameData().isInTag("minecraft:item", "minecraft:bookshelf_books", stack.item);
}

void ChiseledBookShelfBlockEntity::updateState(int slot) {
	if (slot < 0 || slot >= 6 || !level()) return;
	_lastInteractedSlot		 = slot;
	Level&				 at	 = *level();
	const BlockRegistry& reg = at.blocks();
	int					 state = at.getBlockState(pos());
	for (int i = 0; i < 6; i++) {
		int property = reg.property("slot_" + std::to_string(i) + "_occupied");
		if (reg.has(state, property)) state = reg.withBool(state, property, !_items[i].isEmpty());
	}
	at.setBlock(pos(), state, Level::UPDATE_ALL);
}

void ChiseledBookShelfBlockEntity::setItem(int slot, ItemStack stack) {
	if (acceptsItemType(stack)) {
		_items.at(slot) = std::move(stack);
		updateState(slot);
	} else if (stack.isEmpty()) {
		removeItem(slot, maxStackSize());
	}
}

// removeItem: the whole book, whatever the count asked
ItemStack ChiseledBookShelfBlockEntity::removeItem(int slot, int) {
	ItemStack taken = std::move(_items.at(slot));
	_items[slot]	= ItemStack();
	if (!taken.isEmpty()) updateState(slot);
	return taken;
}

bool ChiseledBookShelfBlockEntity::canPlaceItem(int slot, const ItemStack& stack) const {
	// ListBackedContainer.canPlaceItem
	return acceptsItemType(stack) && (_items.at(slot).isEmpty() || _items[slot].count < 1);
}

bool ChiseledBookShelfBlockEntity::canTakeItem(Container& target, int, const ItemStack& stack) {
	if (!level()) return false;
	const GameData& gameData = level()->gameData();
	return hasAnyMatching(target, [&](const ItemStack& other) {
		return other.isEmpty() || (stack.sameItemSameComponents(other) && other.count + stack.count <= target.maxStackSizeFor(other, gameData));
	});
}

bool ChiseledBookShelfBlockEntity::stillValid(Player& player) { return stillValidBlockEntity(*this, player); }

void ChiseledBookShelfBlockEntity::save(BlockEntityWriter& out) const {
	writeStacks(out, _items);
	out.varint(static_cast<uint32_t>(_lastInteractedSlot + 1));
}

void ChiseledBookShelfBlockEntity::load(BlockEntityReader& in) {
	readStacks(in, _items);
	_lastInteractedSlot = static_cast<int>(in.varint()) - 1;
}

// ===== Decorated pot =====

std::array<int, 4> DecoratedPotBlockEntity::orderedDecorations(const GameData& gameData) const {
	int				   brick = gameData.getStaticId("minecraft:item", "minecraft:brick");
	std::array<int, 4> items;
	for (int i = 0; i < 4; i++) items[i] = decorations[i] > 0 ? decorations[i] : brick;
	return items;
}

// ContainerSingleItem: only slot 0
void DecoratedPotBlockEntity::setItem(int slot, ItemStack stack) {
	if (slot == 0) _item = stack.count > 0 ? std::move(stack) : ItemStack();
	setChanged();
}

ItemStack DecoratedPotBlockEntity::removeItem(int slot, int count) {
	if (slot != 0) return {};
	ItemStack taken = splitSlot(_item, count);
	if (!taken.isEmpty()) setChanged();
	return taken;
}

bool DecoratedPotBlockEntity::stillValid(Player& player) { return stillValidBlockEntity(*this, player); }

void DecoratedPotBlockEntity::wobble(Wobble style) {
	if (!level()) return;
	level()->blockEvent(pos(), level()->blocks().blockOf(level()->getBlockState(pos())), 1, static_cast<int>(style));
}

void DecoratedPotBlockEntity::save(BlockEntityWriter& out) const {
	for (int item : decorations) out.string(item > 0 ? out.gameData.getStaticName("minecraft:item", item) : "");
	out.item(_item);
}

void DecoratedPotBlockEntity::load(BlockEntityReader& in) {
	_gameData = &in.gameData;
	for (int& item : decorations) {
		std::string name = in.string();
		item			 = name.empty() ? 0 : std::max(0, in.gameData.getStaticId("minecraft:item", name));
	}
	_item = in.item();
}

// getUpdateTag (saveCustomOnly): the sherds (when there are any) and the item
void DecoratedPotBlockEntity::writeUpdateTag(std::vector<uint8_t>& out) const {
	const GameData* gameData = level() ? &level()->gameData() : _gameData;
	out.push_back(10);
	if (gameData) {
		if (decorations != std::array<int, 4>{0, 0, 0, 0}) {
			name(out, 9, "sherds");
			out.push_back(8);
			i32(out, 4);
			for (int item : orderedDecorations(*gameData)) stringPayload(out, gameData->getStaticName("minecraft:item", item));
		}
		if (!_item.isEmpty()) {
			name(out, 10, "item");
			stackFields(out, _item, *gameData);
			out.push_back(0);
		}
	}
	out.push_back(0);
}

// ===== Shelf =====

// ListBackedContainer.setItem: limited to the stack's size, then setChanged
void ShelfBlockEntity::setItem(int slot, ItemStack stack) {
	if (stack.count <= 0) stack = ItemStack();
	if (level() && !stack.isEmpty()) stack.count = std::min(stack.count, maxStackSizeFor(stack, level()->gameData()));
	_items.at(slot) = std::move(stack);
	setChanged();
}

ItemStack ShelfBlockEntity::swapItemNoUpdate(int slot, ItemStack stack) {
	ItemStack previous = std::move(_items.at(slot));
	if (stack.count <= 0) stack = ItemStack();
	if (level() && !stack.isEmpty()) stack.count = std::min(stack.count, maxStackSizeFor(stack, level()->gameData()));
	_items[slot] = std::move(stack);
	return previous;
}

void ShelfBlockEntity::setChanged() {
	markChanged();
	if (level()) level()->sendBlockUpdated(pos());
}

bool ShelfBlockEntity::stillValid(Player& player) { return stillValidBlockEntity(*this, player); }

void ShelfBlockEntity::save(BlockEntityWriter& out) const {
	writeStacks(out, _items);
	out.u8(alignItemsToBottom);
}

void ShelfBlockEntity::load(BlockEntityReader& in) {
	_gameData = &in.gameData;
	readStacks(in, _items);
	alignItemsToBottom = in.u8() != 0;
}

void ShelfBlockEntity::writeUpdateTag(std::vector<uint8_t>& out) const {
	const GameData* gameData = level() ? &level()->gameData() : _gameData;
	out.push_back(10);
	if (gameData) UpdateTag::items(out, _items, *gameData);
	name(out, 1, "align_items_to_bottom");
	out.push_back(alignItemsToBottom);
	out.push_back(0);
}
