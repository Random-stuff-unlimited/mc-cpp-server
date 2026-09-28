#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/inventory/Menu.hpp"

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	DispenserBlockEntity* dispenserAt(LevelFixture& f, int x, int y, int z) { return f.level->getBlockEntity<DispenserBlockEntity>({x, y, z}); }
	int itemsNamed(LevelFixture& f, const char* name) {
		int count = 0;
		for (ItemEntity* item : f.items()) {
			if (item->item().item == f.item(name)) count += item->item().count;
		}
		return count;
	}
} // namespace

// Placed droppers get their block entity; powered, they drop one item after 4 ticks, once per rising edge
TEST(container_dropper_fires) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:dropper[facing=east,triggered=false]");
	DispenserBlockEntity* dropper = dispenserAt(f, 0, Y, 0);
	CHECK(dropper != nullptr);
	if (!dropper) return;
	dropper->setItem(4, ItemStack(f.item("minecraft:cobblestone"), 3));
	f.tick(1);
	f.set(0, Y, 1, "minecraft:redstone_block");
	f.tick(3);
	CHECK_EQ(itemsNamed(f, "minecraft:cobblestone"), 0);
	f.tick(1);
	CHECK_EQ(itemsNamed(f, "minecraft:cobblestone"), 1);
	CHECK_EQ(dropper->item(4).count, 2);
	f.tick(20); // Still powered: nothing more
	CHECK_EQ(dropper->item(4).count, 2);
	// A comparator reads how full it is: 2 cobblestone of 9 x 64 -> 1
	CHECK_EQ(f.level->behavior(f.at(0, Y, 0)).getAnalogOutputSignal(*f.level, {0, Y, 0}, f.at(0, Y, 0), Direction::West), 1);
	// Breaking it drops what is left
	f.set(0, Y, 0, "minecraft:air");
	f.tick(1);
	CHECK_EQ(itemsNamed(f, "minecraft:cobblestone"), 3);
}

// A dropper facing another container puts the item in it
TEST(container_dropper_into_dropper) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:dropper[facing=east,triggered=false]");
	f.set(1, Y, 0, "minecraft:dropper[facing=up,triggered=false]");
	dispenserAt(f, 0, Y, 0)->setItem(0, ItemStack(f.item("minecraft:redstone"), 5));
	f.set(-1, Y, 0, "minecraft:redstone_block");
	f.tick(5);
	CHECK_EQ(dispenserAt(f, 0, Y, 0)->item(0).count, 4);
	CHECK_EQ(dispenserAt(f, 1, Y, 0)->item(0).count, 1);
	CHECK_EQ(itemsNamed(f, "minecraft:redstone"), 0);
}

// Dispensers pour water buckets and fill empty ones
TEST(container_dispenser_buckets) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:dispenser[facing=east,triggered=false]");
	dispenserAt(f, 0, Y, 0)->setItem(0, ItemStack(f.item("minecraft:water_bucket"), 1));
	f.set(-1, Y, 0, "minecraft:redstone_block");
	f.tick(5);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:water[level=0]");
	CHECK_EQ(dispenserAt(f, 0, Y, 0)->item(0).item, f.item("minecraft:bucket"));
	// Off, then on again: the empty bucket takes the source back
	f.set(-1, Y, 0, "minecraft:air");
	f.tick(2);
	f.set(-1, Y, 0, "minecraft:redstone_block");
	f.tick(5);
	CHECK_EQ(dispenserAt(f, 0, Y, 0)->item(0).item, f.item("minecraft:water_bucket"));
	CHECK(f.nameAt(1, Y, 0) != "minecraft:water[level=0]");
}

// The dispenser's menu: picking up, placing one or all, shift-click, dragging, closing
TEST(container_menu_clicks) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setPosition(0.5, Y, 1.5);
	f.set(0, Y, 0, "minecraft:dispenser[facing=north,triggered=false]");
	int stone = f.item("minecraft:stone");
	player->inventory().set(PlayerInventory::HOTBAR, ItemStack(stone, 10));
	int state = f.at(0, Y, 0);
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	Menu& menu = Menus::current(*player, *f.level);
	CHECK(menu.type() == "minecraft:generic_3x3");
	CHECK_EQ(int(menu.slots().size()), 45);
	DispenserBlockEntity& dispenser = *dispenserAt(f, 0, Y, 0);

	menu.clicked(36, 0, ClickType::Pickup); // First hotbar slot: the 10 stone on the cursor
	CHECK_EQ(menu.carried().count, 10);
	menu.clicked(4, 1, ClickType::Pickup); // Right click: one
	menu.clicked(4, 0, ClickType::Pickup); // Left click: the rest
	CHECK_EQ(dispenser.item(4).count, 10);
	CHECK(menu.carried().isEmpty());
	menu.clicked(4, 1, ClickType::Pickup); // Right click on a stack: half of it
	CHECK_EQ(menu.carried().count, 5);
	CHECK_EQ(dispenser.item(4).count, 5);

	// Dragging 5 over 2 slots (charitable): 2 each, 1 left on the cursor
	menu.clicked(-999, 0, ClickType::QuickCraft); // Start, type 0
	menu.clicked(0, 1, ClickType::QuickCraft);
	menu.clicked(1, 1, ClickType::QuickCraft);
	menu.clicked(-999, 2, ClickType::QuickCraft); // End
	CHECK_EQ(dispenser.item(0).count, 2);
	CHECK_EQ(dispenser.item(1).count, 2);
	CHECK_EQ(menu.carried().count, 1);

	// Shift-click: into the inventory, hotbar end first
	menu.clicked(4, 0, ClickType::QuickMove);
	CHECK(dispenser.item(4).isEmpty());
	CHECK_EQ(player->inventory().get(44).count, 5);
	// Number key 3 on a dispenser slot: swapped with the hotbar
	menu.clicked(0, 2, ClickType::Swap);
	CHECK_EQ(player->inventory().get(PlayerInventory::HOTBAR + 2).count, 2);
	CHECK(dispenser.item(0).isEmpty());

	// Closing: the cursor's stone goes back into the inventory
	Menus::doCloseContainer(*player, *f.level);
	CHECK(&Menus::current(*player, *f.level) != &menu);
	int total = 0;
	for (int slot = 0; slot < PlayerInventory::SIZE; slot++) total += player->inventory().get(slot).item == stone ? player->inventory().get(slot).count : 0;
	CHECK_EQ(total + dispenser.item(1).count, 10);
}

// The player's inventory menu: shift-click puts armor on
TEST(container_inventory_armor) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->inventory().set(PlayerInventory::HOTBAR, ItemStack(f.item("minecraft:iron_helmet"), 1));
	Menu& menu = Menus::inventory(*player, *f.level);
	menu.clicked(36, 0, ClickType::QuickMove);
	CHECK_EQ(player->inventory().get(5).item, f.item("minecraft:iron_helmet")); // Head slot
	// A stone can't go in an armor slot
	player->inventory().set(PlayerInventory::HOTBAR + 1, ItemStack(f.item("minecraft:stone"), 1));
	menu.clicked(37, 0, ClickType::Pickup);
	menu.clicked(6, 0, ClickType::Pickup);
	CHECK(player->inventory().get(6).isEmpty());
	CHECK_EQ(menu.carried().count, 1);
}

// A crafting table opens a 3x3 grid; shift-click fills the grid; closing gives the grid back; breaking the table
// closes it
TEST(container_crafting_table) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setPosition(0.5, Y, 1.5);
	f.set(0, Y, 0, "minecraft:crafting_table");
	int planks = f.item("minecraft:oak_planks");
	player->inventory().set(PlayerInventory::HOTBAR, ItemStack(planks, 5));
	int state = f.at(0, Y, 0);
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	Menu& menu = Menus::current(*player, *f.level);
	CHECK(menu.type() == "minecraft:crafting");
	CHECK_EQ(int(menu.slots().size()), 46);

	menu.clicked(37, 0, ClickType::QuickMove); // Hotbar (menu slot 37): into the grid
	CHECK_EQ(menu.slots()[1].item().count, 5);
	CHECK(player->inventory().get(PlayerInventory::HOTBAR).isEmpty());
	menu.clicked(1, 1, ClickType::Pickup); // Half of it on the cursor
	menu.clicked(5, 0, ClickType::Pickup); // Into the middle
	Menus::doCloseContainer(*player, *f.level);
	int total = 0;
	for (int slot = 0; slot < PlayerInventory::SIZE; slot++) total += player->inventory().get(slot).item == planks ? player->inventory().get(slot).count : 0;
	CHECK_EQ(total, 5);

	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	CHECK(Menus::current(*player, *f.level).stillValid());
	f.set(0, Y, 0, "minecraft:air");
	CHECK(!Menus::current(*player, *f.level).stillValid());
}
