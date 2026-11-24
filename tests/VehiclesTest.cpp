#include "LevelFixture.hpp"
#include "Test.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/Vehicles.hpp"
#include "world/inventory/Menu.hpp"

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	std::shared_ptr<Player> addPlayer(LevelFixture& f, double x, double y, double z) {
		auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		player->setPosition(x, y, z);
		player->setOnGround(true);
		player->setGameMode(GameMode::Survival);
		f.addPlayer(player);
		return player;
	}

	// A pool of still water, 9 by 9 and 3 deep, dug into the ground around (0, 0)
	void digPool(LevelFixture& f) {
		int water = f.data.getDefaultBlockState("minecraft:water");
		for (int x = -4; x <= 4; x++) {
			for (int z = -4; z <= 4; z++) {
				for (int y = Y - 3; y < Y; y++) f.level->setBlock({x, y, z}, water, Level::UPDATE_ALL);
			}
		}
	}

	Boat* addBoat(LevelFixture& f, const char* type, const Vec3& at) {
		auto  boat = std::make_unique<Boat>(*f.level, f.data.getStaticId("minecraft:entity_type", type), at, 0.0F);
		Boat* raw  = boat.get();
		f.level->entities().add(std::move(boat));
		return raw;
	}
} // namespace

// A boat floats at the surface of the water, and rests on the ground
TEST(boat_floats_and_rests) {
	LevelFixture f(1);
	digPool(f);
	Boat* floating = addBoat(f, "minecraft:oak_boat", {0.5, Y - 2.0, 0.5}); // Let go under water: it comes up
	Boat* onLand   = addBoat(f, "minecraft:oak_boat", {8.5, Y + 2.0, 8.5}); // Dropped above the grass: it lands
	f.tick(100);
	CHECK(floating->position().y > Y - 1.3 && floating->position().y < Y + 0.2);
	CHECK(onLand->position().y >= Y - 0.01 && onLand->position().y < Y + 0.1);
}

// A player gets in with a click; hits break it into its item
TEST(boat_ride_and_break) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 2.5, Y, 0.5);
	Boat* boat	 = addBoat(f, "minecraft:birch_boat", {0.5, static_cast<double>(Y), 0.5});
	f.tick();
	CHECK(boat->interact(*player, 0));
	CHECK_EQ(player->vehicleId(), boat->id());
	CHECK(boat->isControlledByRider());
	for (int i = 0; i < 5 && !boat->isRemoved(); i++) boat->hurtServer({"minecraft:player_attack", player.get(), nullptr, std::nullopt}, 1.0F);
	CHECK(boat->isRemoved());
	CHECK_EQ(player->vehicleId(), -1);
	f.tick();
	bool dropped = false;
	for (ItemEntity* item : f.items()) dropped |= item->item().item == f.item("minecraft:birch_boat");
	for (int slot = 9; slot <= 44; slot++) dropped |= player->inventory().get(slot).item == f.item("minecraft:birch_boat"); // Picked up at once
	CHECK(dropped);
}

// The boat item puts a boat on the water the player looks at
TEST(boat_placed_from_item) {
	LevelFixture f(1);
	digPool(f);
	auto player = addPlayer(f, 0.5, Y + 1.0, 0.5);
	player->setRotation(0.0F, 90.0F); // Looking straight down
	ItemStack stack(f.item("minecraft:oak_boat"), 1);
	CHECK(Boat::place(*f.level, *player, stack));
	CHECK(stack.isEmpty());
	int boats = 0;
	f.level->entities().forEach([&](Entity& entity) { boats += dynamic_cast<Boat*>(&entity) != nullptr; });
	CHECK_EQ(boats, 1);
}

// A chest boat keeps its items when saved and loaded, and drops them when it breaks
TEST(chest_boat_items) {
	LevelFixture f(1);
	Boat* boat = addBoat(f, "minecraft:oak_chest_boat", {0.5, static_cast<double>(Y), 0.5});
	CHECK(boat->hasChest());
	auto player = addPlayer(f, 2.5, Y, 0.5);
	player->setShiftKeyDown(true);
	CHECK(boat->interact(*player, 0)); // Its chest opens
	auto* menu = player->openMenuSlot().get();
	CHECK(menu && menu->type() == "minecraft:generic_9x3");
	if (!menu) return;
	menu->slots()[0].set(ItemStack(f.item("minecraft:diamond"), 3));
	Buffer saved;
	boat->save(saved);
	Boat copy(*f.level, boat->typeId());
	Buffer in(saved.getData());
	copy.load(in);
	Menus::closeContainer(*player, *f.level);
	boat->hurtServer({"minecraft:player_attack", player.get(), nullptr, std::nullopt}, 10.0F);
	f.tick();
	int diamonds = 0, boats = 0;
	for (ItemEntity* item : f.items()) {
		if (item->item().item == f.item("minecraft:diamond")) diamonds += item->item().count;
		if (item->item().item == f.item("minecraft:oak_chest_boat")) boats++;
	}
	CHECK_EQ(diamonds, 3);
	CHECK_EQ(boats, 1);
}
