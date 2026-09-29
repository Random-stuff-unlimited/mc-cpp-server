#include "LevelFixture.hpp"

#include <cstdio>
#include "Test.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/mobs/Creeper.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/entity/mobs/Spider.hpp"
#include "world/entity/mobs/Zombie.hpp"

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	Mob* spawn(LevelFixture& f, const char* name, const BlockPos& pos) {
		f.world->setDayTime(18000); // Night: monsters hunt, nothing burns
		return f.level->mobs().spawn(*f.level, f.data.getStaticId("minecraft:entity_type", name), pos, MobRegistry::SpawnReason::Command);
	}

	std::shared_ptr<Player> addPlayer(LevelFixture& f, double x, double z) {
		auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		player->setPosition(x, Y, z);
		player->setOnGround(true);
		player->setGameMode(GameMode::Survival);
		f.addPlayer(player);
		return player;
	}
} // namespace

// The vanilla classes are made for their types
TEST(mob_ai_classes_by_type) {
	LevelFixture f(1);
	CHECK(dynamic_cast<Zombie*>(spawn(f, "minecraft:zombie", {0, Y, 0})) != nullptr);
	CHECK(dynamic_cast<Husk*>(spawn(f, "minecraft:husk", {1, Y, 0})) != nullptr);
	CHECK(dynamic_cast<Creeper*>(spawn(f, "minecraft:creeper", {2, Y, 0})) != nullptr);
	CHECK(dynamic_cast<CaveSpider*>(spawn(f, "minecraft:cave_spider", {3, Y, 0})) != nullptr);
	CHECK(dynamic_cast<Monster*>(spawn(f, "minecraft:skeleton", {4, Y, 0})) != nullptr);
}

// A zombie notices a player 10 blocks away, walks up to it and hits it
TEST(zombie_hunts_player) {
	LevelFixture f(1);
	auto player = addPlayer(f, 10.5, 0.5);
	Mob* zombie = spawn(f, "minecraft:zombie", {0, Y, 0});
	CHECK(zombie);
	if (!zombie) return;
	zombie->setItemSlot(EquipmentSlot::Head, ItemStack{f.data.getStaticId("minecraft:item", "minecraft:diamond_helmet"), 1}); // No sun burn
	float health = player->combat().health;
	bool  targeted = false;
	for (int t = 0; t < 400 && player->combat().health == health; t++) {
		f.tick();
		targeted |= zombie->getTarget() == player.get();
	}
	CHECK(targeted);
	CHECK(player->combat().health < health);
	CHECK(zombie->distanceToSqr(*player) < 4.0);
}

// A creeper walks to the player, swells, and explodes: it's gone, the player is hurt, the ground has a crater
TEST(creeper_explodes_near_player) {
	LevelFixture f(1);
	auto player = addPlayer(f, 6.5, 0.5);
	auto* creeper = dynamic_cast<Creeper*>(spawn(f, "minecraft:creeper", {0, Y, 0}));
	CHECK(creeper);
	if (!creeper) return;
	float health  = player->combat().health;
	bool  swelled = false;
	for (int t = 0; t < 400 && !creeper->isRemoved(); t++) {
		f.tick();
		swelled |= creeper->swell() > 0;
	}
	CHECK(swelled);
	CHECK(creeper->isRemoved());
	CHECK(player->combat().health < health);
}

// Spiders climb walls in their way
TEST(spider_climbs_wall) {
	LevelFixture f(1);
	auto* spider = dynamic_cast<Spider*>(spawn(f, "minecraft:spider", {0, Y, 0}));
	CHECK(spider);
	if (!spider) return;
	for (int y = Y; y < Y + 4; y++) f.set(2, y, 0, "minecraft:stone");
	spider->navigation().moveTo(3.5, Y + 4, 0.5, 1.0);
	double highest = 0;
	for (int t = 0; t < 200; t++) {
		f.tick();
		highest = std::max(highest, spider->position().y);
	}
	CHECK(highest >= Y + 3.0);
}

// Natural spawning: at night, monsters appear around a player, 24 blocks away at least
TEST(natural_spawning_at_night) {
	LevelFixture f(4);
	auto player = addPlayer(f, 0.5, 0.5);
	player->setGameMode(GameMode::Creative); // Not hunted: they stay where they spawned (give or take a few steps)
	f.world->setDayTime(18000);
	f.level->setMobSpawning(true);
	f.tick(20);
	int monsters = 0, tooClose = 0;
	f.level->entities().forEach([&](Entity& entity) {
		auto* mob = dynamic_cast<Mob*>(&entity);
		if (!mob || mob->isRemoved()) return;
		if (mob->type().category == GameData::MobCategory::Monster) monsters++;
		if (mob->distanceToSqr(*player) < 22.0 * 22.0) tooClose++;
	});
	CHECK(monsters > 0);
	CHECK_EQ(tooClose, 0);
}

// The monster cap: 70 per player (a tick's last packs may go a few over, as in vanilla)
TEST(natural_spawning_monster_cap) {
	LevelFixture f(4);
	auto player = addPlayer(f, 0.5, 0.5);
	player->setGameMode(GameMode::Creative);
	f.world->setDayTime(18000);
	f.level->setMobSpawning(true);
	f.tick(300);
	int monsters = 0;
	f.level->entities().forEach([&](Entity& entity) {
		auto* mob = dynamic_cast<Mob*>(&entity);
		if (mob && !mob->isRemoved() && mob->type().category == GameData::MobCategory::Monster) monsters++;
	});
	CHECK(monsters >= 40);
	CHECK(monsters <= 70 + 3);
}


// A player that dies drops its whole inventory (hotbar, main, armor, offhand) around it: nothing stays in it
TEST(player_death_drops_inventory) {
	LevelFixture f(1);
	auto player = addPlayer(f, 0.5, 0.5);
	auto item	= [&](const char* name, int count) { return ItemStack{f.data.getStaticId("minecraft:item", name), count}; };
	player->inventory().set(PlayerInventory::HOTBAR, item("minecraft:stone", 64));
	player->inventory().set(20, item("minecraft:dirt", 10));
	player->inventory().set(5, item("minecraft:iron_helmet", 1));
	player->inventory().set(PlayerInventory::OFFHAND, item("minecraft:torch", 3));
	Combat::damage(f.server, *player, 1000.0F, {"minecraft:generic_kill", nullptr, nullptr, std::nullopt});
	CHECK(player->combat().dead);
	int left = 0;
	for (int slot = 0; slot < PlayerInventory::SIZE; slot++) left += !player->inventory().get(slot).isEmpty();
	CHECK_EQ(left, 0);
	int dropped = 0;
	f.level->entities().forEach([&](Entity& entity) {
		if (auto* dropped_item = dynamic_cast<ItemEntity*>(&entity)) dropped += dropped_item->item().count;
	});
	CHECK_EQ(dropped, 64 + 10 + 1 + 3);
}

namespace {
	ItemStack itemOf(LevelFixture& f, const char* name, int count = 1) { return ItemStack{f.data.getStaticId("minecraft:item", name), count}; }
	std::vector<Mob*> mobsOfType(LevelFixture& f, const char* name) {
		int				  typeId = f.data.getStaticId("minecraft:entity_type", name);
		std::vector<Mob*> found;
		f.level->entities().forEach([&](Entity& entity) {
			auto* mob = dynamic_cast<Mob*>(&entity);
			if (mob && !mob->isRemoved() && mob->typeId() == typeId) found.push_back(mob);
		});
		return found;
	}
	int itemsOnGround(LevelFixture& f, const char* name) {
		int id = f.data.getStaticId("minecraft:item", name), count = 0;
		f.level->entities().forEach([&](Entity& entity) {
			auto* item = dynamic_cast<ItemEntity*>(&entity);
			if (item && !item->isRemoved() && item->item().item == id) count += item->item().count;
		});
		return count;
	}
} // namespace

// Two cows fed wheat fall in love, meet and make a calf; the parents wait 5 minutes
TEST(animals_breed) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 8.5, 8.5);
	auto* a		 = dynamic_cast<Cow*>(spawn(f, "minecraft:cow", {0, Y, 0}));
	auto* b		 = dynamic_cast<Cow*>(spawn(f, "minecraft:cow", {3, Y, 0}));
	CHECK(a && b);
	if (!a || !b) return;
	a->setBaby(false);
	b->setBaby(false);
	player->setGameMode(GameMode::Survival);
	player->inventory().set(player->handSlot(0), itemOf(f, "minecraft:wheat", 2));
	CHECK(a->mobInteract(*player, 0));
	CHECK(b->mobInteract(*player, 0));
	CHECK(a->isInLove() && b->isInLove());
	CHECK(player->inventory().get(player->handSlot(0)).isEmpty());
	for (int t = 0; t < 400 && mobsOfType(f, "minecraft:cow").size() < 3; t++) f.tick();
	std::vector<Mob*> cows = mobsOfType(f, "minecraft:cow");
	CHECK_EQ(cows.size(), size_t(3));
	int babies = 0;
	for (Mob* cow : cows) babies += cow->isBaby();
	CHECK_EQ(babies, 1);
	CHECK(!a->isInLove() && a->getAge() > 5000 && b->getAge() > 5000);
}

// A cow gives milk to a bucket; a sheep is shorn (1 to 3 wool of its color) and regrows it by eating grass
TEST(animals_milk_and_shear) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 3.5, 3.5);
	player->setGameMode(GameMode::Survival);
	auto* cow = dynamic_cast<Cow*>(spawn(f, "minecraft:cow", {0, Y, 0}));
	CHECK(cow);
	if (!cow) return;
	cow->setBaby(false);
	player->inventory().set(player->handSlot(0), itemOf(f, "minecraft:bucket"));
	CHECK(cow->mobInteract(*player, 0));
	CHECK_EQ(player->inventory().get(player->handSlot(0)).item, f.data.getStaticId("minecraft:item", "minecraft:milk_bucket"));

	auto* sheep = dynamic_cast<Sheep*>(spawn(f, "minecraft:sheep", {6, Y, 0}));
	CHECK(sheep);
	if (!sheep) return;
	sheep->setBaby(false);
	sheep->setColor(14); // Red
	player->inventory().set(player->handSlot(0), itemOf(f, "minecraft:shears"));
	CHECK(sheep->mobInteract(*player, 0));
	CHECK(sheep->isSheared());
	int wool = itemsOnGround(f, "minecraft:red_wool");
	CHECK(wool >= 1 && wool <= 3);
	CHECK(!sheep->mobInteract(*player, 0)); // Nothing left to shear
	sheep->ate();
	CHECK(!sheep->isSheared());
}

// A chicken lays its egg when its time comes
TEST(animals_chicken_lays_egg) {
	LevelFixture f(1);
	auto* chicken = dynamic_cast<Chicken*>(spawn(f, "minecraft:chicken", {0, Y, 0}));
	CHECK(chicken);
	if (!chicken) return;
	chicken->setBaby(false);
	chicken->setEggTime(2);
	f.tick(5);
	CHECK_EQ(itemsOnGround(f, "minecraft:egg"), 1);
	CHECK(chicken->eggTime() >= 6000 - 5);
}

// Holding wheat, a player tempts a cow to follow it
TEST(animals_tempted_by_food) {
	LevelFixture f(1);
	auto player = addPlayer(f, 8.5, 0.5);
	player->setGameMode(GameMode::Survival);
	player->inventory().set(player->handSlot(0), itemOf(f, "minecraft:wheat"));
	Mob* cow = spawn(f, "minecraft:cow", {0, Y, 0});
	CHECK(cow);
	if (!cow) return;
	cow->setBaby(false);
	double start = cow->distanceToSqr(*player);
	f.tick(200);
	CHECK(cow->distanceToSqr(*player) < start);
	CHECK(cow->distanceToSqr(*player) < 5.0 * 5.0);
}
