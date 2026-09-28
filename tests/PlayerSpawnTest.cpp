#include "LevelFixture.hpp"
#include "Test.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/PlayerDataStorage.hpp"
#include "world/World.hpp"
#include "world/entity/Geometry.hpp"

#include <cmath>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	std::shared_ptr<Player> makePlayer(LevelFixture& f) {
		auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		player->setPosition(0.5, Y, 0.5);
		player->setOnGround(true);
		return player;
	}
	// A bed at pos (its default state: beds have properties, so getBlockStateFromName wouldn't find it)
	void placeBed(LevelFixture& f, int x, int y, int z) {
		int state = f.data.getDefaultBlockState("minecraft:red_bed");
		f.level->setBlock({x, y, z}, state, Level::UPDATE_ALL);
	}
} // namespace

TEST(world_spawn_get_set) {
	LevelFixture f;
	World&		 world = *f.world;
	world.setSpawn(100, 64, -200);
	CHECK_EQ(world.getSpawn().x, 100.0);
	CHECK_EQ(world.getSpawn().y, 64.0);
	CHECK_EQ(world.getSpawn().z, -200.0);
}

TEST(bed_sets_respawn_point) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	placeBed(f, 0, Y, 0);
	int state = f.level->getBlockState({0, Y, 0});
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	CHECK(player->spawn().valid);
	CHECK_EQ(player->spawn().x, 0);
	CHECK_EQ(player->spawn().y, Y);
	CHECK_EQ(player->spawn().z, 0);
	CHECK(player->spawn().dimension == "minecraft:overworld");
	CHECK(!player->spawn().forced);
}

TEST(bed_obstructed) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	placeBed(f, 0, Y, 0);
	f.set(0, Y + 1, 0, "minecraft:stone"); // A solid block above the bed
	int state = f.level->getBlockState({0, Y, 0});
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	CHECK(!player->spawn().valid);
}

TEST(respawn_at_bed) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	placeBed(f, 0, Y, 0);
	int	 state   = f.level->getBlockState({0, Y, 0});
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	bool invalid = false;
	Vec3 at	   = Combat::respawnPosition(*f.level, {f.world->getSpawn().x, f.world->getSpawn().y, f.world->getSpawn().z}, player->spawn(), invalid);
	CHECK(!invalid);
	CHECK_EQ(at.x, 0.5);
	CHECK_EQ(at.y, Y);
	CHECK_EQ(at.z, 0.5);
}

TEST(respawn_at_world_spawn_without_bed) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	bool		 invalid = false;
	Vec3		 at		= Combat::respawnPosition(*f.level, {f.world->getSpawn().x, f.world->getSpawn().y, f.world->getSpawn().z}, player->spawn(), invalid);
	CHECK(!invalid);
	CHECK_EQ(at.x, f.world->getSpawn().x);
	CHECK_EQ(at.y, f.world->getSpawn().y);
	CHECK_EQ(at.z, f.world->getSpawn().z);
}

TEST(respawn_at_world_spawn_when_bed_missing) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->spawn() = {true, 5, Y, 5, "minecraft:overworld", false}; // A bed that is no longer there
	bool		 invalid = false;
	Vec3		 at		= Combat::respawnPosition(*f.level, {f.world->getSpawn().x, f.world->getSpawn().y, f.world->getSpawn().z}, player->spawn(), invalid);
	CHECK(invalid);
	CHECK_EQ(at.x, f.world->getSpawn().x);
	CHECK_EQ(at.y, f.world->getSpawn().y);
	CHECK_EQ(at.z, f.world->getSpawn().z);
}

TEST(respawn_at_world_spawn_other_dimension) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->spawn() = {true, 5, Y, 5, "minecraft:the_nether", false}; // A dimension the server doesn't load
	bool		 invalid = false;
	Vec3		 at		= Combat::respawnPosition(*f.level, {f.world->getSpawn().x, f.world->getSpawn().y, f.world->getSpawn().z}, player->spawn(), invalid);
	CHECK(!invalid);
	CHECK_EQ(at.x, f.world->getSpawn().x);
	CHECK_EQ(at.y, f.world->getSpawn().y);
	CHECK_EQ(at.z, f.world->getSpawn().z);
}

TEST(playerdata_spawn_roundtrip) {
	LevelFixture			 f;
	auto					 player = makePlayer(f);
	player->spawn() = {true, 1, 2, 3, "minecraft:overworld", true};
	nbt::TagCompound		 saved  = PlayerData::save(*player, f.data, "minecraft:overworld");
	auto					 loaded = std::make_shared<Player>("Alice", PlayerState::Play, -1, f.server);
	PlayerData::load(*loaded, saved, f.data, GameMode::Survival);
	CHECK(loaded->spawn().valid);
	CHECK_EQ(loaded->spawn().x, 1);
	CHECK_EQ(loaded->spawn().y, 2);
	CHECK_EQ(loaded->spawn().z, 3);
	CHECK(loaded->spawn().dimension == "minecraft:overworld");
	CHECK(loaded->spawn().forced);
}