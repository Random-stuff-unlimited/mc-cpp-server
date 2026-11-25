#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/entity/LightningBolt.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/mobs/Creeper.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/entity/mobs/Horses.hpp"

// Lightning: what it does to mobs and players, fire, lightning rods

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	Mob* spawnAt(LevelFixture& f, const char* type, BlockPos pos) {
		Mob* mob = f.level->mobs().spawn(*f.level, f.data.getStaticId("minecraft:entity_type", type), pos, MobRegistry::SpawnReason::Command);
		if (mob) {
			mob->goalSelector().removeAllGoals();
			mob->targetSelector().removeAllGoals();
		}
		return mob;
	}

	void strike(LevelFixture& f, double x, double z) {
		auto bolt = std::make_unique<LightningBolt>(*f.level);
		bolt->snapTo({x, static_cast<double>(Y), z}, 0.0F, 0.0F);
		f.level->entities().add(std::move(bolt));
	}

	int countType(LevelFixture& f, const char* type) {
		int id = f.data.getStaticId("minecraft:entity_type", type), count = 0;
		f.level->entities().forEach([&](Entity& entity) {
			if (entity.typeId() == id && !entity.isRemoved()) count++;
		});
		return count;
	}
} // namespace

// Creepers get charged, pigs become zombified piglins, villagers witches, a mooshroom changes color once; fire is
// left where it struck (normal difficulty); a player nearby is hurt and set on fire
TEST(lightning_strikes_mobs) {
	LevelFixture f(2);
	auto* creeper = dynamic_cast<Creeper*>(spawnAt(f, "minecraft:creeper", {0, Y, 0}));
	spawnAt(f, "minecraft:pig", {1, Y, 0});
	spawnAt(f, "minecraft:villager", {0, Y, 1});
	auto* mooshroom = dynamic_cast<Mooshroom*>(spawnAt(f, "minecraft:mooshroom", {-1, Y, 0}));
	CHECK(creeper && mooshroom);
	if (!creeper || !mooshroom) return;
	auto player = std::make_shared<Player>("Unlucky", PlayerState::Play, -1, f.server);
	player->setPosition(1.5, Y, 1.5);
	player->setGameMode(GameMode::Survival);
	f.addPlayer(player);
	int variant = mooshroom->variant();
	strike(f, 0.5, 0.5);
	f.tick(30);
	CHECK(creeper->isPowered());
	CHECK_EQ(countType(f, "minecraft:pig"), 0);
	CHECK_EQ(countType(f, "minecraft:zombified_piglin"), 1);
	CHECK_EQ(countType(f, "minecraft:villager"), 0);
	CHECK_EQ(countType(f, "minecraft:witch"), 1);
	CHECK(mooshroom->variant() != variant); // Changed once, not back with each tick of the bolt
	CHECK(player->combat().health < 20.0F);
	CHECK_EQ(countType(f, "minecraft:lightning_bolt"), 0); // Gone after its flashes
	bool fire = false;
	for (int x = -2; x <= 2; x++) {
		for (int z = -2; z <= 2; z++) {
			for (int y = Y - 1; y <= Y + 1; y++) fire |= f.nameAt(x, y, z).find("minecraft:fire") == 0;
		}
	}
	CHECK(fire);
}

// A lightning rod that is the top of its column draws the bolts within 128 blocks, and gives a signal for 8 ticks
TEST(lightning_rod) {
	LevelFixture f(2);
	f.level->setBlock({10, Y, 10}, f.data.getDefaultBlockState("minecraft:lightning_rod"), Level::UPDATE_ALL);
	f.tick(1); // The rod's point of interest
	BlockPos target = LightningBolt::findTarget(*f.level, {-5, 0, -5});
	CHECK(target.x == 10 && target.y == Y + 1 && target.z == 10);
	auto bolt = std::make_unique<LightningBolt>(*f.level);
	bolt->snapTo({10.5, static_cast<double>(Y + 1), 10.5}, 0.0F, 0.0F);
	f.level->entities().add(std::move(bolt));
	f.tick(2);
	CHECK(f.data.getProperty(f.at(10, Y, 10), "powered") == "true");
	f.tick(10);
	CHECK(f.data.getProperty(f.at(10, Y, 10), "powered") == "false");
	// Without a rod: the top of the column
	f.set(10, Y, 10, "minecraft:air");
	f.tick(1);
	BlockPos open = LightningBolt::findTarget(*f.level, {-20, 0, -20});
	CHECK(open.y == Y);
}

// A skeleton trap horse: a player coming near sets it off (tame, 3 more horses, 4 armed skeletons)
TEST(skeleton_trap_horse) {
	LevelFixture f(2);
	std::unique_ptr<Mob> mob = f.level->mobs().create(*f.level, f.data.getStaticId("minecraft:entity_type", "minecraft:skeleton_horse"));
	auto* horse = dynamic_cast<UndeadHorse*>(mob.get());
	CHECK(horse);
	if (!horse) return;
	horse->setTrap(true);
	horse->snapTo({0.5, static_cast<double>(Y), 0.5}, 0.0F, 0.0F);
	f.level->addFreshEntity(std::move(mob));
	auto player = std::make_shared<Player>("Victim", PlayerState::Play, -1, f.server);
	player->setPosition(20.5, Y, 0.5);
	player->setGameMode(GameMode::Survival);
	f.addPlayer(player);
	f.tick(5);
	CHECK(horse->isTrap()); // Too far
	player->setPosition(5.5, Y, 0.5);
	f.tick(5);
	CHECK(!horse->isTrap() && horse->isTamed());
	CHECK_EQ(countType(f, "minecraft:skeleton_horse"), 4);
	CHECK_EQ(countType(f, "minecraft:skeleton"), 4);
	int ridden = 0;
	f.level->entities().forEach([&](Entity& entity) {
		if (entity.typeId() == f.data.getStaticId("minecraft:entity_type", "minecraft:skeleton_horse") && !entity.entityPassengers().empty()) ridden++;
	});
	CHECK_EQ(ridden, 4); // Each horse has its skeleton on it
}

// Lightning on oxidized copper: back to plain copper there, and some of the copper around loses a step
TEST(lightning_clears_copper) {
	LevelFixture f(1);
	for (int x = -2; x <= 2; x++) {
		for (int z = -2; z <= 2; z++) f.level->setBlock({x, Y - 1, z}, f.data.getDefaultBlockState("minecraft:oxidized_copper"), Level::UPDATE_ALL);
	}
	strike(f, 0.5, 0.5);
	f.tick(2);
	CHECK(f.nameAt(0, Y - 1, 0) == "minecraft:copper_block");
	int cleaned = 0;
	for (int x = -2; x <= 2; x++) {
		for (int z = -2; z <= 2; z++) cleaned += f.nameAt(x, Y - 1, z) != "minecraft:oxidized_copper";
	}
	CHECK(cleaned > 1);
}
