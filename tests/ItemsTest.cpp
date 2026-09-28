#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"

#include <cmath>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // Top of the grass, first air layer

	std::string describe(LevelFixture& f, const ItemEntity& item) {
		return f.data.getStaticName("minecraft:item", item.item().item) + " x" + std::to_string(item.item().count);
	}
} // namespace

// Each block drops what its loot table says (here without a tool: the tool check is the breaker's job)
TEST(items_blocks_drop_their_loot) {
	LevelFixture f;
	f.level->dropResources(f.state("minecraft:stone"), {0, Y, 0});
	f.level->dropResources(f.state("minecraft:grass_block[snowy=false]"), {2, Y, 0});
	f.level->dropResources(f.state("minecraft:poppy"), {4, Y, 0});
	f.level->dropResources(f.state("minecraft:tall_grass[half=lower]"), {6, Y, 0}); // Only with shears
	auto items = f.items();
	CHECK_EQ(items.size(), size_t(3));
	if (items.size() == 3) {
		CHECK(describe(f, *items[0]) == "minecraft:cobblestone x1");
		CHECK(describe(f, *items[1]) == "minecraft:dirt x1");
		CHECK(describe(f, *items[2]) == "minecraft:poppy x1");
		CHECK_EQ(items[0]->pickupDelay(), 10);
	}
}

// Popped items jump, fall and rest on the ground, exactly on top of it
TEST(items_fall_and_rest_on_ground) {
	LevelFixture f;
	// Straight up, like a popped item without its random sideways push
	f.level->entities().add(std::make_unique<ItemEntity>(*f.level, Vec3{0.5, Y + 3.0, 0.5}, ItemStack(f.item("minecraft:cobblestone"), 1), Vec3{0, 0.2, 0}));
	f.tick(60);
	auto items = f.items();
	CHECK_EQ(items.size(), size_t(1));
	if (items.empty()) return;
	CHECK(std::abs(items[0]->position().y - Y) < 1.0E-9);
	CHECK(items[0]->onGround());
	CHECK(items[0]->position().x == 0.5);
}

// Items of the same kind next to each other merge
TEST(items_merge) {
	LevelFixture f;
	auto add = [&](const char* name, int count, double x) {
		f.level->entities().add(std::make_unique<ItemEntity>(*f.level, Vec3{x, Y, 0.5}, ItemStack(f.item(name), count), Vec3{}));
	};
	add("minecraft:cobblestone", 3, 0.5);
	add("minecraft:cobblestone", 5, 0.9); // Within half a block: they merge
	add("minecraft:dirt", 1, 0.7);		  // Another item: stays apart
	add("minecraft:cobblestone", 2, 5.5); // Too far
	f.tick(80);
	auto items = f.items();
	CHECK_EQ(items.size(), size_t(3));
	bool merged = false;
	for (ItemEntity* item : items) merged = merged || describe(f, *item) == "minecraft:cobblestone x8";
	CHECK(merged);
}

// Flowing water carries items away from its source
TEST(items_carried_by_water) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:water[level=0]");
	f.tick(60);
	f.level->entities().add(std::make_unique<ItemEntity>(*f.level, Vec3{2.5, Y, 0.5}, ItemStack(f.item("minecraft:dirt"), 1), Vec3{}));
	f.tick(40);
	auto items = f.items();
	CHECK_EQ(items.size(), size_t(1));
	if (!items.empty()) CHECK(items[0]->position().x > 3.0);
}

// Water breaking a flower drops it; lava burns items
TEST(items_water_drops_flowers_lava_burns) {
	LevelFixture f;
	f.set(1, Y, 0, "minecraft:poppy");
	f.set(0, Y, 0, "minecraft:water[level=0]");
	f.tick(5);
	auto items = f.items();
	CHECK_EQ(items.size(), size_t(1));
	if (!items.empty()) CHECK(describe(f, *items[0]) == "minecraft:poppy x1");

	LevelFixture l;
	l.set(0, Y, 10, "minecraft:lava[level=0]");
	l.level->entities().add(std::make_unique<ItemEntity>(*l.level, Vec3{0.5, Y + 0.2, 10.5}, ItemStack(l.item("minecraft:dirt"), 1), Vec3{}));
	l.level->entities().add(std::make_unique<ItemEntity>(*l.level, Vec3{0.5, Y + 0.2, 10.5}, ItemStack(l.item("minecraft:netherite_ingot"), 1), Vec3{}));
	l.tick(5);
	auto left = l.items();
	CHECK_EQ(left.size(), size_t(1)); // Netherite survives lava
	if (!left.empty()) CHECK(describe(l, *left[0]) == "minecraft:netherite_ingot x1");
}

// Items disappear after 5 minutes (6000 ticks)
TEST(items_despawn) {
	LevelFixture f(1);
	f.level->popResource({0, Y, 0}, ItemStack(f.item("minecraft:dirt"), 1));
	f.tick(5999);
	CHECK_EQ(f.items().size(), size_t(1));
	f.tick(1);
	CHECK_EQ(f.items().size(), size_t(0));
}

// Inventory.add: fills stacks of the same item, then the first free slots, hotbar first
TEST(items_inventory_add) {
	GameData data;
	data.load("resources/gamedata");
	int				cobblestone = data.getStaticId("minecraft:item", "minecraft:cobblestone");
	PlayerInventory inventory;
	inventory.set(PlayerInventory::HOTBAR + 2, ItemStack(cobblestone, 60));
	ItemStack stack(cobblestone, 70);
	CHECK(inventory.add(stack, 0, false, data));
	CHECK(stack.isEmpty());
	CHECK_EQ(inventory.get(PlayerInventory::HOTBAR + 2).count, 64); // Existing stack first
	CHECK_EQ(inventory.get(PlayerInventory::HOTBAR + 0).count, 64); // Then the first free slot
	CHECK_EQ(inventory.get(PlayerInventory::HOTBAR + 1).count, 2);

	// Full inventory: nothing taken, unless creative
	PlayerInventory full;
	for (int slot = 9; slot <= 44; slot++) full.set(slot, ItemStack(data.getStaticId("minecraft:item", "minecraft:stick"), 64));
	ItemStack more(cobblestone, 5);
	CHECK(!full.add(more, 0, false, data));
	CHECK_EQ(more.count, 5);
	CHECK(full.add(more, 0, true, data));
	CHECK(more.isEmpty());
}

// A player picks up an item once its pickup delay is over
TEST(items_pickup) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setPosition(0.5, Y, 0.5);
	f.level->popResource({0, Y, 0}, ItemStack(f.item("minecraft:dirt"), 3));
	ItemEntity* item = f.items().at(0);
	CHECK_EQ(item->playerTouch(*player), 0); // Delay not over
	f.tick(10);
	CHECK_EQ(item->playerTouch(*player), 3);
	CHECK(item->isRemoved());
	CHECK_EQ(player->inventory().get(PlayerInventory::HOTBAR).count, 3);
}
