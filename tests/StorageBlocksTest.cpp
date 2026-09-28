#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/blocks/Containers.hpp"
#include "world/blocks/StorageBlocks.hpp"
#include "world/inventory/Menu.hpp"
#include "world/inventory/StorageMenus.hpp"
#include "world/item/Components.hpp"

// Chiseled bookshelves, decorated pots, jukeboxes, lecterns, crafters and shelves

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	std::shared_ptr<Player> player(LevelFixture& f, double x, double z) {
		auto created = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		created->setPosition(x, Y, z);
		return created;
	}
	// A right click on the face of the block at pos, hitting (u, v) of that face as world coordinates offsets
	bool useOn(LevelFixture& f, Player& who, BlockPos pos, Direction face, double x, double y, double z, int hand = 0) {
		int					 state	  = f.level->getBlockState(pos);
		const BlockBehavior& behavior = f.level->behavior(state);
		BlockHit			 hit{pos, face, pos.x + x, pos.y + y, pos.z + z};
		UseResult			 result = behavior.useItemOn(*f.level, pos, state, who, hand, hit);
		if (result == UseResult::Success || result == UseResult::Consume) return true;
		if (result == UseResult::TryWithEmptyHand && hand == 0) return behavior.useWithoutItemAt(*f.level, pos, state, who, hit);
		return false;
	}
	ItemStack& hand(Player& who) { return who.inventory().getMutable(who.handSlot(0)); }
	int		   comparator(LevelFixture& f, BlockPos pos, Direction direction = Direction::North) {
		  int state = f.level->getBlockState(pos);
		  return f.level->behavior(state).getAnalogOutputSignal(*f.level, pos, state, direction);
	}
	int itemsNamed(LevelFixture& f, const char* name) {
		int count = 0;
		for (ItemEntity* item : f.items()) {
			if (item->item().item == f.item(name)) count += item->item().count;
		}
		return count;
	}
	bool has(LevelFixture& f, BlockPos pos, const std::string& property) { return f.nameAt(pos.x, pos.y, pos.z).find(property) != std::string::npos; }

	// Saved and loaded again (the chunk format)
	template <typename T> std::unique_ptr<T> reloaded(LevelFixture& f, T& entity) {
		BlockEntityWriter out(f.data);
		entity.save(out);
		auto			  copy = std::make_unique<T>(entity.pos());
		BlockEntityReader in(f.data, out.out.data(), out.out.size());
		copy->load(in);
		return copy;
	}
	void writeString(std::vector<uint8_t>& out, const std::string& text) {
		out.push_back(static_cast<uint8_t>(text.size()));
		out.insert(out.end(), text.begin(), text.end());
	}
} // namespace

// ChiseledBookShelfBlock: the slot is the part of the front face hit (3 columns, 2 rows); comparators read the last
// slot used; hoppers take the books out
TEST(chiseled_bookshelf_slots) {
	LevelFixture f;
	auto bob = player(f, 0.5, -1.5);
	f.set(0, Y, 0, "minecraft:chiseled_bookshelf[facing=north,slot_0_occupied=false,slot_1_occupied=false,slot_2_occupied=false,slot_3_occupied=false,"
				   "slot_4_occupied=false,slot_5_occupied=false]");
	auto* shelf = f.level->getBlockEntity<ChiseledBookShelfBlockEntity>({0, Y, 0});
	CHECK(shelf != nullptr);
	if (!shelf) return;
	hand(*bob) = ItemStack(f.item("minecraft:book"), 2);
	// Facing north, the hit at x = 0.1 is the right column seen from the front: slot 2 (top row)
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::North, 0.1, 0.8, 0.0));
	CHECK(has(f, {0, Y, 0}, "slot_2_occupied=true"));
	CHECK_EQ(shelf->item(2).count, 1);
	CHECK_EQ(hand(*bob).count, 1);
	CHECK_EQ(comparator(f, {0, Y, 0}), 3);
	// Bottom left seen from the front (x = 0.9, y = 0.2): slot 3
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::North, 0.9, 0.2, 0.0));
	CHECK(has(f, {0, Y, 0}, "slot_3_occupied=true"));
	CHECK_EQ(comparator(f, {0, Y, 0}), 4);
	// Another face: nothing
	hand(*bob) = ItemStack(f.item("minecraft:book"), 1);
	CHECK(!useOn(f, *bob, {0, Y, 0}, Direction::East, 1.0, 0.8, 0.5));
	// Not a book: tries the empty hand, which takes the book of that slot
	hand(*bob) = ItemStack(f.item("minecraft:stone"), 1);
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::North, 0.1, 0.8, 0.0));
	CHECK(has(f, {0, Y, 0}, "slot_2_occupied=false"));
	int books = 0;
	for (int slot = 0; slot < PlayerInventory::SIZE; slot++) books += bob->inventory().get(slot).item == f.item("minecraft:book");
	CHECK_EQ(books, 1);
	CHECK_EQ(comparator(f, {0, Y, 0}), 3); // The last slot used

	// Saved and loaded
	auto copy = reloaded(f, *shelf);
	CHECK_EQ(copy->item(3).count, 1);
	CHECK_EQ(copy->lastInteractedSlot(), 2);

	// A hopper below takes the book out
	f.set(0, Y - 1, 0, "minecraft:hopper[enabled=true,facing=east]");
	f.tick(1);
	CHECK(shelf->item(3).isEmpty());
	CHECK(has(f, {0, Y, 0}, "slot_3_occupied=false"));

	// Broken: the books drop
	shelf->setItem(5, ItemStack(f.item("minecraft:enchanted_book"), 1));
	f.set(0, Y, 0, "minecraft:air");
	CHECK_EQ(itemsNamed(f, "minecraft:enchanted_book"), 1);
}

// DecoratedPotBlock: one more of the same item each use, wobbling; its sherds on the item it drops, or loose when
// cracked; its items spill
TEST(decorated_pot_items_and_sherds) {
	LevelFixture f;
	auto bob = player(f, 0.5, -1.5);
	f.set(0, Y, 0, "minecraft:decorated_pot[cracked=false,facing=north,waterlogged=false]");
	auto* pot = f.level->getBlockEntity<DecoratedPotBlockEntity>({0, Y, 0});
	CHECK(pot != nullptr);
	if (!pot) return;
	// Placed from an item with sherds
	ItemStack potItem(f.item("minecraft:decorated_pot"), 1);
	int		  brick = f.item("minecraft:brick"), skull = f.item("minecraft:skull_pottery_sherd");
	Components::set(potItem, f.data, "minecraft:pot_decorations", Components::encodePotDecorations({skull, brick, brick, skull}));
	ContainerItems::apply(*pot, potItem, f.data);
	CHECK(pot->decorations == (std::array<int, 4>{skull, 0, 0, skull}));

	hand(*bob) = ItemStack(f.item("minecraft:wheat"), 3);
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	CHECK_EQ(pot->theItem().count, 2);
	CHECK_EQ(hand(*bob).count, 1);
	CHECK_EQ(comparator(f, {0, Y, 0}), 1); // 2 of 64
	// Another item: the empty hand's use, a wobble
	hand(*bob) = ItemStack(f.item("minecraft:stone"), 1);
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	CHECK_EQ(hand(*bob).count, 1);
	// Creative: not used up
	bob->setGameMode(GameMode::Creative);
	hand(*bob) = ItemStack(f.item("minecraft:wheat"), 1);
	useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5);
	CHECK_EQ(pot->theItem().count, 3);
	CHECK_EQ(hand(*bob).count, 1);

	// Its item keeps the sherds (loot table copy_components)
	ItemStack dropped(f.item("minecraft:decorated_pot"), 1);
	ContainerItems::collect(*pot, dropped, f.data);
	std::optional<std::vector<uint8_t>> sherds = Components::get(dropped, f.data, "minecraft:pot_decorations");
	CHECK(sherds && Components::decodePotDecorations(*sherds) == (std::array<int, 4>{skull, brick, brick, skull}));
	auto copy = reloaded(f, *pot);
	CHECK(copy->decorations == pot->decorations);
	CHECK_EQ(copy->theItem().count, 3);

	// Broken by a pickaxe (#breaks_decorated_pots): cracked, the 4 sides drop, and the wheat
	bob->setGameMode(GameMode::Survival);
	hand(*bob) = ItemStack(f.item("minecraft:iron_pickaxe"), 1);
	int state  = f.at(0, Y, 0);
	f.level->behavior(state).playerWillDestroy(*f.level, {0, Y, 0}, state, *bob);
	CHECK(has(f, {0, Y, 0}, "cracked=true"));
	f.level->destroyBlock({0, Y, 0}, true);
	CHECK_EQ(itemsNamed(f, "minecraft:skull_pottery_sherd"), 2);
	CHECK_EQ(itemsNamed(f, "minecraft:brick"), 2);
	CHECK_EQ(itemsNamed(f, "minecraft:wheat"), 3);
	CHECK_EQ(itemsNamed(f, "minecraft:decorated_pot"), 0);
}

// JukeboxBlock: a disc plays its song (comparators read the song, it powers around while playing), stops after the
// song; used again, the disc comes out
TEST(jukebox_plays_disc) {
	LevelFixture f;
	auto bob = player(f, 0.5, -1.5);
	f.set(0, Y, 0, "minecraft:jukebox[has_record=false]");
	auto* jukebox = f.level->getBlockEntity<JukeboxBlockEntity>({0, Y, 0});
	CHECK(jukebox != nullptr);
	if (!jukebox) return;
	// Not a disc: nothing
	hand(*bob) = ItemStack(f.item("minecraft:stone"), 1);
	CHECK(!useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	hand(*bob) = ItemStack(f.item("minecraft:music_disc_cat"), 1);
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	CHECK(hand(*bob).isEmpty());
	CHECK(has(f, {0, Y, 0}, "has_record=true"));
	CHECK(jukebox->isPlaying());
	CHECK_EQ(comparator(f, {0, Y, 0}), 2);
	int state = f.at(0, Y, 0);
	CHECK_EQ(f.level->behavior(state).getSignal(*f.level, {0, Y, 0}, state, Direction::West), 15);
	// A saved song goes on where it was
	f.tick(100);
	auto copy = reloaded(f, *jukebox);
	CHECK(copy->isPlaying());
	CHECK_EQ(int(copy->ticksSinceSongStarted()), int(jukebox->ticksSinceSongStarted()));
	// 185 s + 20 ticks: over (seen on the tick after), the disc stays
	f.tick(185 * 20 + 20 - 100);
	CHECK(jukebox->isPlaying());
	f.tick(1);
	CHECK(!jukebox->isPlaying());
	CHECK(has(f, {0, Y, 0}, "has_record=true"));
	CHECK_EQ(f.level->behavior(state).getSignal(*f.level, {0, Y, 0}, state, Direction::West), 0);
	CHECK_EQ(comparator(f, {0, Y, 0}), 2);
	// Used again: the disc flies out
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	CHECK(has(f, {0, Y, 0}, "has_record=false"));
	CHECK_EQ(itemsNamed(f, "minecraft:music_disc_cat"), 1);
	CHECK_EQ(comparator(f, {0, Y, 0}), 0);

	// A hopper above puts a disc in, which plays
	f.set(0, Y + 1, 0, "minecraft:hopper[enabled=true,facing=down]");
	f.level->getBlockEntity<HopperBlockEntity>({0, Y + 1, 0})->setItem(0, ItemStack(f.item("minecraft:music_disc_11"), 1));
	f.tick(1);
	CHECK(jukebox->isPlaying());
	CHECK_EQ(comparator(f, {0, Y, 0}), 11);
	// Broken: the disc drops
	f.set(0, Y, 0, "minecraft:stone");
	CHECK_EQ(itemsNamed(f, "minecraft:music_disc_11"), 1);
}

// LecternBlock: a book put on it opens for everyone; page buttons give a redstone pulse; comparators read the page;
// taking it resets the lectern
TEST(lectern_book_pages) {
	LevelFixture f;
	auto bob = player(f, 0.5, -1.5);
	f.set(0, Y, 0, "minecraft:lectern[facing=north,has_book=false,powered=false]");
	ItemStack			 book(f.item("minecraft:writable_book"), 1);
	std::vector<uint8_t> pages{3};
	for (const char* text : {"one", "two", "three"}) {
		writeString(pages, text);
		pages.push_back(0); // Not filtered
	}
	Components::set(book, f.data, "minecraft:writable_book_content", pages);
	// Without a book in hand, nothing opens
	CHECK(!useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5)); // PASS
	CHECK(!bob->openMenuSlot());
	hand(*bob) = book;
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	CHECK(has(f, {0, Y, 0}, "has_book=true"));
	CHECK(hand(*bob).isEmpty());
	auto* lectern = f.level->getBlockEntity<LecternBlockEntity>({0, Y, 0});
	CHECK_EQ(comparator(f, {0, Y, 0}), 1); // Page 0 of 3

	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5));
	Menu& menu = Menus::current(*bob, *f.level);
	CHECK(menu.type() == "minecraft:lectern");
	CHECK(menu.clickMenuButton(2)); // Next page
	CHECK_EQ(lectern->page(), 1);
	CHECK(has(f, {0, Y, 0}, "powered=true"));
	CHECK_EQ(comparator(f, {0, Y, 0}), 8); // floor(0.5 * 14) + 1
	f.tick(2);
	CHECK(has(f, {0, Y, 0}, "powered=false"));
	CHECK(menu.clickMenuButton(102)); // Page 2
	CHECK_EQ(comparator(f, {0, Y, 0}), 15);
	CHECK(menu.clickMenuButton(2)); // Past the end: stays
	CHECK_EQ(lectern->page(), 2);
	auto copy = reloaded(f, *lectern);
	CHECK_EQ(copy->page(), 2);
	// Take the book
	CHECK(menu.clickMenuButton(3));
	CHECK(has(f, {0, Y, 0}, "has_book=false"));
	CHECK(!menu.stillValid());
	int books = 0;
	for (int slot = 0; slot < PlayerInventory::SIZE; slot++) books += bob->inventory().get(slot).item == f.item("minecraft:writable_book");
	CHECK_EQ(books, 1);

	// Broken with a book: the book drops
	Menus::doCloseContainer(*bob, *f.level);
	hand(*bob) = book;
	useOn(f, *bob, {0, Y, 0}, Direction::Up, 0.5, 1.0, 0.5);
	f.set(0, Y, 0, "minecraft:air");
	CHECK_EQ(itemsNamed(f, "minecraft:writable_book"), 1);
}

// CrafterBlock: crafts once per rising edge (4 ticks later), throws the result and the remainders out of its front,
// uses one of each ingredient; disabled slots count for comparators and take nothing
TEST(crafter_crafts_on_pulse) {
	LevelFixture f;
	auto bob = player(f, 0.5, -1.5);
	f.set(0, Y, 0, "minecraft:crafter[crafting=false,orientation=east_up,triggered=false]");
	auto* crafter = f.level->getBlockEntity<CrafterBlockEntity>({0, Y, 0});
	CHECK(crafter != nullptr);
	if (!crafter) return;
	crafter->setItem(4, ItemStack(f.item("minecraft:oak_log"), 2));
	// The menu shows what it would craft
	int state = f.at(0, Y, 0);
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *bob);
	Menu& menu = Menus::current(*bob, *f.level);
	CHECK(menu.type() == "minecraft:crafter_3x3");
	CHECK_EQ(int(menu.slots().size()), 46);
	CHECK_EQ(menu.slots()[45].item().item, f.item("minecraft:oak_planks"));
	CHECK(!menu.slots()[45].mayPickup());
	// A disabled (empty) slot: nothing goes in, comparators count it
	crafter->setSlotState(0, false);
	CHECK(crafter->isSlotDisabled(0));
	CHECK(!menu.slots()[0].mayPlace(ItemStack(f.item("minecraft:stone"), 1), f.data));
	CHECK_EQ(comparator(f, {0, Y, 0}), 2);
	crafter->setSlotState(4, false); // Not empty: stays enabled
	CHECK(!crafter->isSlotDisabled(4));
	Menus::doCloseContainer(*bob, *f.level);

	f.set(-1, Y, 0, "minecraft:redstone_block");
	CHECK(has(f, {0, Y, 0}, "triggered=true"));
	f.tick(3);
	CHECK_EQ(itemsNamed(f, "minecraft:oak_planks"), 0);
	f.tick(1);
	CHECK_EQ(itemsNamed(f, "minecraft:oak_planks"), 4);
	CHECK_EQ(crafter->item(4).count, 1);
	CHECK(has(f, {0, Y, 0}, "crafting=true"));
	f.tick(7);
	CHECK(has(f, {0, Y, 0}, "crafting=false"));
	// Still powered: no second craft
	CHECK_EQ(crafter->item(4).count, 1);

	// Remainders: honey bottles into sugar leave their bottles, into the chest in front
	f.set(-1, Y, 0, "minecraft:air");
	f.set(1, Y, 0, "minecraft:chest[facing=north,type=single,waterlogged=false]");
	crafter->setItem(4, ItemStack());
	crafter->setItem(1, ItemStack(f.item("minecraft:honey_bottle"), 1));
	f.set(-1, Y, 0, "minecraft:redstone_block");
	f.tick(5);
	auto* chest = f.level->getBlockEntity<ChestBlockEntity>({1, Y, 0});
	CHECK_EQ(chest->item(0).item, f.item("minecraft:sugar"));
	CHECK_EQ(chest->item(0).count, 3);
	CHECK_EQ(chest->item(1).item, f.item("minecraft:glass_bottle"));
	CHECK(crafter->item(1).isEmpty());
	auto copy = reloaded(f, *crafter);
	CHECK(copy->isSlotDisabled(0));
	CHECK(copy->isTriggered());
}

// Hoppers fill a crafter's slots evenly
TEST(crafter_hopper_fills_evenly) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:crafter[crafting=false,orientation=north_up,triggered=false]");
	f.set(0, Y + 1, 0, "minecraft:hopper[enabled=true,facing=down]");
	auto* crafter = f.level->getBlockEntity<CrafterBlockEntity>({0, Y, 0});
	auto* hopper  = f.level->getBlockEntity<HopperBlockEntity>({0, Y + 1, 0});
	crafter->setSlotState(8, false);
	crafter->setItem(0, ItemStack(f.item("minecraft:stone"), 1));
	crafter->setItem(1, ItemStack(f.item("minecraft:stone"), 1));
	hopper->setItem(0, ItemStack(f.item("minecraft:stone"), 10));
	f.tick(8 * 7 + 1);
	// Slots 2-7 first (empty), then 0 and 1 again; slot 8 disabled
	for (int i = 0; i < 8; i++) CHECK_EQ(crafter->item(i).count, i < 2 ? 2 : 1);
	CHECK(crafter->item(8).isEmpty());
}

// ShelfBlock: swaps the hand with the slot hit; comparators behind read the slots as bits; powered shelves side by
// side chain up and swap with the hotbar
TEST(shelf_swaps) {
	LevelFixture f;
	auto bob = player(f, 0.5, -1.5);
	f.set(0, Y, 0, "minecraft:oak_shelf[facing=north,powered=false,side_chain=unconnected,waterlogged=false]");
	auto* shelf = f.level->getBlockEntity<ShelfBlockEntity>({0, Y, 0});
	CHECK(shelf != nullptr);
	if (!shelf) return;
	hand(*bob) = ItemStack(f.item("minecraft:stone"), 5);
	// Facing north, x = 0.9 is the left of the front: slot 0
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::North, 0.9, 0.5, 0.0));
	CHECK_EQ(shelf->item(0).count, 5);
	CHECK(hand(*bob).isEmpty());
	CHECK_EQ(comparator(f, {0, Y, 0}, Direction::South), 1);
	CHECK_EQ(comparator(f, {0, Y, 0}, Direction::North), 0);
	// Empty hand: takes it back
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::North, 0.9, 0.5, 0.0));
	CHECK_EQ(hand(*bob).count, 5);
	CHECK(!useOn(f, *bob, {0, Y, 0}, Direction::North, 0.1, 0.5, 0.0) || shelf->item(2).count == 5);

	// Three powered shelves in a row: one chain, left to right as seen from the front
	f.set(1, Y, 0, "minecraft:oak_shelf[facing=north,powered=false,side_chain=unconnected,waterlogged=false]");
	f.set(-1, Y, 0, "minecraft:oak_shelf[facing=north,powered=false,side_chain=unconnected,waterlogged=false]");
	f.set(0, Y, 1, "minecraft:redstone_block"); // Behind the middle one: powers it
	f.set(1, Y, 1, "minecraft:redstone_block");
	f.set(-1, Y, 1, "minecraft:redstone_block");
	CHECK(has(f, {0, Y, 0}, "powered=true"));
	auto& shelves = static_cast<const ShelfBlock&>(f.level->behavior(f.at(0, Y, 0)));
	CHECK_EQ(int(shelves.connectedTo(*f.level, {0, Y, 0}).size()), 3);
	for (int i = 0; i < 9; i++) bob->inventory().set(PlayerInventory::HOTBAR + i, ItemStack(f.item("minecraft:dirt"), i + 1));
	CHECK(useOn(f, *bob, {0, Y, 0}, Direction::North, 0.5, 0.5, 0.0));
	// The leftmost shelf (east, facing north) gets hotbar slots 0-2
	auto* left = f.level->getBlockEntity<ShelfBlockEntity>({1, Y, 0});
	CHECK_EQ(left->item(0).count, 1);
	CHECK_EQ(left->item(2).count, 3);
	CHECK_EQ(f.level->getBlockEntity<ShelfBlockEntity>({-1, Y, 0})->item(2).count, 9);
	auto copy = reloaded(f, *left);
	CHECK_EQ(copy->item(1).count, 2);
}
