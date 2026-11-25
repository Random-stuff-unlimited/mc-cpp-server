#include "LevelFixture.hpp"
#include "world/blocks/Composter.hpp"
#include "world/entity/mobs/FlyingAnimals.hpp"
#include "world/blockentity/EndGatewayBlockEntity.hpp"
#include "world/Portals.hpp"

#include <cstdio>
#include "Test.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/mobs/Amphibians.hpp"
#include "network/buffer.hpp"
#include "world/entity/mobs/Bosses.hpp"
#include "world/inventory/MerchantMenu.hpp"
#include "world/blocks/Eggs.hpp"
#include "world/blockentity/BeehiveBlockEntity.hpp"
#include "world/EndDragonFight.hpp"
#include "world/VillageEvents.hpp"
#include "world/entity/mobs/Illagers.hpp"
#include "world/entity/mobs/IronGolem.hpp"
#include "world/entity/mobs/HelperMobs.hpp"
#include "world/entity/EndCrystal.hpp"
#include "world/entity/mobs/Camelids.hpp"
#include "world/entity/mobs/Slime.hpp"
#include "world/entity/mobs/Creeper.hpp"
#include "world/entity/mobs/Critters.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/entity/mobs/FlyingAnimals.hpp"
#include "world/entity/mobs/HelperMobs.hpp"
#include "world/blockentity/BlockEntity.hpp"
#include "world/entity/mobs/Skeleton.hpp"
#include "world/entity/mobs/Piglins.hpp"
#include "world/entity/mobs/Shulker.hpp"
#include "world/entity/mobs/Spider.hpp"
#include "world/entity/mobs/Villagers.hpp"
#include "world/entity/mobs/Warden.hpp"
#include "world/entity/mobs/TrialMobs.hpp"
#include "world/entity/mobs/Wolf.hpp"
#include "world/entity/Arrow.hpp"
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

namespace {
	// A superflat (the default layers) of one biome
	nlohmann::json flatOf(const char* biome, const char* top = "minecraft:grass_block") {
		return {{"type", "flat"},
				{"biome", biome},
				{"layers",
				 {{{"block", "minecraft:bedrock"}, {"height", 1}},
				  {{"block", "minecraft:dirt"}, {"height", 2}},
				  {{"block", top}, {"height", 1}}}}};
	}

	// The mobs alive in the level, by entity type name
	std::map<std::string, int> mobsByType(LevelFixture& f) {
		std::map<std::string, int> mobs;
		f.level->entities().forEach([&](Entity& entity) {
			auto* mob = dynamic_cast<Mob*>(&entity);
			if (mob && !mob->isRemoved()) mobs[f.data.getStaticName("minecraft:entity_type", mob->typeId())]++;
		});
		return mobs;
	}
} // namespace

// A dimension's spawn cap (config.json "spawn-limits"): 10 monsters at most, none at all once set to 0
TEST(natural_spawning_configured_cap) {
	LevelFixture f(4);
	auto player = addPlayer(f, 0.5, 0.5);
	player->setGameMode(GameMode::Creative);
	f.world->setDayTime(18000);
	f.level->setMobSpawning(true);
	CHECK_EQ(f.level->spawnCap(GameData::MobCategory::Monster), 70);
	CHECK(Level::mobCategoryNamed("water_ambient") == GameData::MobCategory::WaterAmbient);
	CHECK(!Level::mobCategoryNamed("misc"));
	f.level->setSpawnCap(GameData::MobCategory::Monster, 10);
	f.tick(300);
	int monsters = 0;
	for (const auto& [type, count] : mobsByType(f)) {
		if (f.data.getEntityType(f.data.getStaticId("minecraft:entity_type", type))->category == GameData::MobCategory::Monster) monsters += count;
	}
	CHECK(monsters >= 5);
	CHECK(monsters <= 10 + 3);
	LevelFixture g(4);
	auto other = addPlayer(g, 0.5, 0.5);
	other->setGameMode(GameMode::Creative);
	g.world->setDayTime(18000);
	g.level->setMobSpawning(true);
	g.level->setSpawnCap(GameData::MobCategory::Monster, 0);
	g.tick(100);
	CHECK(mobsByType(g).empty());
}

// The biome picks the mobs: deserts mostly spawn husks, mushroom fields no monster at all (only mooshrooms)
TEST(natural_spawning_by_biome) {
	LevelFixture desert(4, "minecraft:overworld", flatOf("minecraft:desert"));
	auto		 player = addPlayer(desert, 0.5, 0.5);
	player->setGameMode(GameMode::Creative);
	desert.world->setDayTime(18000);
	desert.level->setMobSpawning(true);
	desert.tick(200);
	auto mobs = mobsByType(desert);
	CHECK(mobs["minecraft:husk"] > 0);
	CHECK(mobs["minecraft:husk"] > mobs["minecraft:zombie"]);
	CHECK_EQ(mobs["minecraft:cow"], 0);

	LevelFixture mushroom(4, "minecraft:overworld", flatOf("minecraft:mushroom_fields", "minecraft:mycelium"));
	auto other = addPlayer(mushroom, 0.5, 0.5);
	other->setGameMode(GameMode::Creative);
	mushroom.world->setDayTime(18000);
	mushroom.level->setMobSpawning(true);
	mushroom.tick(401); // A creature round (every 400 ticks)
	auto spawned = mobsByType(mushroom);
	CHECK(spawned["minecraft:mooshroom"] > 0);
	for (const auto& [type, count] : spawned) CHECK(type == "minecraft:mooshroom");
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

// A skeleton draws its bow at a player in range and shoots: the arrow hurts the player (about 2 to 5 damage)
TEST(skeleton_shoots_player) {
	LevelFixture f(2);
	auto player = addPlayer(f, 10.5, 0.5);
	Mob* skeleton = spawn(f, "minecraft:skeleton", {0, Y, 0});
	CHECK(skeleton);
	if (!skeleton) return;
	CHECK(skeleton->isHolding("minecraft:bow"));
	float health = player->combat().health;
	bool  drew	 = false;
	for (int t = 0; t < 400 && player->combat().health == health; t++) {
		f.tick();
		drew |= skeleton->isUsingItem();
	}
	CHECK(drew);
	CHECK(player->combat().health < health);
	CHECK(health - player->combat().health <= 6.0F);
}

// Without a bow a skeleton fights in melee; given one back, it shoots again
TEST(skeleton_reassesses_weapon) {
	LevelFixture f(1);
	auto  player   = addPlayer(f, 6.5, 0.5);
	auto* skeleton = dynamic_cast<AbstractSkeleton*>(spawn(f, "minecraft:skeleton", {0, Y, 0}));
	CHECK(skeleton);
	if (!skeleton) return;
	skeleton->setItemSlot(EquipmentSlot::MainHand, ItemStack{});
	float health = player->combat().health;
	for (int t = 0; t < 400 && player->combat().health == health; t++) f.tick();
	CHECK(player->combat().health < health);
	CHECK(skeleton->distanceToSqr(*player) < 4.0); // Came to hit it
}

// An arrow flies, falls and sticks in the ground; nobody may pick up a mob's arrow
TEST(arrow_sticks_in_ground) {
	LevelFixture f(1);
	Mob* skeleton = spawn(f, "minecraft:skeleton", {0, Y, 0});
	CHECK(skeleton);
	if (!skeleton) return;
	auto  arrow = std::make_unique<Arrow>(*f.level, *skeleton, ItemStack{}, nullptr);
	Arrow* shot	= arrow.get();
	arrow->shoot(1.0, 0.2, 0.0, 1.6F, 0.0F);
	f.level->entities().add(std::move(arrow));
	for (int t = 0; t < 100 && !shot->isInGround(); t++) f.tick();
	CHECK(shot->isInGround());
	CHECK(shot->position().x > 3.0);
	CHECK(shot->pickup() == Arrow::Pickup::Disallowed);
}

// A witch throws splash potions at a player: slowness from afar first (or poison), which the player gets
TEST(witch_throws_potions) {
	LevelFixture f(2);
	auto player = addPlayer(f, 9.5, 0.5);
	Mob* witch	= spawn(f, "minecraft:witch", {0, Y, 0});
	CHECK(witch);
	if (!witch) return;
	bool affected = false;
	for (int t = 0; t < 400 && !affected; t++) {
		f.tick();
		affected = !player->effects().empty() || player->combat().health < 20.0F;
	}
	CHECK(affected);
	CHECK(player->hasEffect(MobEffects::Slowness) || player->hasEffect(MobEffects::Poison) || player->combat().health < 20.0F);
}

// A snow golem throws snowballs at a zombie (no damage but to blazes) and leaves snow where it stands
TEST(snow_golem_throws_snowballs) {
	LevelFixture f(2);
	Mob* golem	= spawn(f, "minecraft:snow_golem", {0, Y, 0});
	Mob* zombie = spawn(f, "minecraft:zombie", {6, Y, 0});
	CHECK(golem && zombie);
	if (!golem || !zombie) return;
	zombie->goalSelector().removeAllGoals();
	zombie->targetSelector().removeAllGoals();
	bool hit = false;
	for (int t = 0; t < 200 && !hit; t++) {
		f.tick();
		hit = zombie->hurtTime() > 0;
	}
	CHECK(hit);
	CHECK_EQ(zombie->health(), zombie->maxHealth());
	CHECK(f.level->getBlockState(golem->blockPosition()) == f.data.getDefaultBlockState("minecraft:snow"));
}

// A tamed wolf has 40 health, sits when told, and attacks what its owner hits
TEST(wolf_tamed_defends_owner) {
	LevelFixture f(2);
	auto  player = addPlayer(f, 2.5, 0.5);
	auto* wolf	 = dynamic_cast<Wolf*>(spawn(f, "minecraft:wolf", {0, Y, 0}));
	Mob*  zombie = spawn(f, "minecraft:zombie", {5, Y, 2});
	CHECK(wolf && zombie);
	if (!wolf || !zombie) return;
	zombie->goalSelector().removeAllGoals();
	zombie->targetSelector().removeAllGoals();
	CHECK_EQ(wolf->maxHealth(), 8.0F);
	wolf->tame(*player);
	CHECK(wolf->isTame() && wolf->getOwner() == player.get());
	CHECK_EQ(wolf->health(), 40.0F);
	// Ordered to sit: it stays put
	wolf->setOrderedToSit(true);
	f.tick(10);
	CHECK(wolf->isInSittingPose());
	wolf->setOrderedToSit(false);
	// Its owner hits a zombie: the wolf goes for it
	player->setLastAttack(-100);
	player->setPosition(4.5, Y, 2.5);
	player->survival().tickCount = 50; // The fixture doesn't tick the players
	Combat::attack(f.server, *player, *zombie);
	float health = zombie->health();
	bool  targeted = false;
	for (int t = 0; t < 200 && zombie->health() >= health; t++) {
		f.tick();
		targeted |= wolf->getTarget() == zombie;
	}
	CHECK(targeted);
	CHECK(zombie->health() < health);
}

// A cod swims around in water and keeps its air; on land it flops, runs out of air and dies
TEST(fish_swims_and_suffocates_on_land) {
	LevelFixture f(1);
	int water = f.data.getDefaultBlockState("minecraft:water");
	for (int x = -4; x <= 4; x++) {
		for (int z = -4; z <= 4; z++) {
			for (int y = Y; y < Y + 5; y++) f.level->setBlock({x, y, z}, water, Level::UPDATE_ALL);
		}
	}
	Mob* cod = spawn(f, "minecraft:cod", {0, Y + 2, 0});
	CHECK(cod);
	if (!cod) return;
	Vec3   start = cod->position();
	double moved = 0.0;
	for (int t = 0; t < 400; t++) {
		f.tick();
		moved = std::max(moved, (cod->position() - start).length());
	}
	CHECK(cod->isAlive());
	CHECK(moved > 1.0);
	CHECK_EQ(cod->airSupply(), 300);
	Mob* beached = spawn(f, "minecraft:cod", {12, Y, 12});
	CHECK(beached);
	if (!beached) return;
	int id = beached->id();
	f.tick(450);
	Actor* still = f.level->actorById(id); // Gone once dead
	CHECK(!still || !still->isAlive());
}

// A strider stands on the surface of lava instead of sinking, and isn't hurt by it
TEST(strider_stands_on_lava) {
	LevelFixture f(1);
	int lava = f.data.getDefaultBlockState("minecraft:lava");
	for (int x = -4; x <= 4; x++) {
		for (int z = -4; z <= 4; z++) {
			for (int y = Y; y < Y + 3; y++) f.level->setBlock({x, y, z}, lava, Level::UPDATE_ALL);
		}
	}
	Mob* strider = spawn(f, "minecraft:strider", {0, Y + 4, 0});
	CHECK(strider);
	if (!strider) return;
	float health = strider->health();
	f.tick(200);
	CHECK(strider->isAlive());
	CHECK_EQ(strider->health(), health);
	CHECK(strider->position().y > Y + 2.4 && strider->position().y < Y + 3.1);
}

// A red fox hunts the chicken it sees at night: stalks, pounces, bites
TEST(fox_hunts_chicken) {
	LevelFixture f(1);
	Mob* fox	 = spawn(f, "minecraft:fox", {0, Y, 0});
	Mob* chicken = spawn(f, "minecraft:chicken", {7, Y, 0});
	CHECK(fox && chicken);
	if (!fox || !chicken) return;
	fox->setBaby(false);
	chicken->goalSelector().removeAllGoals();
	int	 id	  = chicken->id();
	bool hurt = false;
	for (int t = 0; t < 1200 && !hurt; t++) {
		f.tick();
		Actor* still = f.level->actorById(id);
		hurt		 = !still || !still->isAlive() || static_cast<Mob*>(still)->health() < 4.0F;
	}
	CHECK(hurt);
}

// Outside the Nether a hoglin turns into a zoglin after 15 seconds
TEST(hoglin_zombifies_in_overworld) {
	LevelFixture f(1);
	Mob* hoglin = spawn(f, "minecraft:hoglin", {0, Y, 0});
	CHECK(hoglin);
	if (!hoglin) return;
	int id = hoglin->id();
	f.tick(320);
	CHECK(f.level->actorById(id) == nullptr || !f.level->actorById(id)->isAlive());
	int zoglins = 0;
	f.level->entities().forEach([&](Entity& entity) {
		if (entity.typeId() == f.data.getStaticId("minecraft:entity_type", "minecraft:zoglin") && !entity.isRemoved()) zoglins++;
	});
	CHECK_EQ(zoglins, 1);
}

// A zoglin attacks any living thing, a cow too
TEST(zoglin_attacks_cow) {
	LevelFixture f(1);
	Mob* zoglin = spawn(f, "minecraft:zoglin", {0, Y, 0});
	Mob* cow	= spawn(f, "minecraft:cow", {6, Y, 0});
	CHECK(zoglin && cow);
	if (!zoglin || !cow) return;
	cow->goalSelector().removeAllGoals();
	int	  id	 = cow->id();
	float health = cow->health();
	bool  hurt	 = false;
	for (int t = 0; t < 400 && !hurt; t++) {
		f.tick();
		Actor* still = f.level->actorById(id);
		hurt		 = !still || !still->isAlive() || static_cast<Mob*>(still)->health() < health;
	}
	CHECK(hurt);
}

namespace {
	// Ticks until the player loses health; whether it did
	bool hurtsPlayer(LevelFixture& f, Player& player, int ticks) {
		float health = player.combat().health;
		for (int t = 0; t < ticks && player.combat().health >= health; t++) f.tick();
		return player.combat().health < health;
	}
} // namespace

// A vindicator charges the player with its axe
TEST(vindicator_hits_player) {
	LevelFixture f(1);
	auto player = addPlayer(f, 8.5, 0.5);
	Mob* mob	= spawn(f, "minecraft:vindicator", {0, Y, 0});
	CHECK(mob);
	if (!mob) return;
	CHECK(!mob->getMainHandItem().isEmpty());
	CHECK(hurtsPlayer(f, *player, 400));
}

// A pillager loads its crossbow and shoots the player from afar
TEST(pillager_shoots_player) {
	LevelFixture f(2);
	auto player = addPlayer(f, 7.5, 0.5);
	Mob* mob	= spawn(f, "minecraft:pillager", {0, Y, 0});
	CHECK(mob);
	if (!mob) return;
	CHECK(hurtsPlayer(f, *player, 600));
	CHECK(mob->distanceToSqr(*player) > 4.0); // From a distance
}

// An evoker summons vexes and bites with fangs
TEST(evoker_casts_spells) {
	LevelFixture f(2);
	auto player = addPlayer(f, 6.5, 0.5);
	Mob* mob	= spawn(f, "minecraft:evoker", {0, Y, 0});
	CHECK(mob);
	if (!mob) return;
	int	 vexType = f.data.getStaticId("minecraft:entity_type", "minecraft:vex");
	int	 vexes	 = 0;
	bool hurt	 = false;
	float health = player->combat().health;
	for (int t = 0; t < 1500 && (vexes == 0 || !hurt); t++) {
		f.tick();
		hurt |= player->combat().health < health;
		vexes = 0;
		f.level->entities().forEach([&](Entity& entity) {
			if (entity.typeId() == vexType && !entity.isRemoved()) vexes++;
		});
	}
	CHECK(vexes > 0);
	CHECK(hurt);
}

// With nothing to fight, an evoker turns a blue sheep red
TEST(evoker_wololo) {
	LevelFixture f(1);
	Mob*  evoker = spawn(f, "minecraft:evoker", {0, Y, 0});
	auto* sheep	 = dynamic_cast<Sheep*>(spawn(f, "minecraft:sheep", {4, Y, 0}));
	CHECK(evoker && sheep);
	if (!evoker || !sheep) return;
	sheep->setColor(11);
	sheep->goalSelector().removeAllGoals();
	for (int t = 0; t < 400 && sheep->color() == 11; t++) f.tick();
	CHECK_EQ(sheep->color(), 14);
}

// A ravager bites the player
TEST(ravager_bites_player) {
	LevelFixture f(1);
	auto player = addPlayer(f, 8.5, 0.5);
	Mob* mob	= spawn(f, "minecraft:ravager", {0, Y, 0});
	CHECK(mob);
	if (!mob) return;
	CHECK(hurtsPlayer(f, *player, 400));
}

// A llama spits at a wild wolf nearby
TEST(llama_spits_at_wolf) {
	LevelFixture f(1);
	Mob* llama = spawn(f, "minecraft:llama", {0, Y, 0});
	Mob* wolf  = spawn(f, "minecraft:wolf", {3, Y, 0}); // Llamas only look 4 blocks away for wolves
	CHECK(llama && wolf);
	if (!llama || !wolf) return;
	llama->setBaby(false);
	wolf->goalSelector().removeAllGoals();
	wolf->targetSelector().removeAllGoals();
	float health = wolf->health();
	bool  targeted = false;
	for (int t = 0; t < 2000 && wolf->health() >= health; t++) {
		f.tick();
		targeted |= llama->getTarget() == wolf;
	}
	CHECK(targeted);
	CHECK(wolf->health() < health);
}

// A camel sits down after a while, and stands up when hurt
TEST(camel_sits_and_stands) {
	LevelFixture f(1);
	auto* camel = dynamic_cast<Camel*>(spawn(f, "minecraft:camel", {0, Y, 0}));
	CHECK(camel);
	if (!camel) return;
	camel->setBaby(false);
	bool sat = false;
	for (int t = 0; t < 4000 && !sat; t++) {
		f.tick();
		sat = camel->isCamelSitting();
	}
	CHECK(sat);
	camel->hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, 1.0F);
	CHECK(!camel->isCamelSitting());
}

// A screaming goat soon rams a cow standing a few blocks away
TEST(goat_rams_cow) {
	LevelFixture f(1);
	auto* goat = dynamic_cast<Goat*>(spawn(f, "minecraft:goat", {0, Y, 0}));
	Mob*  cow  = spawn(f, "minecraft:cow", {5, Y, 0});
	CHECK(goat && cow);
	if (!goat || !cow) return;
	goat->setBaby(false);
	goat->setScreamingGoat(true);
	cow->goalSelector().removeAllGoals();
	float health = cow->health();
	for (int t = 0; t < 3000 && cow->health() >= health; t++) f.tick();
	CHECK(cow->health() < health);
}

// Hurt, an armadillo rolls up and takes less damage; safe again, it unrolls
TEST(armadillo_rolls_up) {
	LevelFixture f(1);
	auto* armadillo = dynamic_cast<Armadillo*>(spawn(f, "minecraft:armadillo", {0, Y, 0}));
	CHECK(armadillo);
	if (!armadillo) return;
	armadillo->setBaby(false);
	armadillo->hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, 1.0F);
	f.tick(10);
	CHECK(armadillo->state() == Armadillo::State::Scared);
	float health = armadillo->health();
	f.tick(20); // Past the invulnerability
	armadillo->hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, 5.0F);
	CHECK_EQ(armadillo->health(), health - 2.0F);
	f.tick(200);
	CHECK(armadillo->state() == Armadillo::State::Idle);
}

// A sniffer digs up an ancient seed from the grass
TEST(sniffer_digs_seed) {
	LevelFixture f(1);
	auto* sniffer = dynamic_cast<Sniffer*>(spawn(f, "minecraft:sniffer", {0, Y, 0}));
	CHECK(sniffer);
	if (!sniffer) return;
	sniffer->setBaby(false);
	int	 torch = f.data.getStaticId("minecraft:item", "minecraft:torchflower_seeds");
	int	 pod   = f.data.getStaticId("minecraft:item", "minecraft:pitcher_pod");
	bool found = false;
	for (int t = 0; t < 6000 && !found; t++) {
		f.tick();
		if (t % 20) continue;
		f.level->entities().forEach([&](Entity& entity) {
			if (auto* item = dynamic_cast<ItemEntity*>(&entity); item && (item->item().item == torch || item->item().item == pod)) found = true;
		});
	}
	CHECK(found);
}

namespace {
	// A pool of water, 9 by 9 and 3 deep, dug into the ground around (0, 0)
	void digPool(LevelFixture& f) {
		int water = f.data.getDefaultBlockState("minecraft:water");
		for (int x = -4; x <= 4; x++) {
			for (int z = -4; z <= 4; z++) {
				for (int y = Y - 3; y < Y; y++) f.level->setBlock({x, y, z}, water, Level::UPDATE_ALL);
			}
		}
	}

	int countOf(LevelFixture& f, const char* type) {
		int id = f.data.getStaticId("minecraft:entity_type", type), count = 0;
		f.level->entities().forEach([&](Entity& entity) {
			if (entity.typeId() == id && !entity.isRemoved()) count++;
		});
		return count;
	}
} // namespace

// A frog catches a tiny slime with its tongue
TEST(frog_eats_tiny_slime) {
	LevelFixture f(1);
	Mob*   frog	 = spawn(f, "minecraft:frog", {0, Y, 0});
	auto*  slime = dynamic_cast<Slime*>(spawn(f, "minecraft:slime", {4, Y, 0}));
	CHECK(frog && slime);
	if (!frog || !slime) return;
	frog->setBaby(false);
	slime->setSize(1, true);
	slime->goalSelector().removeAllGoals();
	int id = slime->id();
	for (int t = 0; t < 600 && f.level->actorById(id) && f.level->actorById(id)->isAlive(); t++) f.tick();
	CHECK(!f.level->actorById(id) || !f.level->actorById(id)->isAlive());
}

// A tadpole about to grow up turns into a frog
TEST(tadpole_grows_into_frog) {
	LevelFixture f(1);
	digPool(f);
	auto* tadpole = dynamic_cast<Tadpole*>(spawn(f, "minecraft:tadpole", {0, Y - 2, 0}));
	CHECK(tadpole);
	if (!tadpole) return;
	tadpole->setAge(Tadpole::MAX_AGE - 10);
	f.tick(20);
	CHECK_EQ(countOf(f, "minecraft:tadpole"), 0);
	CHECK_EQ(countOf(f, "minecraft:frog"), 1);
}

// An axolotl hunts a cod in the water
TEST(axolotl_hunts_fish) {
	LevelFixture f(1);
	digPool(f);
	Mob* axolotl = spawn(f, "minecraft:axolotl", {-2, Y - 2, 0});
	Mob* cod	 = spawn(f, "minecraft:cod", {2, Y - 2, 0});
	CHECK(axolotl && cod);
	if (!axolotl || !cod) return;
	axolotl->setBaby(false);
	cod->goalSelector().removeAllGoals();
	float health = cod->health();
	for (int t = 0; t < 600 && cod->isAlive() && cod->health() >= health; t++) f.tick();
	CHECK(!cod->isAlive() || cod->health() < health);
}

// Out of the water, an axolotl dries out
TEST(axolotl_dries_out) {
	LevelFixture f(1);
	Mob* axolotl = spawn(f, "minecraft:axolotl", {0, Y, 0});
	CHECK(axolotl);
	if (!axolotl) return;
	axolotl->setAirSupply(5);
	float health = axolotl->health();
	f.tick(40);
	CHECK(axolotl->health() < health);
}

// A parrot flies around: it gets off the ground
TEST(parrot_flies) {
	LevelFixture f(1);
	Mob* parrot = spawn(f, "minecraft:parrot", {0, Y, 0});
	CHECK(parrot);
	if (!parrot) return;
	double highest = 0;
	for (int t = 0; t < 1200 && highest < Y + 1.5; t++) {
		f.tick();
		highest = std::max(highest, parrot->position().y);
	}
	CHECK(highest >= Y + 1.5);
}

// A bee pollinates the flower next to its hive and brings the nectar home: the hive's honey level rises
TEST(bee_makes_honey) {
	LevelFixture f(1);
	f.level->setBlock({4, Y, 0}, f.data.getDefaultBlockState("minecraft:beehive"), Level::UPDATE_ALL);
	f.level->setBlock({1, Y, 2}, f.data.getDefaultBlockState("minecraft:dandelion"), Level::UPDATE_ALL);
	auto* bee = dynamic_cast<Bee*>(spawn(f, "minecraft:bee", {0, Y + 1, 0}));
	CHECK(bee);
	if (!bee) return;
	bee->setBaby(false);
	f.world->setDayTime(1000); // In the day: at night it stays in its hive
	bool honey = false, nectar = false;
	for (int t = 0; t < 9000 && !honey; t++) {
		f.tick();
		nectar |= bee->hasNectar();
		honey = f.data.getProperty(f.level->getBlockState({4, Y, 0}), "honey_level") != "0";
	}
	CHECK(nectar);
	CHECK(honey);
}

// Hit by a player, a bee stings back and loses its stinger
TEST(bee_stings_attacker) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 3.5, 0.5);
	auto* bee	 = dynamic_cast<Bee*>(spawn(f, "minecraft:bee", {0, Y + 1, 0}));
	CHECK(bee);
	if (!bee) return;
	bee->setBaby(false);
	bee->hurtServer({"minecraft:player_attack", player.get(), nullptr, std::nullopt}, 1.0F);
	float health = player->combat().health;
	for (int t = 0; t < 400 && !bee->hasStung(); t++) f.tick();
	CHECK(bee->hasStung());
	CHECK(player->combat().health < health);
}

// A breeze shoots wind charges at the player
TEST(breeze_shoots_wind_charges) {
	LevelFixture f(2);
	auto player = addPlayer(f, 10.5, 0.5);
	Mob* breeze = spawn(f, "minecraft:breeze", {0, Y, 0});
	CHECK(breeze);
	if (!breeze) return;
	CHECK(hurtsPlayer(f, *player, 600));
}

// A creaking freezes while the player looks at it, and comes for the player otherwise
TEST(creaking_freezes_when_watched) {
	LevelFixture f(1);
	auto  player   = addPlayer(f, 10.5, 0.5);
	auto* creaking = dynamic_cast<Creaking*>(spawn(f, "minecraft:creaking", {0, Y, 0}));
	CHECK(creaking);
	if (!creaking) return;
	player->setRotation(90.0F, 0.0F); // Facing -x: toward the creaking
	f.tick(40);
	CHECK(!creaking->canMove());
	double x = creaking->position().x;
	f.tick(100);
	CHECK(std::abs(creaking->position().x - x) < 0.1);
	player->setRotation(-90.0F, 0.0F); // Back turned
	bool moved = false, couldMove = false;
	for (int t = 0; t < 100 && !moved; t++) {
		f.tick();
		couldMove |= creaking->canMove();
		moved = creaking->position().x > x + 1.0;
	}
	CHECK(couldMove);
	CHECK(moved);
}

// A shulker opens up and shoots a homing bullet at the player: hurt, and levitating
TEST(shulker_bullet_levitates_player) {
	LevelFixture f(1);
	auto player	 = addPlayer(f, 8.5, 0.5);
	Mob* shulker = spawn(f, "minecraft:shulker", {0, Y, 0});
	CHECK(shulker);
	if (!shulker) return;
	bool levitating = false;
	for (int t = 0; t < 600 && !levitating; t++) {
		f.tick();
		levitating = player->hasEffect(MobEffects::Levitation);
	}
	CHECK(levitating);
	CHECK(shulker->position().x == 0.5 && shulker->position().z == 0.5); // It never moved
}

// A warden doesn't hear a sneaking player; one walking about is hunted down and hit hard
TEST(warden_hears_walking_player) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 8.5, 0.5);
	auto* warden = dynamic_cast<Warden*>(spawn(f, "minecraft:warden", {0, Y, 0}));
	CHECK(warden);
	if (!warden) return;
	warden->goalSelector().removeAllGoals(); // Standing still: only its hearing is tested first
	player->setShiftKeyDown(true);
	for (int t = 0; t < 200; t++) {
		player->setPosition(8.5 + (t % 2) * 0.2, Y, 0.5);
		f.tick();
	}
	CHECK_EQ(warden->angerAt(*player), 0);
	player->setShiftKeyDown(false);
	for (int t = 0; t < 200; t++) {
		player->setPosition(8.5 + (t % 2) * 0.2, Y, 0.5);
		f.tick();
	}
	CHECK(warden->angerAt(*player) >= Warden::ANGERY);
	CHECK(warden->getTarget() == player.get());
}

// Angry, a warden walks up and swipes, or booms from afar
TEST(warden_attacks_angry_target) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 8.5, 0.5);
	auto* warden = dynamic_cast<Warden*>(spawn(f, "minecraft:warden", {0, Y, 0}));
	CHECK(warden);
	if (!warden) return;
	warden->increaseAngerAt(*player, Warden::MAX_ANGER, false);
	CHECK(hurtsPlayer(f, *player, 600));
}

// Given a diamond, an allay collects the diamonds lying around and brings them to its player
TEST(allay_brings_items) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 0.5, 0.5);
	auto* allay	 = dynamic_cast<Allay*>(spawn(f, "minecraft:allay", {1, Y + 1, 0}));
	CHECK(allay);
	if (!allay) return;
	int diamond = f.item("minecraft:diamond");
	player->inventory().set(player->handSlot(0), ItemStack(diamond, 1));
	CHECK(allay->mobInteract(*player, 0));
	CHECK(!allay->getMainHandItem().isEmpty());
	f.level->entities().add(std::make_unique<ItemEntity>(*f.level, Vec3{8.5, Y, 0.5}, ItemStack(diamond, 5), Vec3{}));
	bool collected = false, delivered = false;
	for (int t = 0; t < 1200 && !delivered; t++) {
		f.tick();
		collected |= allay->inventory().count == 5;
		if (collected && allay->inventory().isEmpty()) {
			for (ItemEntity* item : f.items()) delivered |= item->item().item == diamond && item->distanceToSqr(*player) < 6.0 * 6.0;
		}
	}
	CHECK(collected);
	CHECK(delivered);
}

// A copper golem carries the items of a copper chest to the chest that already holds the same item
TEST(copper_golem_sorts_items) {
	LevelFixture f(1);
	f.level->setBlock({3, Y, 0}, f.data.getDefaultBlockState("minecraft:copper_chest"), Level::UPDATE_ALL);
	f.level->setBlock({-3, Y, 3}, f.data.getDefaultBlockState("minecraft:chest"), Level::UPDATE_ALL);
	f.level->setBlock({-3, Y, -3}, f.data.getDefaultBlockState("minecraft:chest"), Level::UPDATE_ALL);
	auto* source = dynamic_cast<Container*>(f.level->getBlockEntity({3, Y, 0}));
	auto* other	 = dynamic_cast<Container*>(f.level->getBlockEntity({-3, Y, 3}));
	auto* same	 = dynamic_cast<Container*>(f.level->getBlockEntity({-3, Y, -3}));
	CHECK(source && other && same);
	if (!source || !other || !same) return;
	int iron = f.item("minecraft:iron_ingot");
	source->setItem(0, ItemStack(iron, 10));
	other->setItem(0, ItemStack(f.item("minecraft:dirt"), 1));
	same->setItem(0, ItemStack(iron, 1));
	Mob* golem = spawn(f, "minecraft:copper_golem", {0, Y, 0});
	CHECK(golem);
	if (!golem) return;
	for (int t = 0; t < 2400 && same->item(0).count < 11; t++) f.tick();
	CHECK_EQ(same->item(0).count, 11);
	CHECK(source->item(0).isEmpty());
}

// A happy ghast floats: it doesn't fall to the ground
TEST(happy_ghast_floats) {
	LevelFixture f(1);
	Mob* ghast = spawn(f, "minecraft:happy_ghast", {0, Y + 6, 0});
	CHECK(ghast);
	if (!ghast) return;
	ghast->setBaby(false);
	f.tick(100);
	CHECK(ghast->position().y > Y + 2.0);
}

// A piglin picks up a gold ingot, admires it, and throws something back
TEST(piglin_barters_gold) {
	LevelFixture f(1);
	auto* piglin = dynamic_cast<Piglin*>(spawn(f, "minecraft:piglin", {0, Y, 0}));
	CHECK(piglin);
	if (!piglin) return;
	piglin->setBaby(false);
	piglin->setImmuneToZombification(true);
	int gold = f.item("minecraft:gold_ingot");
	f.level->entities().add(std::make_unique<ItemEntity>(*f.level, Vec3{3.5, Y, 0.5}, ItemStack(gold, 1), Vec3{}));
	bool admired = false, bartered = false;
	for (int t = 0; t < 600 && !bartered; t++) {
		f.tick();
		admired |= piglin->isAdmiring();
		if (admired && !piglin->isAdmiring()) {
			for (ItemEntity* item : f.items()) bartered |= item->item().item != gold;
		}
	}
	CHECK(admired);
	CHECK(bartered);
}

// A piglin attacks a player without gold, and leaves alone one wearing a golden helmet
TEST(piglin_spares_gold_wearers) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 6.5, 0.5);
	auto* piglin = dynamic_cast<Piglin*>(spawn(f, "minecraft:piglin", {0, Y, 0}));
	CHECK(piglin);
	if (!piglin) return;
	piglin->setBaby(false);
	piglin->setImmuneToZombification(true);
	player->inventory().set(5, ItemStack(f.item("minecraft:golden_helmet"), 1));
	f.tick(200);
	CHECK(piglin->getTarget() == nullptr);
	player->inventory().set(5, ItemStack{});
	CHECK(hurtsPlayer(f, *player, 600));
}

// A villager near a lectern becomes a librarian, and sleeps in the bed nearby at night
TEST(villager_takes_job_and_sleeps) {
	LevelFixture f(1);
	f.level->setBlock({4, Y, 0}, f.data.getDefaultBlockState("minecraft:lectern"), Level::UPDATE_ALL);
	int foot = f.data.getBlockState("minecraft:red_bed", {{"facing", "south"}, {"occupied", "false"}, {"part", "foot"}});
	int head = f.data.getBlockState("minecraft:red_bed", {{"facing", "south"}, {"occupied", "false"}, {"part", "head"}});
	f.level->setBlock({-4, Y, 0}, foot, Level::UPDATE_ALL);
	f.level->setBlock({-4, Y, 1}, head, Level::UPDATE_ALL);
	auto* villager = dynamic_cast<Villager*>(spawn(f, "minecraft:villager", {0, Y, 0}));
	CHECK(villager);
	if (!villager) return;
	villager->setBaby(false);
	f.world->setDayTime(3000);
	f.tick(100);
	CHECK(villager->professionName() == "minecraft:librarian");
	CHECK(villager->home().has_value());
	f.world->setDayTime(13000);
	bool slept = false;
	for (int t = 0; t < 600 && !slept; t++) {
		f.tick();
		slept = villager->isSleeping();
	}
	CHECK(slept);
}

// A villager runs from a zombie
TEST(villager_flees_zombie) {
	LevelFixture f(1);
	auto* villager = dynamic_cast<Villager*>(spawn(f, "minecraft:villager", {0, Y, 0}));
	Mob*  zombie   = spawn(f, "minecraft:zombie", {4, Y, 0});
	CHECK(villager && zombie);
	if (!villager || !zombie) return;
	villager->setBaby(false);
	zombie->goalSelector().removeAllGoals();
	zombie->targetSelector().removeAllGoals();
	f.world->setDayTime(1000);
	f.tick(100);
	CHECK(villager->distanceToSqr(*zombie) > 6.0 * 6.0);
}

// A T of soul sand topped with three wither skeleton skulls summons a wither: it charges up, explodes, then attacks
TEST(wither_summoned_by_pattern) {
	LevelFixture f(2);
	auto player = addPlayer(f, 12.5, 0.5);
	int	 soul	= f.data.getDefaultBlockState("minecraft:soul_sand");
	int	 skull	= f.data.getDefaultBlockState("minecraft:wither_skeleton_skull");
	f.level->setBlock({0, Y, 0}, soul, Level::UPDATE_ALL);
	for (int x = -1; x <= 1; x++) f.level->setBlock({x, Y + 1, 0}, soul, Level::UPDATE_ALL);
	for (int x = -1; x <= 1; x++) f.level->setBlock({x, Y + 2, 0}, skull, Level::UPDATE_ALL);
	WitherBoss* wither = nullptr;
	f.level->entities().forEach([&](Entity& entity) {
		if (auto* w = dynamic_cast<WitherBoss*>(&entity)) wither = w;
	});
	CHECK(wither);
	if (!wither) return;
	CHECK(wither->invulnerableTicks() > 0);
	CHECK(f.level->blocks().isAir(f.level->getBlockState({0, Y + 2, 0}))); // The pattern is gone
	f.tick(230);
	CHECK_EQ(wither->invulnerableTicks(), 0);
	CHECK(hurtsPlayer(f, *player, 600));
}

// The ender dragon's parts (the 8 ids after its own) are the dragon; killed, it rises and goes after 10 seconds
TEST(ender_dragon_parts_and_death) {
	LevelFixture f(2);
	auto* dragon = dynamic_cast<EnderDragon*>(spawn(f, "minecraft:ender_dragon", {0, Y + 30, 0}));
	CHECK(dragon);
	if (!dragon) return;
	for (int part = 1; part <= EnderDragon::PARTS; part++) CHECK(f.level->actorById(dragon->id() + part) == dragon);
	Vec3 start = dragon->position();
	f.tick(40);
	CHECK((dragon->position() - start).lengthSqr() > 1.0); // It flies
	dragon->hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, 1000.0F);
	CHECK(dragon->phase() == EnderDragon::Phase::Dying);
	int id = dragon->id();
	f.tick(210);
	CHECK(f.level->actorById(id) == nullptr);
}

// Every mob type: spawned, ticked for 5 seconds, saved and loaded back without error
TEST(every_mob_ticks_saves_and_loads) {
	LevelFixture f(2);
	auto player = addPlayer(f, 20.5, 20.5);
	std::vector<std::pair<std::string, int>> mobs; // By id: the ones that convert or die are freed
	for (int id = 0;; id++) {
		const GameData::EntityTypeInfo* type = f.data.getEntityType(id);
		if (!type) break;
		if (!type->mob) continue;
		std::string name = f.data.getStaticName("minecraft:entity_type", id);
		int			i	 = static_cast<int>(mobs.size());
		Mob*		mob	 = spawn(f, name.c_str(), {(i % 9) * 3 - 12, Y + 1, (i / 9) * 3 - 12});
		CHECK(mob != nullptr);
		if (mob) mobs.emplace_back(name, mob->id());
	}
	f.tick(100);
	int loaded = 0;
	for (auto& [name, id] : mobs) {
		auto* mob = dynamic_cast<Mob*>(f.level->actorById(id));
		if (!mob || mob->isRemoved()) continue; // Died, despawned or converted meanwhile
		Buffer saved;
		mob->save(saved);
		std::unique_ptr<Mob> copy = f.level->mobs().create(*f.level, mob->typeId());
		CHECK(copy != nullptr);
		if (!copy) continue;
		try {
			Buffer in(saved.getData());
			copy->load(in);
			loaded++;
		} catch (const std::exception& e) {
			std::printf("  load of %s failed: %s\n", name.c_str(), e.what());
			CHECK(false);
		}
	}
	CHECK(loaded > 60);
}


// A librarian's trades open on a click; selecting an offer moves its payment in, taking the result pays and counts
TEST(villager_trading) {
	LevelFixture f(1);
	auto player = addPlayer(f, 1.5, 0.5);
	f.level->setBlock({3, Y, 0}, f.data.getDefaultBlockState("minecraft:lectern"), Level::UPDATE_ALL);
	auto* villager = dynamic_cast<Villager*>(spawn(f, "minecraft:villager", {0, Y, 0}));
	CHECK(villager);
	if (!villager) return;
	villager->setBaby(false);
	f.world->setDayTime(3000);
	f.tick(60);
	CHECK(villager->hasJob());
	// PlayerInventory's slots are the window's: 9-35 the main inventory, 36-44 the hotbar
	player->inventory().set(9, ItemStack(f.item("minecraft:paper"), 64));
	player->inventory().set(10, ItemStack(f.item("minecraft:emerald"), 64));
	player->inventory().set(11, ItemStack(f.item("minecraft:book"), 64));
	player->setSelectedSlot(8); // An empty hand
	CHECK(villager->mobInteract(*player, 0));
	auto* menu = dynamic_cast<MerchantMenu*>(player->openMenuSlot().get());
	CHECK(menu);
	if (!menu) return;
	CHECK(villager->tradingPlayer() == player.get());
	CHECK_EQ(static_cast<int>(villager->offers().size()), 2);
	menu->selectTrade(0);
	CHECK(!menu->slots()[2].item().isEmpty());
	ItemStack result = menu->slots()[2].item();
	menu->clicked(2, 0, ClickType::QuickMove);
	CHECK(villager->offers()[0].uses >= 1);
	CHECK(villager->merchantXp() > 0);
	bool got = false;
	for (int slot = 12; slot <= 44; slot++) got |= player->inventory().get(slot).item == result.item;
	CHECK(got || result.item == f.item("minecraft:emerald"));
}

// Frogspawn on water hatches into tadpoles; a sniffer egg cracks twice and hatches a snifflet
TEST(eggs_hatch) {
	LevelFixture f(1);
	digPool(f);
	BlockPos spawnPos{0, Y, 0};
	int		 frogspawn = f.data.getDefaultBlockState("minecraft:frogspawn");
	f.level->setBlock(spawnPos, frogspawn, Level::UPDATE_ALL);
	CHECK_EQ(f.level->getBlockState(spawnPos), frogspawn);
	FrogspawnBlock{}.tick(*f.level, spawnPos, frogspawn); // Its scheduled tick, without waiting 10 minutes
	f.tick();
	CHECK(f.level->blocks().isAir(f.level->getBlockState(spawnPos)));
	CHECK(countOf(f, "minecraft:tadpole") >= 2);

	BlockPos eggPos{6, Y, 6};
	f.level->setBlock(eggPos, f.data.getDefaultBlockState("minecraft:sniffer_egg"), Level::UPDATE_ALL);
	for (int i = 0; i < 3; i++) SnifferEggBlock{}.tick(*f.level, eggPos, f.level->getBlockState(eggPos));
	f.tick();
	CHECK(f.level->blocks().isAir(f.level->getBlockState(eggPos)));
	CHECK_EQ(countOf(f, "minecraft:sniffer"), 1);
}

// At night a bee goes into its hive (and leaves the level); the hive keeps it
TEST(bee_enters_hive_at_night) {
	LevelFixture f(1);
	f.level->setBlock({3, Y, 0}, f.data.getDefaultBlockState("minecraft:beehive"), Level::UPDATE_ALL);
	auto* bee = dynamic_cast<Bee*>(spawn(f, "minecraft:bee", {0, Y + 1, 0})); // spawn() makes it night
	CHECK(bee);
	if (!bee) return;
	bee->setBaby(false);
	int id = bee->id();
	for (int t = 0; t < 1200 && f.level->actorById(id); t++) f.tick();
	CHECK(f.level->actorById(id) == nullptr);
	auto* hive = dynamic_cast<BeehiveBlockEntity*>(f.level->getBlockEntity({3, Y, 0}));
	CHECK(hive && hive->occupantCount() == 1);
}

// The End: a player arrives, the arena is built (ten crystals on their spikes) and the dragon comes; dead, the exit
// portal opens with the egg on it
TEST(end_dragon_fight) {
	LevelFixture f(2, "minecraft:the_end");
	EndDragonFight* fight = f.level->endFight();
	CHECK(fight);
	if (!fight) return;
	auto player = addPlayer(f, 10.5, 10.5);
	f.tick(40);
	CHECK(fight->arenaBuilt());
	int crystals = 0;
	EnderDragon* dragon = nullptr;
	f.level->entities().forEach([&](Entity& entity) {
		if (dynamic_cast<EndCrystal*>(&entity)) crystals++;
		if (auto* d = dynamic_cast<EnderDragon*>(&entity)) dragon = d;
	});
	CHECK_EQ(crystals, 10);
	CHECK(dragon);
	if (!dragon) return;
	dragon->hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, 1000.0F);
	f.tick(220);
	CHECK(fight->dragonKilled());
	BlockPos portal = fight->portalLocation();
	CHECK(f.data.getStaticName("minecraft:block", f.level->blocks().blockOf(f.level->getBlockState(portal.offset(1, 0, 0)))) == "minecraft:end_portal");
	CHECK(f.data.getStaticName("minecraft:block", f.level->blocks().blockOf(f.level->getBlockState(portal.offset(0, 4, 0)))) == "minecraft:dragon_egg");

	// Its death opened a gateway (the first of 20) on the circle of 96 blocks; it leads 1024 blocks out, to a gateway back
	CHECK_EQ(fight->gatewaysLeft(), 19);
	BlockPos gateway;
	bool	 found = false;
	for (const BlockPos& pos : EndDragonFight::gatewayPositions(f.world->getSeed())) {
		if (f.nameAt(pos.x, pos.y, pos.z) == "minecraft:end_gateway") {
			gateway = pos;
			found	= true;
		}
	}
	CHECK(found);
	if (found) {
		auto transition = Portals::gatewayDestination(*f.level, *player, gateway);
		CHECK(transition && transition->level == f.level.get());
		if (transition) {
			double out = std::sqrt(transition->position.x * transition->position.x + transition->position.z * transition->position.z);
			CHECK(out > 1000.0 && out < 1050.0);
			auto* back = dynamic_cast<EndGatewayBlockEntity*>(
					f.level->getBlockEntity({Mth::floor(transition->position.x), Mth::floor(transition->position.y) + 9, Mth::floor(transition->position.z) - 2}));
			CHECK(back && back->exitPortal() && *back->exitPortal() == gateway);
		}
	}

	// Four crystals on the sides of the exit portal: the spikes are rebuilt and a new dragon comes
	for (Direction side : Directions::HORIZONTAL) {
		BlockPos at = portal.relative(side, 3).above();
		f.level->entities().add(std::make_unique<EndCrystal>(*f.level, Vec3{at.x + 0.5, static_cast<double>(at.y), at.z + 0.5}));
	}
	CHECK(fight->tryRespawn());
	CHECK(fight->isRespawning());
	f.tick(800);
	CHECK(!fight->isRespawning());
	CHECK(!fight->dragonKilled());
	int dragons = 0;
	crystals	= 0;
	f.level->entities().forEach([&](Entity& entity) {
		if (dynamic_cast<EnderDragon*>(&entity) && !entity.isRemoved()) dragons++;
		if (dynamic_cast<EndCrystal*>(&entity) && !entity.isRemoved()) crystals++;
	});
	CHECK_EQ(dragons, 1);
	CHECK_EQ(crystals, 10); // On the spikes again; the four used ones blew up
}

// A carved pumpkin on top of a T of iron blocks makes an iron golem; on two snow blocks a snow golem; on oxidized
// copper an oxidized copper golem
TEST(golems_built_from_patterns) {
	LevelFixture f(1);
	int pumpkin = f.data.getDefaultBlockState("minecraft:carved_pumpkin");
	int iron	= f.data.getDefaultBlockState("minecraft:iron_block");
	f.level->setBlock({0, Y, 0}, iron, Level::UPDATE_ALL);
	for (int x = -1; x <= 1; x++) f.level->setBlock({x, Y + 1, 0}, iron, Level::UPDATE_ALL);
	f.level->setBlock({0, Y + 2, 0}, pumpkin, Level::UPDATE_ALL);
	int snow = f.data.getDefaultBlockState("minecraft:snow_block");
	f.level->setBlock({6, Y, 0}, snow, Level::UPDATE_ALL);
	f.level->setBlock({6, Y + 1, 0}, snow, Level::UPDATE_ALL);
	f.level->setBlock({6, Y + 2, 0}, pumpkin, Level::UPDATE_ALL);
	f.level->setBlock({-6, Y, 0}, f.data.getDefaultBlockState("minecraft:oxidized_copper"), Level::UPDATE_ALL);
	f.level->setBlock({-6, Y + 1, 0}, pumpkin, Level::UPDATE_ALL);
	IronGolem*	 ironGolem	 = nullptr;
	CopperGolem* copperGolem = nullptr;
	int			 snowGolems	 = countOf(f, "minecraft:snow_golem");
	f.level->entities().forEach([&](Entity& entity) {
		if (auto* golem = dynamic_cast<IronGolem*>(&entity)) ironGolem = golem;
		if (auto* golem = dynamic_cast<CopperGolem*>(&entity)) copperGolem = golem;
	});
	CHECK(ironGolem && ironGolem->isPlayerCreated());
	CHECK_EQ(snowGolems, 1);
	CHECK(copperGolem && copperGolem->weather() == CopperGolem::Weather::Oxidized);
	CHECK(f.level->blocks().isAir(f.level->getBlockState({0, Y + 1, 0})));
}

// A patrol: its captain carries the banner and leads it far away, the others follow; killing the captain gives Bad Omen
TEST(patrol_and_bad_omen) {
	LevelFixture f(2);
	auto player = addPlayer(f, 30.5, 30.5);
	player->setGameMode(GameMode::Creative); // Not their target
	VillageEvents* events = f.level->villageEvents();
	CHECK(events);
	if (!events) return;
	CHECK_EQ(events->spawnPatrol({0, Y, 0}, 3), 3);
	Raider* captain = nullptr;
	f.level->entities().forEach([&](Entity& entity) {
		if (auto* raider = dynamic_cast<Raider*>(&entity); raider && raider->isPatrolLeader()) captain = raider;
	});
	CHECK(captain);
	if (!captain) return;
	CHECK(captain->patrolTarget().has_value());
	CHECK(!captain->getItemBySlot(EquipmentSlot::Head).isEmpty());
	Vec3 start = captain->position();
	f.tick(200);
	CHECK((captain->position() - start).horizontalDistanceSqr() > 4.0);
	player->setGameMode(GameMode::Survival);
	captain->hurtServer({"minecraft:player_attack", player.get(), nullptr, std::nullopt}, 1000.0F);
	CHECK(player->hasEffect(MobEffects::BadOmen));
}

// Bad Omen in a village: Raid Omen, then the raid; its first wave comes
TEST(raid_starts_in_village) {
	LevelFixture f(3);
	f.level->setBlock({0, Y, 0}, f.data.getDefaultBlockState("minecraft:bell"), Level::UPDATE_ALL);
	auto* villager = dynamic_cast<Villager*>(spawn(f, "minecraft:villager", {2, Y, 2}));
	CHECK(villager);
	if (!villager) return;
	villager->setBaby(false);
	f.world->setDayTime(1000);
	f.tick(60);
	CHECK(villager->meetingPoint().has_value());
	CHECK(f.level->isVillage({0, Y, 0}));
	auto player = addPlayer(f, 1.5, 1.5);
	player->addEffect({MobEffects::BadOmen, 120000, 0});
	f.tick(40);
	CHECK(player->hasEffect(MobEffects::RaidOmen));
	CHECK(!player->hasEffect(MobEffects::BadOmen));
	player->removeEffect(MobEffects::RaidOmen); // Its 30 seconds run out (players' effects don't tick in the fixture)
	f.tick(40);													  // The raid starts
	VillageEvents* events = f.level->villageEvents();
	CHECK(events && !events->raids().empty());
	if (!events || events->raids().empty()) return;
	Raid* raid = events->raids().begin()->second.get();
	f.tick(320);
	CHECK_EQ(raid->groupsSpawned(), 1);
	CHECK(raid->raidersAlive() > 0);
}

// Killed by a zombie in hard difficulty, a villager becomes a zombie villager of the same profession
TEST(villager_killed_by_zombie_converts) {
	LevelFixture f(1);
	f.server.getConfig().setDifficulty("hard");
	auto* villager = dynamic_cast<Villager*>(spawn(f, "minecraft:villager", {0, Y, 0}));
	Mob*  zombie   = spawn(f, "minecraft:zombie", {3, Y, 0});
	CHECK(villager && zombie);
	if (!villager || !zombie) return;
	villager->setBaby(false);
	int librarian = f.data.getStaticId("minecraft:villager_profession", "minecraft:librarian");
	villager->setVillagerData(villager->villagerType(), librarian, 2);
	villager->hurtServer({"minecraft:mob_attack", zombie, nullptr, std::nullopt}, 1000.0F);
	f.tick(2);
	ZombieVillager* converted = nullptr;
	f.level->entities().forEach([&](Entity& entity) {
		if (auto* z = dynamic_cast<ZombieVillager*>(&entity); z && !z->isRemoved()) converted = z;
	});
	CHECK(converted != nullptr);
	CHECK(converted && converted->profession() == librarian);
	f.server.getConfig().setDifficulty("normal");
}

// Villagers that slept lately, with no golem around: five of them together make an iron golem; then they know one
// is there
TEST(villagers_summon_iron_golem) {
	LevelFixture f(2);
	std::vector<Villager*> villagers;
	for (int i = 0; i < 5; i++) {
		auto* villager = dynamic_cast<Villager*>(spawn(f, "minecraft:villager", {i, Y, 0}));
		CHECK(villager);
		if (!villager) return;
		villager->setBaby(false);
		villager->goalSelector().removeAllGoals();
		villagers.push_back(villager);
	}
	int64_t now = f.level->getGameTime();
	CHECK(!villagers[0]->spawnGolemIfNeeded(now, 5)); // They haven't slept
	for (Villager* villager : villagers) villager->setLastSlept(now);
	CHECK(villagers[0]->spawnGolemIfNeeded(now, 5));
	int golems = 0;
	f.level->entities().forEach([&](Entity& entity) { golems += dynamic_cast<IronGolem*>(&entity) != nullptr; });
	CHECK_EQ(golems, 1);
	CHECK(!villagers[1]->spawnGolemIfNeeded(now + 10, 5)); // One was just seen
}

// A farmer at work harvests the ripe wheat next to it, picks up what drops, and plants seeds on the empty farmland
TEST(farmer_harvests_and_replants) {
	LevelFixture f(1);
	f.world->setDayTime(3000); // Work time
	for (int x = -1; x <= 1; x++) {
		for (int z = -1; z <= 1; z++) f.level->setBlock({x, Y - 1, z}, f.data.getDefaultBlockState("minecraft:farmland"), Level::UPDATE_ALL);
	}
	f.level->setBlock({0, Y - 1, 0}, f.data.getDefaultBlockState("minecraft:water"), Level::UPDATE_ALL);
	int ripe = f.data.withProperty(f.data.getDefaultBlockState("minecraft:wheat"), "age", "7");
	f.level->setBlock({1, Y, 0}, ripe, Level::UPDATE_ALL);
	f.level->setBlock({0, Y, 0}, f.data.getDefaultBlockState("minecraft:stone"), Level::UPDATE_ALL); // Over the water
	auto* farmer = dynamic_cast<Villager*>(spawn(f, "minecraft:villager", {0, Y + 1, 0}));
	CHECK(farmer);
	if (!farmer) return;
	farmer->setBaby(false);
	farmer->setVillagerData(farmer->villagerType(), f.data.getStaticId("minecraft:villager_profession", "minecraft:farmer"), 1);
	farmer->goalSelector().removeAllGoals();
	farmer->registerGoals();
	farmer->setPos({0.5, static_cast<double>(Y + 1), 0.5});
	farmer->addToInventory(f.item("minecraft:wheat_seeds"), 3);
	f.world->setDayTime(3000); // Work time (spawn() made it night)
	f.tick(400);
	int planted = 0;
	for (int x = -1; x <= 1; x++) {
		for (int z = -1; z <= 1; z++) planted += f.nameAt(x, Y, z).find("minecraft:wheat") == 0;
	}
	CHECK(planted >= 2); // Replanted the harvested one and more
	bool harvested = f.data.getProperty(f.at(1, Y, 0), "age") != "7";
	CHECK(harvested);
}

// A full beehive: shears get 3 honeycombs, a bottle a honey bottle; smoked by a lit campfire below, the bees stay calm,
// otherwise the ones around go for the player
TEST(beehive_harvest_and_smoke) {
	LevelFixture f(1);
	BlockPos hive{0, Y + 2, 0};
	int		 full = f.data.withProperty(f.data.getDefaultBlockState("minecraft:beehive"), "honey_level", "5");
	f.level->setBlock(hive, full, Level::UPDATE_ALL);
	f.level->setBlock({0, Y, 0}, f.data.withProperty(f.data.getDefaultBlockState("minecraft:campfire"), "lit", "true"), Level::UPDATE_ALL);
	CHECK(BeehiveBlock::isSmokey(*f.level, hive));
	auto  player = addPlayer(f, 1.5, 0.5);
	auto* bee	 = dynamic_cast<Bee*>(spawn(f, "minecraft:bee", {3, Y + 2, 0}));
	CHECK(bee);
	if (!bee) return;
	player->setSelectedSlot(0);
	player->inventory().set(player->handSlot(0), ItemStack(f.item("minecraft:glass_bottle"), 1));
	BlockHit hit{hive, Direction::South, 0.5, Y + 2.5, 1.0};
	int		 state = f.level->getBlockState(hive);
	CHECK(f.level->behavior(state).useItemOn(*f.level, hive, state, *player, 0, hit) == UseResult::Success);
	CHECK(player->inventory().get(player->handSlot(0)).item == f.item("minecraft:honey_bottle"));
	CHECK(f.data.getProperty(f.at(0, Y + 2, 0), "honey_level") == "0");
	CHECK(bee->getTarget() == nullptr); // Smoked: calm
	// Without the campfire: shears, and the bee attacks
	f.set(0, Y, 0, "minecraft:air");
	f.level->setBlock(hive, full, Level::UPDATE_ALL);
	player->inventory().set(player->handSlot(0), ItemStack(f.item("minecraft:shears"), 1));
	state = f.level->getBlockState(hive);
	f.level->behavior(state).useItemOn(*f.level, hive, state, *player, 0, hit);
	int combs = 0;
	for (ItemEntity* item : f.items()) combs += item->item().item == f.item("minecraft:honeycomb") ? item->item().count : 0;
	CHECK_EQ(combs, 3);
	CHECK(bee->getTarget() == player.get());
}

// Snowy plains in the rain: snow covers the ground, still water freezes, a cauldron fills with powder snow; in plains it
// fills with water and nothing freezes
TEST(precipitation_snow_ice_cauldrons) {
	LevelFixture cold(1, "minecraft:overworld", flatOf("minecraft:snowy_plains"));
	cold.world->setWeather(true, false, 1000);
	for (int i = 0; i < 100; i++) cold.world->tickWeather(true); // The rain level ramps up (the server ticks the weather)
	CHECK(cold.level->isRaining());
	cold.level->setBlock({2, Y - 1, 2}, cold.data.getDefaultBlockState("minecraft:water"), Level::UPDATE_ALL);
	cold.level->setBlock({4, Y, 4}, cold.data.getDefaultBlockState("minecraft:cauldron"), Level::UPDATE_ALL);
	for (int i = 0; i < 200; i++) {
		cold.level->tickPrecipitation({0, Y, 0});
		cold.level->tickPrecipitation({2, Y, 2});
		cold.level->tickPrecipitation({4, Y, 4});
	}
	CHECK(cold.nameAt(0, Y, 0).find("minecraft:snow[") == 0);
	CHECK(cold.nameAt(2, Y - 1, 2) == "minecraft:ice");
	CHECK(cold.nameAt(4, Y, 4).find("minecraft:powder_snow_cauldron") == 0);

	LevelFixture warm(1);
	warm.world->setWeather(true, false, 1000);
	for (int i = 0; i < 100; i++) warm.world->tickWeather(true);
	warm.level->setBlock({2, Y - 1, 2}, warm.data.getDefaultBlockState("minecraft:water"), Level::UPDATE_ALL);
	warm.level->setBlock({4, Y, 4}, warm.data.getDefaultBlockState("minecraft:cauldron"), Level::UPDATE_ALL);
	for (int i = 0; i < 200; i++) {
		warm.level->tickPrecipitation({0, Y, 0});
		warm.level->tickPrecipitation({2, Y, 2});
		warm.level->tickPrecipitation({4, Y, 4});
	}
	CHECK(warm.nameAt(0, Y, 0) == "minecraft:air");
	CHECK(warm.nameAt(2, Y - 1, 2).find("minecraft:water") == 0);
	CHECK(warm.nameAt(4, Y, 4).find("minecraft:water_cauldron") == 0);
}

// A tame parrot lands on its owner's shoulder (it leaves the level); hurt, the owner lets it fly off again
TEST(parrot_on_shoulder) {
	LevelFixture f(1);
	auto  player = addPlayer(f, 1.5, 0.5);
	auto* parrot = dynamic_cast<Parrot*>(spawn(f, "minecraft:parrot", {0, Y, 0}));
	CHECK(parrot);
	if (!parrot) return;
	parrot->tame(*player);
	parrot->setOrderedToSit(false);
	parrot->setVariant(3);
	for (int i = 0; i < 120 && !player->survival().shoulders[0]; i++) f.tick(10); // Between its flights
	CHECK(player->survival().shoulders[0].has_value());
	CHECK(player->survival().shoulders[0] && player->survival().shoulders[0]->variant == 3);
	CHECK_EQ(countOf(f, "minecraft:parrot"), 0);
	Combat::damage(f.server, *player, 1.0F, {"minecraft:generic", nullptr, nullptr, std::nullopt});
	CHECK(!player->survival().shoulders[0].has_value());
	CHECK_EQ(countOf(f, "minecraft:parrot"), 1);
}
