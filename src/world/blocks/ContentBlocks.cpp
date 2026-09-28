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

// ===== Jukebox =====
// ===== Jukebox =====

UseResult JukeboxBlock::useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit&) const {
	if (blocks().getBool(state, blocks().property("has_record"))) return UseResult::TryWithEmptyHand;
	// JukeboxPlayable.tryInsertIntoJukebox
	ItemStack& held = heldStack(player, hand);
	if (!JukeboxSong::fromStack(held, level.gameData())) return UseResult::TryWithEmptyHand;
	ItemStack disc = consumeOne(held, player);
	if (auto* jukebox = level.getBlockEntity<JukeboxBlockEntity>(pos)) jukebox->setTheItem(std::move(disc));
	return UseResult::Success;
}

bool JukeboxBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player&) const {
	auto* jukebox = level.getBlockEntity<JukeboxBlockEntity>(pos);
	if (!blocks().getBool(state, blocks().property("has_record")) || !jukebox) return false;
	jukebox->popOutTheItem();
	return true;
}

int JukeboxBlock::getSignal(Level& level, const BlockPos& pos, int, Direction) const {
	auto* jukebox = level.getBlockEntity<JukeboxBlockEntity>(pos);
	return jukebox && jukebox->isPlaying() ? 15 : 0;
}

int JukeboxBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int, Direction) const {
	auto* jukebox = level.getBlockEntity<JukeboxBlockEntity>(pos);
	return jukebox ? jukebox->comparatorOutput() : 0;
}

// ===== Lectern =====

int LecternBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	int state = blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(Directions::opposite(context.horizontalDirection())));
	return blocks().withBool(state, blocks().property("has_book"), false);
}

bool LecternBlock::tryPlaceBook(Level& level, const BlockPos& pos, int state, Player* player, ItemStack& stack) {
	const BlockRegistry& reg = level.blocks();
	if (reg.getBool(state, reg.property("has_book"))) return false;
	auto* lectern = level.getBlockEntity<LecternBlockEntity>(pos);
	if (!lectern) return true;
	ItemStack one = player ? consumeOne(stack, *player) : stack.copyWithCount(1);
	if (!player) stack.shrink(1);
	lectern->setBook(std::move(one));
	// resetBookState(true)
	int placed = reg.withBool(reg.withBool(state, reg.property("powered"), false), reg.property("has_book"), true);
	level.setBlock(pos, placed, Level::UPDATE_ALL);
	level.updateNeighborsAt(pos.below(), reg.blockOf(state));
	level.playSound(nullptr, pos, "minecraft:item.book.put", Level::SoundSource::Blocks, 1.0F, 1.0F);
	return true;
}

UseResult LecternBlock::useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit&) const {
	if (blocks().getBool(state, blocks().property("has_book"))) return UseResult::TryWithEmptyHand;
	ItemStack& held = heldStack(player, hand);
	if (!held.isEmpty() && level.gameData().isInTag("minecraft:item", "minecraft:lectern_books", held.item)) {
		return tryPlaceBook(level, pos, state, &player, held) ? UseResult::Success : UseResult::Pass;
	}
	return held.isEmpty() && hand == 0 ? UseResult::Pass : UseResult::TryWithEmptyHand;
}

bool LecternBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (!blocks().getBool(state, blocks().property("has_book"))) return true; // CONSUME
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	if (!dynamic_cast<LecternBlockEntity*>(entity.get())) return true;
	Menus::openStorage(
			player, level, [&](int id) { return std::make_unique<LecternMenu>(level, player, id, entity); }, {}, "container.lectern");
	return true;
}

// The end of the page change pulse
void LecternBlock::tick(Level& level, const BlockPos& pos, int state) const {
	level.setBlock(pos, blocks().withBool(state, blocks().property("powered"), false), Level::UPDATE_ALL);
	level.updateNeighborsAt(pos.below(), blockOf(state));
}

void LecternBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool) const {
	if (blocks().getBool(state, blocks().property("powered"))) level.updateNeighborsAt(pos.below(), blockOf(state));
}

int LecternBlock::getSignal(Level&, const BlockPos&, int state, Direction) const { return blocks().getBool(state, blocks().property("powered")) ? 15 : 0; }

int LecternBlock::getDirectSignal(Level&, const BlockPos&, int state, Direction direction) const {
	return direction == Direction::Up && blocks().getBool(state, blocks().property("powered")) ? 15 : 0;
}

int LecternBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction) const {
	if (!blocks().getBool(state, blocks().property("has_book"))) return 0;
	auto* lectern = level.getBlockEntity<LecternBlockEntity>(pos);
	return lectern ? lectern->redstoneSignal() : 0;
}

// ===== Crafter =====

Direction CrafterBlock::front(int state) const {
	std::string orientation = blocks().valueName(blocks().get(state, blocks().property("orientation")));
	return directionNamed(orientation.substr(0, orientation.find('_')));
}

// Its front toward the player (up and down too); its top up, or toward where the player looks when facing up/down
int CrafterBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	Direction frontFace = Directions::opposite(context.nearestLookingDirection());
	Direction top		= frontFace == Direction::Down ? Directions::opposite(context.horizontalDirection())
						  : frontFace == Direction::Up ? context.horizontalDirection()
													   : Direction::Up;
	constexpr const char* names[6] = {"down", "up", "north", "south", "west", "east"};
	std::string			  value	   = std::string(names[static_cast<int>(frontFace)]) + "_" + names[static_cast<int>(top)];
	int					  state	   = blocks().with(blocks().defaultState(context.block), blocks().property("orientation"), blocks().value(value));
	return blocks().withBool(state, blocks().property("triggered"), level.hasNeighborSignal(context.clickedPos));
}

void CrafterBlock::setPlacedBy(Level& level, const BlockPos& pos, int state) const {
	if (blocks().getBool(state, blocks().property("triggered"))) level.scheduleTick(pos, blockOf(state), 4);
}

std::unique_ptr<BlockEntity> CrafterBlock::newBlockEntity(const BlockPos& pos, int state) const {
	auto crafter = std::make_unique<CrafterBlockEntity>(pos);
	int	 triggered = blocks().property("triggered");
	crafter->setTriggered(blocks().has(state, triggered) && blocks().getBool(state, triggered));
	return crafter;
}

void CrafterBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	bool  powered	= level.hasNeighborSignal(pos);
	int	  triggered = blocks().property("triggered");
	bool  was		= blocks().getBool(state, triggered);
	auto* crafter	= level.getBlockEntity<CrafterBlockEntity>(pos);
	if (powered && !was) {
		level.scheduleTick(pos, blockOf(state), 4);
		level.setBlock(pos, blocks().withBool(state, triggered, true), Level::UPDATE_CLIENTS);
		if (crafter) crafter->setTriggered(true);
	} else if (!powered && was) {
		level.setBlock(pos, blocks().withBool(blocks().withBool(state, triggered, false), blocks().property("crafting"), false), Level::UPDATE_CLIENTS);
		if (crafter) crafter->setTriggered(false);
	}
}

void CrafterBlock::tick(Level& level, const BlockPos& pos, int state) const { dispenseFrom(level, pos, state); }

bool CrafterBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::shared_ptr<BlockEntity> entity	 = level.getSharedBlockEntity(pos);
	auto*						 crafter = dynamic_cast<CrafterBlockEntity*>(entity.get());
	if (!crafter) return true;
	Menus::openStorage(
			player, level, [&](int id) { return std::make_unique<CrafterMenu>(level, player, player.inventory(), id, entity); }, crafter->customName,
			crafter->defaultName());
	return true;
}

int CrafterBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int, Direction) const {
	auto* crafter = level.getBlockEntity<CrafterBlockEntity>(pos);
	return crafter ? crafter->redstoneSignal() : 0;
}

void CrafterBlock::dispenseFrom(Level& level, const BlockPos& pos, int state) const {
	auto* crafter = level.getBlockEntity<CrafterBlockEntity>(pos);
	if (!crafter) return;
	CraftingInput input	 = CraftingInput::ofPositioned(3, 3, crafter->items()).input;
	const Recipe* recipe = level.recipes().getRecipeFor(RecipeType::Crafting, input);
	ItemStack	  result = recipe ? recipe->assemble(input) : ItemStack();
	if (result.isEmpty()) {
		level.levelEvent(nullptr, LEVEL_EVENT_CRAFTER_FAIL, pos, 0);
		return;
	}
	crafter->setCraftingTicksRemaining(6);
	level.setBlock(pos, blocks().withBool(state, blocks().property("crafting"), true), Level::UPDATE_CLIENTS);
	dispenseItem(level, pos, *crafter, result, state);
	for (const ItemStack& remainder : recipe->remainingItems(input, level.gameData())) {
		if (!remainder.isEmpty()) dispenseItem(level, pos, *crafter, remainder, state);
	}
	for (ItemStack& stack : crafter->items()) {
		if (stack.isEmpty()) continue;
		stack.shrink(1);
		if (stack.count <= 0) stack = ItemStack();
	}
	crafter->setChanged();
}

// Into the container in front (one by one into a crafter, or when it can't take the whole stack), the rest thrown out
void CrafterBlock::dispenseItem(Level& level, const BlockPos& pos, CrafterBlockEntity& crafter, const ItemStack& stack, int state) const {
	Direction				   facing = front(state);
	Direction				   face	  = Directions::opposite(facing);
	std::shared_ptr<Container> target = Hoppers::containerAt(level, pos.relative(facing));
	ItemStack				   left	  = stack;
	if (target && (dynamic_cast<CrafterBlockEntity*>(target.get()) || stack.count > target->maxStackSizeFor(stack, level.gameData()))) {
		while (!left.isEmpty()) {
			ItemStack rest = Hoppers::addItem(level, &crafter, *target, left.copyWithCount(1), &face);
			if (!rest.isEmpty()) break;
			left.shrink(1);
		}
	} else if (target) {
		while (!left.isEmpty()) {
			int count = left.count;
			left	  = Hoppers::addItem(level, &crafter, *target, std::move(left), &face);
			if (count == left.count) break;
		}
	}
	if (left.count <= 0) return;
	// DefaultDispenseItemBehavior.spawnItem(level, stack, 6, facing, center + 0.7 toward the front)
	double x = pos.x + 0.5 + 0.7 * step(facing, 0);
	double y = pos.y + 0.5 + 0.7 * step(facing, 1);
	double z = pos.z + 0.5 + 0.7 * step(facing, 2);
	y -= facing == Direction::Up || facing == Direction::Down ? 0.125 : 0.15625;
	auto		item	 = ItemEntity::create(level, {x, y, z}, std::move(left));
	JavaRandom& random	 = level.random();
	double		speed	 = random.nextDouble() * 0.1 + 0.2;
	auto		triangle = [&random](double mode, double deviation) { return mode + deviation * (random.nextDouble() - random.nextDouble()); };
	double		dx		 = triangle(step(facing, 0) * speed, 0.0172275 * 6);
	double		dy		 = triangle(0.2, 0.0172275 * 6);
	double		dz		 = triangle(step(facing, 2) * speed, 0.0172275 * 6);
	item->setDeltaMovement({dx, dy, dz});
	level.entities().add(std::move(item));
	level.levelEvent(nullptr, LEVEL_EVENT_CRAFTER_CRAFT, pos, 0);
	level.levelEvent(nullptr, LEVEL_EVENT_CRAFTER_SHOOT, pos, static_cast<int>(facing));
}

