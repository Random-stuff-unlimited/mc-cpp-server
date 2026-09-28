#include "world/blocks/StorageBlocks.hpp"

#include "lib/JavaRandom.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/blocks/Containers.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/inventory/StorageMenus.hpp"
#include "world/item/Components.hpp"
#include "world/item/Recipes.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace {
	constexpr int LEVEL_EVENT_CRAFTER_CRAFT = 1049;
	constexpr int LEVEL_EVENT_CRAFTER_FAIL	= 1050;
	constexpr int LEVEL_EVENT_CRAFTER_SHOOT = 2010;

	int step(Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; }
	bool creative(const Player& player) { return player.getGameMode() == GameMode::Creative; } // hasInfiniteMaterials
	ItemStack& heldStack(Player& player, int hand) { return player.inventory().getMutable(player.handSlot(hand)); }

	// ItemStack.consumeAndReturn(1, player): one of the stack, which only shrinks without infinite materials
	ItemStack consumeOne(ItemStack& stack, const Player& player) {
		ItemStack one = stack.copyWithCount(1);
		if (!creative(player)) {
			stack.shrink(1);
			if (stack.count <= 0) stack = ItemStack();
		}
		return one;
	}

	int maxStackSize(const ItemStack& stack, const GameData& gameData) {
		const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
		return item ? item->maxStackSize : 64;
	}

	bool isItem(const ItemStack& stack, const GameData& gameData, const char* name) { return stack.item == gameData.getStaticId("minecraft:item", name); }

	// Inventory.add, the rest thrown (Player.drop)
	void giveOrDrop(Level& level, Player& player, ItemStack stack) {
		if (stack.isEmpty()) return;
		if (!player.inventory().add(stack, player.getSelectedSlot(), creative(player), level.gameData()) && !stack.isEmpty()) {
			level.dropFromPlayer(player, std::move(stack), false);
		}
	}

	Direction directionNamed(const std::string& name) {
		constexpr const char* names[6] = {"down", "up", "north", "south", "west", "east"};
		for (int i = 0; i < 6; i++) {
			if (name == names[i]) return static_cast<Direction>(i);
		}
		return Direction::North;
	}
} // namespace
std::optional<int> hitSlot(const BlockHit& hit, Direction facing, int rows, int columns) {
	if (hit.face != facing) return std::nullopt;
	// getRelativeHitCoordinatesForBlockFace: from the neighbor in front
	BlockPos front = hit.pos.relative(hit.face);
	double	 x = hit.x - front.x, y = hit.y - front.y, z = hit.z - front.z;
	float	 u;
	switch (facing) {
	case Direction::North:
		u = static_cast<float>(1.0 - x);
		break;
	case Direction::South:
		u = static_cast<float>(x);
		break;
	case Direction::West:
		u = static_cast<float>(z);
		break;
	case Direction::East:
		u = static_cast<float>(1.0 - z);
		break;
	default:
		return std::nullopt;
	}
	float v = static_cast<float>(y);
	// getSection: which of n parts of 16 pixels
	auto section = [](float value, int count) {
		float pixels = value * 16.0F;
		float size	 = 16.0F / count;
		return std::clamp(static_cast<int>(std::floor(pixels / size)), 0, count - 1);
	};
	int row = section(1.0F - v, rows);
	int col = section(u, columns);
	return col + row * columns;
}

void StorageBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool) const {
	level.updateNeighbourForOutputSignal(pos, blockOf(state));
}

// ===== Chiseled bookshelf =====

int ChiseledBookShelfBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	return blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(Directions::opposite(context.horizontalDirection())));
}

UseResult ChiseledBookShelfBlock::useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const {
	auto* shelf = level.getBlockEntity<ChiseledBookShelfBlockEntity>(pos);
	if (!shelf) return UseResult::Pass;
	ItemStack& held = heldStack(player, hand);
	if (held.isEmpty() || !level.gameData().isInTag("minecraft:item", "minecraft:bookshelf_books", held.item)) return UseResult::TryWithEmptyHand;
	std::optional<int> slot = hitSlot(hit, _context->direction(state, _context->facing), 2, 3);
	if (!slot) return UseResult::Pass;
	if (blocks().getBool(state, blocks().property("slot_" + std::to_string(*slot) + "_occupied"))) return UseResult::TryWithEmptyHand;
	// addBook
	bool enchanted = isItem(held, level.gameData(), "minecraft:enchanted_book");
	shelf->setItem(*slot, consumeOne(held, player));
	level.playSound(nullptr, pos, enchanted ? "minecraft:block.chiseled_bookshelf.insert.enchanted" : "minecraft:block.chiseled_bookshelf.insert",
					Level::SoundSource::Blocks, 1.0F, 1.0F);
	return UseResult::Success;
}

bool ChiseledBookShelfBlock::useWithoutItemAt(Level& level, const BlockPos& pos, int state, Player& player, const BlockHit& hit) const {
	auto* shelf = level.getBlockEntity<ChiseledBookShelfBlockEntity>(pos);
	if (!shelf) return false;
	std::optional<int> slot = hitSlot(hit, _context->direction(state, _context->facing), 2, 3);
	if (!slot) return false;
	// An empty slot: CONSUME (nothing else happens)
	if (!blocks().getBool(state, blocks().property("slot_" + std::to_string(*slot) + "_occupied"))) return true;
	// removeBook
	ItemStack book		= shelf->removeItem(*slot, 1);
	bool	  enchanted = isItem(book, level.gameData(), "minecraft:enchanted_book");
	level.playSound(nullptr, pos, enchanted ? "minecraft:block.chiseled_bookshelf.pickup.enchanted" : "minecraft:block.chiseled_bookshelf.pickup",
					Level::SoundSource::Blocks, 1.0F, 1.0F);
	giveOrDrop(level, player, std::move(book));
	return true;
}

int ChiseledBookShelfBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int, Direction) const {
	auto* shelf = level.getBlockEntity<ChiseledBookShelfBlockEntity>(pos);
	return shelf ? shelf->lastInteractedSlot() + 1 : 0;
}

// ===== Decorated pot =====

// Faces the way the player looks (not toward it)
int DecoratedPotBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	int state = blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(context.horizontalDirection()));
	state	  = blocks().withBool(state, _context->waterlogged, level.getFluidState(context.clickedPos).type == level.fluids().water());
	return blocks().withBool(state, blocks().property("cracked"), false);
}

UseResult DecoratedPotBlock::useItemOn(Level& level, const BlockPos& pos, int, Player& player, int hand, const BlockHit&) const {
	auto* pot = level.getBlockEntity<DecoratedPotBlockEntity>(pos);
	if (!pot) return UseResult::Pass;
	const GameData& gameData = level.gameData();
	ItemStack&		held	 = heldStack(player, hand);
	ItemStack&		there	 = pot->theItem();
	if (held.isEmpty() || !(there.isEmpty() || (there.sameItemSameComponents(held) && there.count < maxStackSize(there, gameData)))) {
		return UseResult::TryWithEmptyHand;
	}
	pot->wobble(DecoratedPotBlockEntity::Wobble::Positive);
	ItemStack one = consumeOne(held, player);
	float	  fullness;
	if (there.isEmpty()) {
		fullness = static_cast<float>(one.count) / maxStackSize(one, gameData);
		there	 = std::move(one);
	} else {
		there.grow(1);
		fullness = static_cast<float>(there.count) / maxStackSize(there, gameData);
	}
	level.playSound(nullptr, pos, "minecraft:block.decorated_pot.insert", Level::SoundSource::Blocks, 1.0F, 0.7F + 0.5F * fullness);
	sendParticles(level, "minecraft:dust_plume", pos.x + 0.5, pos.y + 1.2, pos.z + 0.5, 7, 0.0, 0.0, 0.0, 0.0);
	pot->setChanged();
	return UseResult::Success;
}

bool DecoratedPotBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player&) const {
	auto* pot = level.getBlockEntity<DecoratedPotBlockEntity>(pos);
	if (!pot) return false;
	level.playSound(nullptr, pos, "minecraft:block.decorated_pot.insert_fail", Level::SoundSource::Blocks, 1.0F, 1.0F);
	pot->wobble(DecoratedPotBlockEntity::Wobble::Negative);
	return true;
}

// Broken with a tool of #breaks_decorated_pots, it cracks: its loot is then its sherds (enchantments aren't known
// yet: #prevents_decorated_pot_shattering is never checked)
void DecoratedPotBlock::playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const {
	const ItemStack& tool = player.getStackInHand(0);
	if (tool.isEmpty() || !level.gameData().isInTag("minecraft:item", "minecraft:breaks_decorated_pots", tool.item)) return;
	level.setBlock(pos, blocks().withBool(state, blocks().property("cracked"), true), Level::UPDATE_INVISIBLE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
}

int DecoratedPotBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int, Direction) const {
	auto* pot = level.getBlockEntity<DecoratedPotBlockEntity>(pos);
	return pot ? redstoneSignalFromContainer(*pot, level.gameData()) : 0;
}

// The wobble event (type 1, a WobbleStyle) goes to the clients
bool DecoratedPotBlock::triggerEvent(Level&, const BlockPos&, int, int type, int data) const { return type == 1 && data >= 0 && data < 2; }

// ===== Shelf =====

ShelfBlock::Part ShelfBlock::part(int state) const {
	std::string value = blocks().valueName(blocks().get(state, blocks().property("side_chain")));
	return value == "right" ? Part::Right : value == "center" ? Part::Center : value == "left" ? Part::Left : Part::Unconnected;
}

int ShelfBlock::withPart(int state, Part part) const {
	constexpr const char* names[4] = {"unconnected", "right", "center", "left"};
	return blocks().with(state, blocks().property("side_chain"), blocks().value(names[static_cast<int>(part)]));
}

Direction ShelfBlock::facing(int state) const { return _context->direction(state, _context->facing); }

bool ShelfBlock::isConnectable(int state) const {
	int powered = blocks().property("powered");
	return _context->data.isInTag("minecraft:block", "minecraft:wooden_shelves", blockOf(state)) && blocks().has(state, powered) && blocks().getBool(state, powered);
}

void ShelfBlock::setPart(Level& level, const BlockPos& pos, Part value) const {
	int state = level.getBlockState(pos);
	if (part(state) != value) level.setBlock(pos, withPart(state, value), Level::UPDATE_ALL);
}

namespace {
	// SideChainPart
	using ChainPart = int; // ShelfBlock::Part: unconnected, right, center, left
	constexpr ChainPart UNCONNECTED = 0, RIGHT = 1, CENTER = 2, LEFT = 3;
	ChainPart whenConnectedToTheRight(ChainPart part) { return part == UNCONNECTED || part == LEFT ? LEFT : CENTER; }
	ChainPart whenConnectedToTheLeft(ChainPart part) { return part == UNCONNECTED || part == RIGHT ? RIGHT : CENTER; }
	ChainPart whenDisconnectedFromTheRight(ChainPart part) { return part == UNCONNECTED || part == LEFT ? UNCONNECTED : RIGHT; }
	ChainPart whenDisconnectedFromTheLeft(ChainPart part) { return part == UNCONNECTED || part == RIGHT ? UNCONNECTED : LEFT; }
	bool	  isConnectionTowards(ChainPart part, ChainPart side) { return part == CENTER || part == side; }
	bool	  isChainEnd(ChainPart part) { return part != CENTER; }
} // namespace

// SideChainPartBlock.Neighbors: the shelves on the left (clockwise of the facing) and the right, as first seen
struct ShelfNeighbors {
	struct Neighbor {
		BlockPos pos;
		bool	 connectable = false; // A powered shelf facing the same way (else EmptyNeighbor)
		int		 part		 = 0;
	};
	Level&						 level;
	std::function<int(int)>		 partOf;
	std::function<bool(int)>	 connectable;
	std::function<Direction(int)> facingOf;
	Direction					 facing;
	BlockPos					 center;
	std::map<int64_t, Neighbor>	 cache;

	Neighbor& at(Direction side, int distance) {
		BlockPos pos = center.offset(step(side, 0) * distance, 0, step(side, 2) * distance);
		auto	 it	 = cache.find(pos.asLong());
		if (it != cache.end()) return it->second;
		int		 state = level.getBlockState(pos);
		Neighbor neighbor{pos};
		if (connectable(state) && facingOf(state) == facing) {
			neighbor.connectable = true;
			neighbor.part		 = partOf(state);
		}
		return cache.emplace(pos.asLong(), neighbor).first->second;
	}
	Neighbor& left(int distance = 1) { return at(clockWise(facing), distance); }
	Neighbor& right(int distance = 1) { return at(counterClockWise(facing), distance); }
};

std::vector<BlockPos> ShelfBlock::connectedTo(Level& level, const BlockPos& pos) const {
	int state = level.getBlockState(pos);
	if (!isConnectable(state)) return {};
	ShelfNeighbors neighbors{level, [this](int s) { return static_cast<int>(part(s)); }, [this](int s) { return isConnectable(s); },
							 [this](int s) { return facing(s); }, facing(state), pos, {}};
	std::vector<BlockPos> chain{pos};
	// addBlocksConnectingTowards, up to the chain's length
	for (int i = 1; i < 3; i++) {
		ShelfNeighbors::Neighbor& left = neighbors.left(i);
		if (left.connectable && isConnectionTowards(left.part, LEFT)) chain.insert(chain.begin(), left.pos);
		if (!left.connectable || isChainEnd(left.part)) break;
	}
	for (int i = 1; i < 3; i++) {
		ShelfNeighbors::Neighbor& right = neighbors.right(i);
		if (right.connectable && isConnectionTowards(right.part, RIGHT)) chain.push_back(right.pos);
		if (!right.connectable || isChainEnd(right.part)) break;
	}
	return chain;
}

void ShelfBlock::updateNeighborsAfterPoweringDown(Level& level, const BlockPos& pos, int state) const {
	ShelfNeighbors neighbors{level, [this](int s) { return static_cast<int>(part(s)); }, [this](int s) { return isConnectable(s); },
							 [this](int s) { return facing(s); }, facing(state), pos, {}};
	ShelfNeighbors::Neighbor& left = neighbors.left();
	if (left.connectable) setPart(level, left.pos, static_cast<Part>(whenDisconnectedFromTheRight(left.part)));
	ShelfNeighbors::Neighbor& right = neighbors.right();
	if (right.connectable) setPart(level, right.pos, static_cast<Part>(whenDisconnectedFromTheLeft(right.part)));
}

void ShelfBlock::updateSelfAndNeighborsOnPoweringUp(Level& level, const BlockPos& pos, int state, int oldState) const {
	if (!isConnectable(state)) return;
	// isBeingUpdatedByNeighbor: already connected, or it was
	if (part(state) != Part::Unconnected || (isConnectable(oldState) && part(oldState) != Part::Unconnected)) return;
	ShelfNeighbors neighbors{level, [this](int s) { return static_cast<int>(part(s)); }, [this](int s) { return isConnectable(s); },
							 [this](int s) { return facing(s); }, facing(state), pos, {}};
	ShelfNeighbors::Neighbor& left	= neighbors.left();
	ShelfNeighbors::Neighbor& right = neighbors.right();
	int	 leftSize  = left.connectable ? static_cast<int>(connectedTo(level, left.pos).size()) : 0;
	int	 rightSize = right.connectable ? static_cast<int>(connectedTo(level, right.pos).size()) : 0;
	int	 length	   = 1;
	ChainPart self = UNCONNECTED;
	auto canConnect = [](int size, int with) { return size > 0 && with + size <= 3; };
	if (canConnect(leftSize, length)) {
		self = whenConnectedToTheLeft(self);
		setPart(level, left.pos, static_cast<ShelfBlock::Part>(whenConnectedToTheRight(left.part)));
		length += leftSize;
	}
	if (canConnect(rightSize, length)) {
		self = whenConnectedToTheRight(self);
		setPart(level, right.pos, static_cast<ShelfBlock::Part>(whenConnectedToTheLeft(right.part)));
	}
	setPart(level, pos, static_cast<ShelfBlock::Part>(self));
}

int ShelfBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	int state = blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(Directions::opposite(context.horizontalDirection())));
	state	  = blocks().withBool(state, blocks().property("powered"), level.hasNeighborSignal(context.clickedPos));
	return blocks().withBool(state, _context->waterlogged, level.getFluidState(context.clickedPos).type == level.fluids().water());
}

int ShelfBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	level.scheduleWaterlogged(pos, state);
	return state;
}

void ShelfBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (blocks().getBool(state, blocks().property("powered"))) {
		updateSelfAndNeighborsOnPoweringUp(level, pos, state, oldState);
	} else {
		updateNeighborsAfterPoweringDown(level, pos, state);
	}
}

void ShelfBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	bool powered = level.hasNeighborSignal(pos);
	int	 id		 = blocks().property("powered");
	if (blocks().getBool(state, id) == powered) return;
	int changed = blocks().withBool(state, id, powered);
	if (!powered) changed = withPart(changed, Part::Unconnected);
	level.setBlock(pos, changed, Level::UPDATE_ALL);
	level.playSound(nullptr, pos, powered ? "minecraft:block.shelf.activate" : "minecraft:block.shelf.deactivate", Level::SoundSource::Blocks, 1.0F, 1.0F);
}

void ShelfBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool) const {
	level.updateNeighbourForOutputSignal(pos, blockOf(state));
	updateNeighborsAfterPoweringDown(level, pos, state);
}

// swapHotbar: the chained shelves' slots, left to right, with the end of the hotbar (3 shelves: the whole of it)
bool ShelfBlock::swapHotbar(Level& level, const BlockPos& pos, Player& player) const {
	std::vector<BlockPos> chain = connectedTo(level, pos);
	if (chain.empty()) return false;
	bool			 swapped   = false;
	PlayerInventory& inventory = player.inventory();
	for (size_t i = 0; i < chain.size(); i++) {
		auto* shelf = level.getBlockEntity<ShelfBlockEntity>(chain[i]);
		if (!shelf) continue;
		for (int j = 0; j < shelf->size(); j++) {
			int slot = 9 - static_cast<int>(chain.size() - i) * shelf->size() + j;
			if (slot < 0 || slot > 8) continue;
			int		  window = PlayerInventory::windowSlot(slot);
			ItemStack held	 = inventory.get(window);
			inventory.set(window, ItemStack());
			ItemStack taken = shelf->swapItemNoUpdate(j, held);
			if (!held.isEmpty() || !taken.isEmpty()) {
				inventory.set(window, std::move(taken));
				swapped = true;
			}
		}
		shelf->setChanged();
	}
	return swapped;
}

UseResult ShelfBlock::useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const {
	auto* shelf = level.getBlockEntity<ShelfBlockEntity>(pos);
	if (!shelf || hand != 0) return UseResult::Pass;
	std::optional<int> slot = hitSlot(hit, facing(state), 1, 3);
	if (!slot) return UseResult::Pass;
	if (!blocks().getBool(state, blocks().property("powered"))) {
		// swapSingleItem: the hand's stack for the slot's
		int		  window = player.handSlot(0);
		ItemStack held	 = player.inventory().get(window);
		ItemStack taken	 = shelf->swapItemNoUpdate(*slot, held);
		player.inventory().set(window, creative(player) && taken.isEmpty() ? held : taken);
		shelf->setChanged();
		if (!taken.isEmpty()) {
			level.playSound(nullptr, pos, held.isEmpty() ? "minecraft:block.shelf.take_item" : "minecraft:block.shelf.single_swap", Level::SoundSource::Blocks, 1.0F,
							1.0F);
		} else {
			if (held.isEmpty()) return UseResult::Pass;
			level.playSound(nullptr, pos, "minecraft:block.shelf.place_item", Level::SoundSource::Blocks, 1.0F, 1.0F);
		}
		return UseResult::Success;
	}
	if (!swapHotbar(level, pos, player)) return UseResult::Consume;
	level.playSound(nullptr, pos, "minecraft:block.shelf.multi_swap", Level::SoundSource::Blocks, 1.0F, 1.0F);
	return UseResult::Success;
}

// Which slots hold something (bits 0-2), read from behind the shelf
int ShelfBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const {
	if (direction != Directions::opposite(facing(state))) return 0;
	auto* shelf = level.getBlockEntity<ShelfBlockEntity>(pos);
	if (!shelf) return 0;
	return (shelf->item(0).isEmpty() ? 0 : 1) | (shelf->item(1).isEmpty() ? 0 : 2) | (shelf->item(2).isEmpty() ? 0 : 4);
}

// ===== Components =====

namespace StorageItems {
	namespace {
		void setContainer(ItemStack& stack, const GameData& gameData, const std::vector<ItemStack>& items) {
			bool any = std::any_of(items.begin(), items.end(), [](const ItemStack& s) { return !s.isEmpty(); });
			// ItemContainerContents.fromItems: EMPTY (the item's default) when nothing is inside
			if (any) Components::set(stack, gameData, "minecraft:container", Components::encodeContainer(items));
		}
		std::vector<ItemStack> containerOf(const ItemStack& stack, const GameData& gameData) {
			std::optional<std::vector<uint8_t>> contents = Components::get(stack, gameData, "minecraft:container");
			if (!contents) return {};
			return Components::decodeContainer(*contents, gameData).value_or(std::vector<ItemStack>());
		}
		// ItemContainerContents.copyInto
		void copyInto(const std::vector<ItemStack>& from, std::vector<ItemStack>& to) {
			for (size_t i = 0; i < to.size(); i++) to[i] = i < from.size() ? from[i] : ItemStack();
		}
	} // namespace

	bool collect(const BlockEntity& entity, ItemStack& stack, const GameData& gameData) {
		if (auto* shelf = dynamic_cast<const ChiseledBookShelfBlockEntity*>(&entity)) {
			setContainer(stack, gameData, shelf->items());
			return true;
		}
		if (auto* shelf = dynamic_cast<const ShelfBlockEntity*>(&entity)) {
			setContainer(stack, gameData, shelf->items());
			return true;
		}
		if (auto* pot = dynamic_cast<const DecoratedPotBlockEntity*>(&entity)) {
			if (pot->decorations != std::array<int, 4>{0, 0, 0, 0}) {
				Components::set(stack, gameData, "minecraft:pot_decorations", Components::encodePotDecorations(pot->orderedDecorations(gameData)));
			}
			setContainer(stack, gameData, {pot->theItem()});
			return true;
		}
		return false;
	}

	bool apply(BlockEntity& entity, const ItemStack& stack, const GameData& gameData) {
		if (auto* shelf = dynamic_cast<ChiseledBookShelfBlockEntity*>(&entity)) {
			copyInto(containerOf(stack, gameData), shelf->items());
			return true;
		}
		if (auto* shelf = dynamic_cast<ShelfBlockEntity*>(&entity)) {
			copyInto(containerOf(stack, gameData), shelf->items());
			return true;
		}
		if (auto* pot = dynamic_cast<DecoratedPotBlockEntity*>(&entity)) {
			// PotDecorations: bricks are no sherd
			int brick		 = gameData.getStaticId("minecraft:item", "minecraft:brick");
			pot->decorations = {0, 0, 0, 0};
			if (std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, "minecraft:pot_decorations")) {
				if (std::optional<std::array<int, 4>> items = Components::decodePotDecorations(*value)) {
					for (int i = 0; i < 4; i++) pot->decorations[i] = (*items)[i] == brick ? 0 : (*items)[i];
				}
			}
			std::vector<ItemStack> items = containerOf(stack, gameData);
			pot->theItem()				 = items.empty() ? ItemStack() : items[0]; // copyOne
			return true;
		}
		return false;
	}

	void dynamicDrops(const BlockEntity& entity, const std::string& name, std::vector<ItemStack>& out, const GameData& gameData) {
		if (auto* pot = dynamic_cast<const DecoratedPotBlockEntity*>(&entity); pot && name == "minecraft:sherds") {
			for (int item : pot->orderedDecorations(gameData)) out.emplace_back(item, 1);
		}
	}
} // namespace StorageItems
