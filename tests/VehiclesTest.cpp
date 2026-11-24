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

namespace {
	void rail(LevelFixture& f, int x, int y, int z, const char* block = "minecraft:rail") {
		f.level->setBlock({x, y, z}, f.data.getDefaultBlockState(block), Level::UPDATE_ALL);
	}
	std::string shapeAt(LevelFixture& f, int x, int y, int z) { return f.data.getProperty(f.level->getBlockState({x, y, z}), "shape"); }

	Minecart* addMinecart(LevelFixture& f, const char* type, int x, int y, int z) {
		auto	  cart = std::make_unique<Minecart>(*f.level, f.data.getStaticId("minecraft:entity_type", type), Vec3{x + 0.5, y + Minecart::RAIL_HEIGHT, z + 0.5});
		Minecart* raw  = cart.get();
		f.level->entities().add(std::move(cart));
		return raw;
	}
} // namespace

// Rails connect: a corner curves, a rail next to one a block higher slopes up to it
TEST(rails_connect) {
	LevelFixture f(1);
	rail(f, 0, Y, 0);
	rail(f, 1, Y, 0);
	CHECK(shapeAt(f, 0, Y, 0) == "east_west");
	rail(f, 0, Y, 1);
	CHECK(shapeAt(f, 0, Y, 0) == "south_east");
	f.set(5, Y, 0, "minecraft:stone");
	rail(f, 5, Y + 1, 0);
	rail(f, 4, Y, 0);
	CHECK(shapeAt(f, 4, Y, 0) == "ascending_east");
}

// A pushed minecart rolls along a straight rail, staying on it, and slows down
TEST(minecart_rolls_on_rail) {
	LevelFixture f(1);
	for (int x = -10; x <= 10; x++) rail(f, x, Y, 0);
	Minecart* cart = addMinecart(f, "minecraft:minecart", -10, Y, 0);
	cart->setDeltaMovement({0.4, 0.0, 0.0});
	f.tick(20);
	CHECK(cart->position().x > -6.0);
	CHECK(std::abs(cart->position().z - 0.5) < 0.01);
	CHECK(std::abs(cart->position().y - (Y + Minecart::RAIL_HEIGHT)) < 0.01);
	f.tick(200);
	CHECK(cart->deltaMovement().horizontalDistanceSqr() < 0.0001); // Friction stopped it
}

// A powered rail (a redstone block next to it) speeds it up to its maximum
TEST(minecart_powered_rail) {
	LevelFixture f(1);
	for (int x = 0; x <= 20; x++) rail(f, x, Y, 0, x < 4 ? "minecraft:powered_rail" : "minecraft:rail");
	f.set(0, Y, 1, "minecraft:redstone_block");
	CHECK(f.data.getProperty(f.level->getBlockState({3, Y, 0}), "powered") == "true"); // Through the line
	Minecart* cart = addMinecart(f, "minecraft:minecart", 1, Y, 0);
	cart->setDeltaMovement({0.05, 0.0, 0.0});
	f.tick(10);
	CHECK(cart->deltaMovement().x > 0.3);
}

// Its rider pushes it with the forward key; an activator rail throws the rider out
TEST(minecart_rider_and_activator) {
	LevelFixture f(1);
	for (int x = 0; x <= 12; x++) rail(f, x, Y, 0, x == 10 ? "minecraft:activator_rail" : "minecraft:rail");
	f.set(10, Y, 1, "minecraft:redstone_block");
	Minecart* cart	 = addMinecart(f, "minecraft:minecart", 1, Y, 0);
	auto	  player = addPlayer(f, 1.5, Y, 1.5);
	CHECK(cart->interact(*player, 0));
	CHECK_EQ(player->vehicleId(), cart->id());
	player->setRotation(-90.0F, 0.0F); // Facing +x
	player->setForwardKeyDown(true);
	bool ejected = false;
	for (int t = 0; t < 200 && !ejected; t++) {
		f.tick();
		ejected = player->vehicleId() == -1;
	}
	CHECK(ejected);
	CHECK(cart->position().x > 9.0);
}

// A TNT minecart primed by a powered activator rail explodes 4 seconds later
TEST(tnt_minecart_explodes) {
	LevelFixture f(1);
	rail(f, 0, Y, 0, "minecraft:activator_rail");
	f.set(0, Y, 1, "minecraft:redstone_block");
	Minecart* cart = addMinecart(f, "minecraft:tnt_minecart", 0, Y, 0);
	f.tick(2);
	CHECK(cart->isPrimed());
	int id = cart->id();
	f.tick(90);
	CHECK(f.level->actorById(id) == nullptr);
}

// A detector rail gives a signal while a minecart is on it; a hopper minecart picks up the items it passes
TEST(detector_rail_and_hopper_minecart) {
	LevelFixture f(1);
	rail(f, 0, Y, 0, "minecraft:detector_rail");
	Minecart* hopper = addMinecart(f, "minecraft:hopper_minecart", 0, Y, 0);
	f.level->entities().add(std::make_unique<ItemEntity>(*f.level, Vec3{0.5, Y + 0.3, 0.5}, ItemStack(f.item("minecraft:iron_ingot"), 4), Vec3{}));
	f.tick(10);
	CHECK(f.data.getProperty(f.level->getBlockState({0, Y, 0}), "powered") == "true");
	CHECK(f.items().empty());
	hopper->hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, 100.0F); // Broken: its items drop
	f.tick(30);
	CHECK(f.data.getProperty(f.level->getBlockState({0, Y, 0}), "powered") == "false");
	int ingots = 0;
	for (ItemEntity* item : f.items()) {
		if (item->item().item == f.item("minecraft:iron_ingot")) ingots += item->item().count;
	}
	CHECK_EQ(ingots, 4);
}

#include "world/entity/LeashKnot.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/mobs/Camelids.hpp"
#include "world/entity/mobs/Horses.hpp"
#include "world/inventory/HorseMenu.hpp"

namespace {
	Mob* spawnMob(LevelFixture& f, const char* type, const BlockPos& pos) {
		return f.level->mobs().spawn(*f.level, f.data.getStaticId("minecraft:entity_type", type), pos, MobRegistry::SpawnReason::Command);
	}
} // namespace

// A tamed horse's inventory: sneak-click opens it, a saddle goes in its saddle slot (and on the horse)
TEST(horse_inventory_saddle) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 2.5, Y, 0.5);
	auto* horse	 = dynamic_cast<Horse*>(spawnMob(f, "minecraft:horse", {0, Y, 0}));
	CHECK(horse);
	if (!horse) return;
	horse->setBaby(false);
	horse->setTamed(true);
	player->setShiftKeyDown(true);
	CHECK(horse->mobInteract(*player, 0));
	auto* menu = dynamic_cast<HorseMenu*>(player->openMenuSlot().get());
	CHECK(menu);
	if (!menu) return;
	ItemStack saddle(f.item("minecraft:saddle"), 1);
	CHECK(menu->slots()[0].mayPlace(saddle, f.data));
	CHECK(!menu->slots()[1].mayPlace(saddle, f.data));
	CHECK(menu->slots()[1].mayPlace(ItemStack(f.item("minecraft:iron_horse_armor"), 1), f.data));
	menu->slots()[0].set(saddle);
	CHECK(horse->isSaddled());
}

// A donkey takes a chest; its 15 slots keep their items through a save and drop on death; a llama wears a carpet
TEST(donkey_chest_and_llama_carpet) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 2.5, Y, 0.5);
	auto* donkey = dynamic_cast<AbstractChestedHorse*>(spawnMob(f, "minecraft:donkey", {0, Y, 0}));
	CHECK(donkey);
	if (!donkey) return;
	donkey->setBaby(false);
	donkey->setTamed(true);
	player->setSelectedSlot(0);
	player->inventory().set(player->handSlot(0), ItemStack(f.item("minecraft:chest"), 1));
	CHECK(donkey->mobInteract(*player, 0));
	CHECK(donkey->hasChest());
	CHECK_EQ(donkey->inventoryColumns(), 5);
	donkey->chestContainer()->item(4) = ItemStack(f.item("minecraft:gold_ingot"), 7);
	Buffer saved;
	donkey->save(saved);
	auto copy = f.level->mobs().create(*f.level, donkey->typeId());
	Buffer in(saved.getData());
	copy->load(in);
	auto* loaded = dynamic_cast<AbstractChestedHorse*>(copy.get());
	CHECK(loaded && loaded->hasChest() && loaded->chestContainer()->item(4).count == 7);
	donkey->hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, 1000.0F);
	f.tick(25);
	int gold = 0, chests = 0;
	for (ItemEntity* item : f.items()) {
		if (item->item().item == f.item("minecraft:gold_ingot")) gold += item->item().count;
		if (item->item().item == f.item("minecraft:chest")) chests++;
	}
	CHECK_EQ(gold, 7);
	CHECK_EQ(chests, 1);
	auto* llama = dynamic_cast<Llama*>(spawnMob(f, "minecraft:llama", {6, Y, 6}));
	CHECK(llama && llama->canWearBody(ItemStack(f.item("minecraft:red_carpet"), 1)) && !llama->canUseSaddle());
}

// A lead: the mob follows when pulled, the lead breaks past 10 blocks (dropping it); tied to a fence it stays there
TEST(leads_and_fence_knots) {
	LevelFixture f(1);
	auto player = addPlayer(f, 0.5, Y, 2.5);
	Mob* cow	= spawnMob(f, "minecraft:cow", {0, Y, 0});
	CHECK(cow);
	if (!cow) return;
	player->setSelectedSlot(0);
	player->inventory().set(player->handSlot(0), ItemStack(f.item("minecraft:lead"), 2));
	CHECK(cow->handleLeashInteraction(*player, 0));
	CHECK(cow->leashHolder() == player.get());
	player->setPosition(8.5, Y, 0.5);
	f.tick(60);
	CHECK(cow->isLeashed());
	CHECK(cow->position().x > 2.0); // Pulled along
	// Tied to a fence
	f.level->setBlock({cow->blockPosition().x, Y, 3}, f.data.getDefaultBlockState("minecraft:oak_fence"), Level::UPDATE_ALL);
	BlockPos fence{cow->blockPosition().x, Y, 3};
	player->setPosition(fence.x + 0.5, Y, 5.5);
	CHECK_EQ(LeashKnot::bindPlayerMobs(*f.level, *player, fence), 1);
	CHECK(dynamic_cast<LeashKnot*>(cow->leashHolder()) != nullptr);
	// Untied, then pulled too far: the lead breaks and drops
	cow->dropLeash(false);
	CHECK(cow->handleLeashInteraction(*player, 0));
	player->setPosition(cow->position().x + 30.0, Y, cow->position().z);
	f.tick(5);
	CHECK(!cow->isLeashed());
	bool lead = false;
	for (ItemEntity* item : f.items()) lead |= item->item().item == f.item("minecraft:lead");
	CHECK(lead);
}
