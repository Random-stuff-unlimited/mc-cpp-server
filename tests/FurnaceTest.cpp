#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/blockentity/ProcessingEntities.hpp"
#include "world/inventory/ProcessingMenus.hpp"
#include "world/item/FuelValues.hpp"
#include "world/item/PlayerInventory.hpp"
#include "world/item/PotionBrewing.hpp"

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	AbstractFurnaceBlockEntity* furnaceAt(LevelFixture& f, int x, int y, int z) { return f.level->getBlockEntity<AbstractFurnaceBlockEntity>({x, y, z}); }
	ItemStack stack(LevelFixture& f, const char* name, int count) { return ItemStack(f.item(name), count); }
	std::string litAt(LevelFixture& f, int x, int y, int z) { return f.data.getProperty(f.at(x, y, z), "lit"); }
} // namespace

// The fuel table: vanilla burn times, tags resolved, non-flammable wood removed
TEST(furnace_fuel_values) {
	LevelFixture f;
	const FuelValues& fuel = f.level->fuelValues();
	CHECK_EQ(fuel.burnDuration(stack(f, "minecraft:coal", 1)), 1600);
	CHECK_EQ(fuel.burnDuration(stack(f, "minecraft:lava_bucket", 1)), 20000);
	CHECK_EQ(fuel.burnDuration(stack(f, "minecraft:oak_planks", 1)), 300);
	CHECK_EQ(fuel.burnDuration(stack(f, "minecraft:oak_slab", 1)), 150);
	CHECK_EQ(fuel.burnDuration(stack(f, "minecraft:white_carpet", 1)), 67);
	CHECK_EQ(fuel.burnDuration(stack(f, "minecraft:dried_kelp_block", 1)), 4001);
	CHECK(!fuel.isFuel(stack(f, "minecraft:crimson_planks", 1)));
	CHECK(!fuel.isFuel(stack(f, "minecraft:stone", 1)));
}

// Iron ore and coal: lit on the first tick, an ingot after 200 ticks, 1600 ticks of burning
TEST(furnace_smelts_iron) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:furnace[facing=north,lit=false]");
	AbstractFurnaceBlockEntity* furnace = furnaceAt(f, 0, Y, 0);
	CHECK(furnace != nullptr);
	if (!furnace) return;
	furnace->setItem(0, stack(f, "minecraft:iron_ore", 2));
	furnace->setItem(1, stack(f, "minecraft:coal", 1));
	CHECK_EQ(furnace->cookingTotalTime, 200);
	f.tick(1);
	CHECK(furnace->item(1).isEmpty());
	CHECK_EQ(furnace->litTotalTime, 1600);
	CHECK(litAt(f, 0, Y, 0) == "true");
	f.tick(198);
	CHECK(furnace->item(2).isEmpty());
	CHECK_EQ(furnace->cookingTimer, 199);
	f.tick(1);
	CHECK_EQ(furnace->item(2).item, f.item("minecraft:iron_ingot"));
	CHECK_EQ(furnace->item(0).count, 1);
	// Data slots in vanilla order: lit time, lit duration, cooking progress, total cooking time
	CHECK_EQ(furnace->data(0), 1401);
	CHECK_EQ(furnace->data(1), 1600);
	CHECK_EQ(furnace->data(2), 0);
	CHECK_EQ(furnace->data(3), 200);
	CHECK_EQ(furnace->recipesUsed.size(), size_t(1));
	f.tick(200);
	CHECK_EQ(furnace->item(2).count, 2);
	CHECK(furnace->item(0).isEmpty());
	// Nothing left to cook: it burns out, then goes out
	f.tick(1200);
	CHECK(litAt(f, 0, Y, 0) == "true");
	f.tick(1);
	CHECK(litAt(f, 0, Y, 0) == "false");
	// A comparator reads it: 2 ingots of 3 x 64
	CHECK_EQ(f.level->behavior(f.at(0, Y, 0)).getAnalogOutputSignal(*f.level, {0, Y, 0}, f.at(0, Y, 0), Direction::North), 1);
	// Broken: its items drop
	f.set(0, Y, 0, "minecraft:air");
	int ingots = 0;
	for (ItemEntity* item : f.items()) ingots += item->item().item == f.item("minecraft:iron_ingot") ? item->item().count : 0;
	CHECK_EQ(ingots, 2);
}

// A lava bucket burns and leaves its bucket; a blast furnace cooks in 100 ticks with half the burn time
TEST(furnace_lava_and_blast_furnace) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:blast_furnace[facing=north,lit=false]");
	AbstractFurnaceBlockEntity* furnace = furnaceAt(f, 0, Y, 0);
	CHECK(furnace != nullptr);
	if (!furnace) return;
	CHECK(furnace->defaultName() == "container.blast_furnace");
	furnace->setItem(0, stack(f, "minecraft:iron_ore", 1));
	furnace->setItem(1, stack(f, "minecraft:lava_bucket", 1));
	f.tick(1);
	CHECK_EQ(furnace->item(1).item, f.item("minecraft:bucket"));
	CHECK_EQ(furnace->litTotalTime, 10000);
	f.tick(99);
	CHECK_EQ(furnace->item(2).item, f.item("minecraft:iron_ingot"));
	// Food doesn't go in a blast furnace
	furnace->setItem(0, stack(f, "minecraft:beef", 1));
	f.tick(200);
	CHECK_EQ(furnace->item(0).count, 1);
}

// Hoppers: from above into the input, from the side into the fuel, from below out of the result
TEST(furnace_hoppers) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:furnace[facing=north,lit=false]");
	f.set(0, Y + 1, 0, "minecraft:hopper[enabled=true,facing=down]");
	f.set(1, Y, 0, "minecraft:hopper[enabled=true,facing=west]");
	f.set(0, Y - 1, 0, "minecraft:hopper[enabled=true,facing=down]");
	auto* above = f.level->getBlockEntity<HopperBlockEntity>({0, Y + 1, 0});
	auto* side	= f.level->getBlockEntity<HopperBlockEntity>({1, Y, 0});
	auto* below = f.level->getBlockEntity<HopperBlockEntity>({0, Y - 1, 0});
	CHECK(above && side && below);
	if (!above || !side || !below) return;
	above->setItem(0, stack(f, "minecraft:iron_ore", 1));
	side->setItem(0, stack(f, "minecraft:coal", 1));
	side->setItem(1, stack(f, "minecraft:dirt", 1)); // Not a fuel: stays in the hopper
	f.tick(1);
	AbstractFurnaceBlockEntity* furnace = furnaceAt(f, 0, Y, 0);
	f.tick(210);
	CHECK(above->item(0).isEmpty());
	CHECK(side->item(0).isEmpty());
	CHECK_EQ(side->item(1).count, 1);
	CHECK(furnace->item(2).isEmpty());
	CHECK_EQ(below->item(0).item, f.item("minecraft:iron_ingot"));
	// The fuel slot only gives buckets
	furnace->setItem(1, stack(f, "minecraft:coal", 3));
	f.tick(20);
	CHECK_EQ(furnace->item(1).count, 3);
}

// The menu: shift-click sends ores to the input and fuels to the fuel slot; the result slot takes nothing; one
// bucket at a time in the fuel slot
TEST(furnace_menu) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setPosition(0.5, Y, 1.5);
	f.set(0, Y, 0, "minecraft:smoker[facing=south,lit=false]");
	int state = f.at(0, Y, 0);
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	Menu& menu = Menus::current(*player, *f.level);
	CHECK(menu.type() == "minecraft:smoker");
	CHECK_EQ(int(menu.slots().size()), 39);
	AbstractFurnaceBlockEntity& furnace = *furnaceAt(f, 0, Y, 0);

	player->inventory().set(PlayerInventory::HOTBAR, stack(f, "minecraft:beef", 5));
	player->inventory().set(PlayerInventory::HOTBAR + 1, stack(f, "minecraft:coal", 4));
	player->inventory().set(PlayerInventory::HOTBAR + 2, stack(f, "minecraft:iron_ore", 4)); // Not smoked
	player->inventory().set(PlayerInventory::HOTBAR + 3, stack(f, "minecraft:bucket", 2));
	menu.clicked(30, 0, ClickType::QuickMove);
	CHECK_EQ(furnace.item(0).count, 5);
	menu.clicked(31, 0, ClickType::QuickMove);
	CHECK_EQ(furnace.item(1).count, 4);
	menu.clicked(32, 0, ClickType::QuickMove); // Hotbar -> inventory
	CHECK_EQ(player->inventory().get(9).item, f.item("minecraft:iron_ore"));
	// Onto the result slot: refused
	menu.clicked(3, 0, ClickType::Pickup);
	menu.clicked(2, 0, ClickType::Pickup);
	CHECK(furnace.item(2).isEmpty());
	CHECK_EQ(menu.carried().count, 4);
	menu.clicked(3, 0, ClickType::Pickup);
	// Fuel slot out, then two buckets onto it: one goes in
	menu.clicked(1, 0, ClickType::QuickMove);
	CHECK(furnace.item(1).isEmpty());
	menu.clicked(33, 0, ClickType::Pickup);
	menu.clicked(1, 0, ClickType::Pickup);
	CHECK_EQ(furnace.item(1).count, 1);
	CHECK_EQ(menu.carried().count, 1);
	Menus::doCloseContainer(*player, *f.level);
}

// The recipe book places the ingredient in the input slot
TEST(furnace_recipe_book) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setPosition(0.5, Y, 1.5);
	f.set(0, Y, 0, "minecraft:furnace[facing=south,lit=false]");
	int state = f.at(0, Y, 0);
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	auto* menu = dynamic_cast<AbstractFurnaceMenu*>(&Menus::current(*player, *f.level));
	CHECK(menu != nullptr);
	if (!menu) return;
	player->inventory().set(PlayerInventory::HOTBAR, stack(f, "minecraft:iron_ore", 10));
	const Recipe* recipe = f.level->recipes().byId("minecraft:iron_ingot_from_smelting_iron_ore");
	CHECK(recipe != nullptr);
	if (!recipe) return;
	CHECK(!menu->handlePlacement(*recipe, false, false));
	CHECK_EQ(furnaceAt(f, 0, Y, 0)->item(0).count, 1);
	CHECK(!menu->handlePlacement(*recipe, false, false));
	CHECK_EQ(furnaceAt(f, 0, Y, 0)->item(0).count, 2);
	CHECK(!menu->handlePlacement(*recipe, true, false));
	CHECK_EQ(furnaceAt(f, 0, Y, 0)->item(0).count, 10);
	// Missing items: a ghost recipe
	const Recipe* gold = f.level->recipes().byId("minecraft:gold_ingot_from_smelting_gold_ore");
	if (gold) CHECK(menu->handlePlacement(*gold, false, false));
}

// Saving keeps the items, the burn and cooking progress and the recipes used
TEST(furnace_save_load) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:furnace[facing=north,lit=false]");
	AbstractFurnaceBlockEntity* furnace = furnaceAt(f, 0, Y, 0);
	furnace->setItem(0, stack(f, "minecraft:iron_ore", 3));
	furnace->setItem(1, stack(f, "minecraft:coal", 2));
	f.tick(250);
	BlockEntityWriter out(f.data);
	furnace->save(out);
	AbstractFurnaceBlockEntity copy("minecraft:furnace", {0, Y, 0});
	BlockEntityReader		   in(f.data, out.out.data(), out.out.size());
	copy.load(in);
	CHECK_EQ(copy.item(0).count, 2);
	CHECK_EQ(copy.item(1).count, 1);
	CHECK_EQ(copy.item(2).count, 1);
	CHECK_EQ(copy.cookingTimer, furnace->cookingTimer);
	CHECK_EQ(copy.cookingTimer, 50);
	CHECK_EQ(copy.cookingTotalTime, 200);
	CHECK_EQ(copy.litTimeRemaining, furnace->litTimeRemaining);
	CHECK_EQ(copy.litTotalTime, 1600);
	CHECK_EQ(copy.recipesUsed.size(), size_t(1));
}

// Water bottle + nether wart with blaze powder: an awkward potion after 400 ticks
TEST(brewing_awkward_potion) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:brewing_stand[has_bottle_0=false,has_bottle_1=false,has_bottle_2=false]");
	auto* stand = f.level->getBlockEntity<BrewingStandBlockEntity>({0, Y, 0});
	CHECK(stand != nullptr);
	if (!stand) return;
	const PotionBrewing& brewing = f.level->potionBrewing();
	int					 water	 = f.data.getStaticId("minecraft:potion", "minecraft:water");
	int					 awkward = f.data.getStaticId("minecraft:potion", "minecraft:awkward");
	stand->setItem(0, brewing.createItemStack(f.item("minecraft:potion"), water));
	stand->setItem(3, stack(f, "minecraft:nether_wart", 2));
	stand->setItem(4, stack(f, "minecraft:blaze_powder", 1));
	CHECK(brewing.isIngredient(stand->item(3)));
	f.tick(1);
	CHECK(stand->item(4).isEmpty());
	CHECK_EQ(stand->fuel, 19);
	CHECK_EQ(stand->brewTime, 400);
	CHECK(f.data.getProperty(f.at(0, Y, 0), "has_bottle_0") == "true");
	CHECK(f.data.getProperty(f.at(0, Y, 0), "has_bottle_1") == "false");
	f.tick(399);
	CHECK_EQ(brewing.potionOf(stand->item(0)), water);
	f.tick(1);
	CHECK_EQ(brewing.potionOf(stand->item(0)), awkward);
	CHECK_EQ(stand->item(3).count, 1);
	// No mix for awkward + nether wart: nothing more
	f.tick(5);
	CHECK_EQ(stand->brewTime, 0);
	CHECK_EQ(stand->fuel, 19);
	// Gunpowder makes it a splash potion
	stand->setItem(3, stack(f, "minecraft:gunpowder", 1));
	f.tick(401);
	CHECK_EQ(stand->item(0).item, f.item("minecraft:splash_potion"));
	CHECK_EQ(brewing.potionOf(stand->item(0)), awkward);
}

// The brewing stand's menu: shift-click places fuel, ingredient and bottles (one per slot)
TEST(brewing_menu) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setPosition(0.5, Y, 1.5);
	f.set(0, Y, 0, "minecraft:brewing_stand[has_bottle_0=false,has_bottle_1=false,has_bottle_2=false]");
	int state = f.at(0, Y, 0);
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	Menu& menu = Menus::current(*player, *f.level);
	CHECK(menu.type() == "minecraft:brewing_stand");
	CHECK_EQ(int(menu.slots().size()), 41);
	auto* stand = f.level->getBlockEntity<BrewingStandBlockEntity>({0, Y, 0});
	player->inventory().set(PlayerInventory::HOTBAR, stack(f, "minecraft:blaze_powder", 3));
	player->inventory().set(PlayerInventory::HOTBAR + 1, stack(f, "minecraft:glass_bottle", 5));
	player->inventory().set(PlayerInventory::HOTBAR + 2, stack(f, "minecraft:nether_wart", 7));
	menu.clicked(32, 0, ClickType::QuickMove);
	CHECK_EQ(stand->item(4).count, 3);
	menu.clicked(33, 0, ClickType::QuickMove);
	CHECK_EQ(stand->item(0).count, 1);
	CHECK_EQ(stand->item(1).count, 1);
	CHECK_EQ(stand->item(2).count, 1);
	CHECK_EQ(player->inventory().get(PlayerInventory::HOTBAR + 1).count, 2);
	menu.clicked(34, 0, ClickType::QuickMove);
	CHECK_EQ(stand->item(3).count, 7);
	Menus::doCloseContainer(*player, *f.level);
}
