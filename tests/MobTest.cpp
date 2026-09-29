#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/Chunk.hpp"
#include "world/ChunkStorage.hpp"
#include "world/Combat.hpp"
#include "world/entity/Mob.hpp"
#include "world/item/SpawnEggItem.hpp"

#include <chrono>
#include <cmath>
#include <iostream>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // Top of the grass, first air layer

	int type(LevelFixture& f, const char* name) { return f.data.getStaticId("minecraft:entity_type", name); }

	// Without its goals, at night: these tests are about the physics, not the AI (MobAiTest) or sun burning
	Mob* spawn(LevelFixture& f, const char* name, const BlockPos& pos) {
		f.world->setDayTime(18000);
		Mob* mob = f.level->mobs().spawn(*f.level, type(f, name), pos, MobRegistry::SpawnReason::Command);
		if (mob) {
			mob->setBaby(false); // Zombies are babies 5% of the time
			mob->goalSelector().removeAllGoals();
			mob->targetSelector().removeAllGoals();
		}
		return mob;
	}

	// The block's default state
	void place(LevelFixture& f, int x, int y, int z, const char* block) {
		f.level->setBlock({x, y, z}, f.data.getDefaultBlockState(block), Level::UPDATE_ALL);
	}

	std::vector<Mob*> mobs(LevelFixture& f) {
		std::vector<Mob*> found;
		f.level->entities().forEach([&](Entity& entity) {
			if (auto* mob = dynamic_cast<Mob*>(&entity); mob && !mob->isRemoved()) found.push_back(mob);
		});
		return found;
	}
} // namespace

// Every mob type gets its size and default attributes from the game data
TEST(mob_types_have_vanilla_size_and_attributes) {
	LevelFixture f(1);
	Mob* zombie = spawn(f, "minecraft:zombie", {0, Y, 0});
	Mob* cow	= spawn(f, "minecraft:cow", {3, Y, 0});
	CHECK(zombie && cow);
	if (!zombie || !cow) return;
	CHECK_EQ(zombie->health(), 20.0F);
	CHECK_EQ(cow->health(), 10.0F);
	CHECK(std::abs(zombie->width() - 0.6F) < 1e-6 && std::abs(zombie->height() - 1.95F) < 1e-6);
	CHECK(std::abs(cow->width() - 0.9F) < 1e-6 && std::abs(cow->height() - 1.4F) < 1e-6);
	const AttributeIds& ids = zombie->attributeIds();
	CHECK(std::abs(zombie->getAttributeValue(ids.movementSpeed) - 0.23) < 1e-6);
	CHECK_EQ(zombie->getAttributeValue(ids.armor), 2.0);
	CHECK_EQ(zombie->getAttributeValue(ids.attackDamage), 3.0);
	// finalizeSpawn: follow range 35 with a random bonus of at most ±11.485%
	double follow = zombie->getAttributeValue(ids.followRange);
	CHECK(follow >= 35.0 * (1 - 0.115) && follow <= 35.0 * (1 + 0.115));
	CHECK(!cow->attributes().hasAttribute(ids.attackDamage));
	// Every mob type can be made
	int made = 0;
	for (int id = 0; const GameData::EntityTypeInfo* info = f.data.getEntityType(id); id++) {
		if (!info->mob) continue;
		std::unique_ptr<Mob> mob = f.level->mobs().create(*f.level, id);
		made += mob != nullptr;
	}
	CHECK(made >= 80);
}

// Spawned in the air, a mob falls and rests on the ground: its physics then stop until something changes
TEST(mob_falls_and_rests_on_ground) {
	LevelFixture f(1);
	Mob* zombie = spawn(f, "minecraft:zombie", {0, Y + 2, 0});
	CHECK(zombie);
	if (!zombie) return;
	f.tick(40);
	CHECK(zombie->onGround());
	CHECK_EQ(zombie->position().y, static_cast<double>(Y));
	CHECK_EQ(zombie->position().x, 0.5);
	CHECK_EQ(zombie->health(), 20.0F); // A 2 block fall doesn't hurt
	CHECK(zombie->isResting());
	CHECK(std::abs(zombie->deltaMovement().y - (-0.0784)) < 1e-6);

	// The block under it goes: it falls again
	f.set(0, Y - 1, 0, "minecraft:air");
	f.tick(1);
	CHECK(!zombie->isResting());
	f.tick(30);
	CHECK(zombie->onGround());
	CHECK_EQ(zombie->position().y, static_cast<double>(Y - 1));
}

// A long fall hurts: fall distance minus the 3 safe blocks
TEST(mob_takes_fall_damage) {
	LevelFixture f(1);
	Mob* cow = spawn(f, "minecraft:cow", {0, Y + 10, 0});
	CHECK(cow);
	if (!cow) return;
	f.tick(60);
	CHECK(cow->onGround());
	// About 10 blocks: floor(10 - 3) = 7 damage
	CHECK_EQ(cow->health(), 3.0F);
	CHECK_EQ(cow->hurtTime(), 0); // Flashed on landing, long ago
}

// A player's hit hurts (damage of the hand), flashes it and knocks it back away from the player
TEST(mob_hurt_by_player_attack) {
	LevelFixture f(1);
	Mob* zombie = spawn(f, "minecraft:zombie", {0, Y, 0});
	CHECK(zombie);
	if (!zombie) return;
	f.tick(5);
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setPosition(2.5, Y, 0.5);
	player->setLastAttack(-100); // Fully charged
	Combat::attack(f.server, *player, *zombie);
	// 1 (hand) after the zombie's 2 armor points: 1 * (1 - clamp(2 - 1/2, 0.4, 20) / 25) = 0.94
	CHECK(std::abs(zombie->health() - (20.0F - 0.94F)) < 1e-4);
	CHECK_EQ(zombie->hurtTime(), 10);
	CHECK_EQ(zombie->invulnerableTime(), 20);
	CHECK(zombie->deltaMovement().x < -0.1); // Away from the player (at +x)
	CHECK(zombie->deltaMovement().y > 0.1);
	CHECK_EQ(zombie->lastHurtByPlayer(), player->getPlayerID());
	// Hit again at once: invulnerable, only more damage than the last hit counts
	player->setLastAttack(-100);
	float health = zombie->health();
	Combat::attack(f.server, *player, *zombie);
	CHECK_EQ(zombie->health(), health);
	f.tick(30);
	CHECK(zombie->onGround());
	CHECK(zombie->position().x < 0.3);
}

// At 0 health it dies: the death animation, its loot, removed 20 ticks later
TEST(mob_dies_and_drops_loot) {
	LevelFixture f(1);
	Mob* cow = spawn(f, "minecraft:cow", {0, Y, 0});
	CHECK(cow);
	if (!cow) return;
	f.tick(2);
	CHECK(cow->hurtServer({"minecraft:generic", nullptr}, 100.0F));
	CHECK(cow->isDeadOrDying());
	// Beef (1 to 3) always, leather sometimes
	int beef = 0;
	for (ItemEntity* item : f.items()) {
		if (item->item().item == f.item("minecraft:beef")) beef += item->item().count;
		CHECK_EQ(item->pickupDelay(), 10);
	}
	CHECK(beef >= 1 && beef <= 3);
	CHECK(!cow->hurtServer({"minecraft:generic", nullptr}, 1.0F)); // Already dead
	f.tick(19);
	CHECK_EQ(mobs(f).size(), size_t(1));
	CHECK_EQ(cow->deathTime(), 19);
	f.tick(1);
	CHECK_EQ(mobs(f).size(), size_t(0));
}

// A cow on fire drops cooked beef (the loot table's furnace_smelt)
TEST(mob_on_fire_drops_cooked_loot) {
	LevelFixture f(1);
	place(f, 0, Y, 0, "minecraft:fire");
	Mob* cow = spawn(f, "minecraft:cow", {0, Y, 0});
	CHECK(cow);
	if (!cow) return;
	f.tick(2);
	CHECK(cow->isOnFire());
	CHECK(cow->health() < 10.0F);
	cow->hurtServer({"minecraft:generic", nullptr}, 100.0F);
	int cooked = 0;
	for (ItemEntity* item : f.items()) cooked += item->item().item == f.item("minecraft:cooked_beef");
	CHECK(cooked > 0);
}

// Mobs are saved with their chunk and come back when it loads again
TEST(mob_survives_chunk_save_and_load) {
	LevelFixture f(1);
	Mob* zombie = spawn(f, "minecraft:zombie", {3, Y + 1, 5});
	CHECK(zombie);
	if (!zombie) return;
	f.tick(20);
	zombie->hurtServer({"minecraft:generic", nullptr}, 5.0F);
	zombie->setNoAi(true);
	f.tick(1);
	UUID  uuid	   = zombie->uuid();
	Vec3  position = zombie->position();
	float health   = zombie->health();
	float yRot	   = zombie->yRot();
	double follow  = zombie->getAttributeValue(zombie->attributeIds().followRange);

	f.level->saveEntities();
	std::shared_ptr<Chunk> chunk = f.world->loadedChunk(0, 0);
	CHECK(chunk && !chunk->savedEntities().empty() && chunk->isDirty());
	if (!chunk) return;

	// Through the region file format
	GameData&				  data = f.data;
	std::filesystem::path	  directory = f.directory / "storage-test";
	PalettedContainer::Config blockConfig{4096, 4, 8, PalettedContainer::bitsFor(static_cast<uint32_t>(data.getBlockStateCount()))};
	PalettedContainer::Config biomeConfig{64, 1, 3, 6};
	uint32_t				  air = static_cast<uint32_t>(data.getDefaultBlockState("minecraft:air"));
	ChunkStorage::Layout	  layout{-64, 24, &blockConfig, &biomeConfig, air, 0};
	DiskPalette				  blocks(
			  directory / "blocks.txt", data.getBlockStateCount(), [&](uint32_t id) { return data.getBlockStateName(static_cast<int>(id)); },
			  [&](const std::string& name) { return data.getBlockStateFromName(name); }, air);
	DiskPalette biomes(
			directory / "biomes.txt", 64, [](uint32_t id) { return "biome" + std::to_string(id); },
			[](const std::string& name) { return std::stoi(name.substr(5)); }, 0);
	ChunkStorage::TickTypes types{[&](int id) { return data.getStaticName("minecraft:block", id); },
								  [&](int id) { return data.getStaticName("minecraft:fluid", id); },
								  [&](const std::string& name) { return data.getStaticId("minecraft:block", name); },
								  [&](const std::string& name) { return data.getStaticId("minecraft:fluid", name); }};
	ChunkStorage storage(directory, layout, blocks, biomes, types, &data);
	Chunk		 copy(0, 0, -64, 24, blockConfig, biomeConfig, air, 0);
	copy.savedEntities() = chunk->savedEntities();
	storage.write(0, 0, storage.encode(copy, 0));
	std::unique_ptr<Chunk> loaded = storage.load(0, 0);
	CHECK(loaded && loaded->savedEntities() == chunk->savedEntities());

	// The chunk leaves the level: the zombie goes with it; back with the chunk
	f.level->entities().unloadChunk(Chunk::key(0, 0));
	f.tick(1);
	CHECK_EQ(mobs(f).size(), size_t(0));
	f.level->entities().queueLoad(loaded ? loaded->savedEntities() : chunk->savedEntities());
	f.tick(1);
	std::vector<Mob*> back = mobs(f);
	CHECK_EQ(back.size(), size_t(1));
	if (back.empty()) return;
	Mob* again = back[0];
	CHECK(again->typeId() == type(f, "minecraft:zombie"));
	CHECK(again->uuid().getMostSigBits() == uuid.getMostSigBits() && again->uuid().getLeastSigBits() == uuid.getLeastSigBits());
	CHECK_EQ(again->position().x, position.x);
	CHECK_EQ(again->position().y, position.y);
	CHECK_EQ(again->position().z, position.z);
	CHECK_EQ(again->health(), health);
	CHECK_EQ(again->yRot(), yRot);
	CHECK(again->isNoAi());
	// The follow range bonus of finalizeSpawn is a permanent modifier: saved
	CHECK_EQ(again->getAttributeValue(again->attributeIds().followRange), follow);
}

// Using a spawn egg on the top of a block puts its mob standing on it; one egg is used, none in creative
TEST(mob_spawn_egg_on_block) {
	LevelFixture f(1);
	auto	  player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	ItemStack egg(f.item("minecraft:zombie_spawn_egg"), 2);
	Mob*	  zombie = SpawnEggItem::useOn(*f.level, player.get(), egg, {0, Y - 1, 0}, Direction::Up);
	CHECK(zombie);
	if (!zombie) return;
	CHECK(zombie->typeId() == type(f, "minecraft:zombie"));
	CHECK_EQ(zombie->position().y, static_cast<double>(Y));
	CHECK_EQ(zombie->position().x, 0.5);
	CHECK_EQ(egg.count, 1);
	CHECK_EQ(zombie->yHeadRot(), zombie->yRot());

	// On a slab's side: in front of it; on tall grass (no collision): in it, on the ground
	player->setGameMode(GameMode::Creative);
	ItemStack cowEgg(f.item("minecraft:cow_spawn_egg"), 1);
	place(f, 4, Y, 0, "minecraft:short_grass");
	Mob* cow = SpawnEggItem::useOn(*f.level, player.get(), cowEgg, {4, Y, 0}, Direction::North);
	CHECK(cow && cow->typeId() == type(f, "minecraft:cow"));
	if (cow) CHECK_EQ(cow->position().y, static_cast<double>(Y));
	CHECK_EQ(cowEgg.count, 1);
	// Not an egg
	ItemStack stone(f.item("minecraft:stone"), 1);
	CHECK(SpawnEggItem::useOn(*f.level, player.get(), stone, {0, Y - 1, 0}, Direction::Up) == nullptr);
}

// Under water a cow loses air, then drowns; zombies (undead) breathe water. In lava it burns
TEST(mob_drowns_and_burns) {
	LevelFixture f(1);
	for (int y = Y; y < Y + 4; y++) place(f, 0, y, 0, "minecraft:water");
	Mob* cow	= spawn(f, "minecraft:cow", {0, Y, 0});
	CHECK(cow);
	if (!cow) return;
	f.tick(40);
	CHECK_EQ(cow->airSupply(), 300 - 40);
	f.tick(300 - 40 + 20);
	CHECK_EQ(cow->health(), 8.0F); // Out of air: 2 damage
	CHECK_EQ(cow->airSupply(), 0);

	LevelFixture g(1);
	place(g, 0, Y, 0, "minecraft:lava");
	Mob* burning = spawn(g, "minecraft:cow", {0, Y, 0});
	Mob* zombie	 = spawn(g, "minecraft:zombie", {5, Y, 0});
	CHECK(burning && zombie);
	if (!burning || !zombie) return;
	g.tick(1);
	CHECK(burning->isOnFire());
	CHECK_EQ(burning->health(), 6.0F); // Lava: 4
	CHECK(!zombie->isOnFire());
}

// Cost of idle mobs: 1000 cows resting on the ground, spread over 5x5 chunks
TEST(mob_idle_cost_per_1000) {
	LevelFixture f(2);
	int	 cowType = type(f, "minecraft:cow");
	for (int i = 0; i < 1000; i++) {
		int x = (i % 40) * 2 - 32, z = (i / 40) * 2 - 32;
		Mob* cow = f.level->mobs().spawn(*f.level, cowType, {x, Y, z}, MobRegistry::SpawnReason::Command);
		cow->setBaby(false);
		cow->goalSelector().removeAllGoals(); // The physics alone: no wandering
	}
	f.tick(20);
	int resting = 0;
	for (Mob* mob : mobs(f)) resting += mob->isResting();
	CHECK(resting >= 990);
	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < 100; i++) {
		f.level->tickEntities();
		f.level->sendEntityChanges();
	}
	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 100.0;
	// The same mobs kept awake (a tiny change of movement each tick): the full physics of a standing mob
	std::vector<Mob*> all = mobs(f);
	start				  = std::chrono::steady_clock::now();
	for (int i = 0; i < 100; i++) {
		for (Mob* mob : all) mob->setDeltaMovement({0.0, -0.0784 - 1.0E-9 * (i % 2), 0.0});
		f.level->tickEntities();
		f.level->sendEntityChanges();
	}
	double awake = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 100.0;
	std::cout << "    1000 idle mobs: " << ms << " ms per tick resting (" << resting << " resting), " << awake << " ms awake\n";
	CHECK(ms < 50.0);
}
