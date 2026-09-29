#include "LevelFixture.hpp"
#include "Test.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/PlayerDataStorage.hpp"
#include "world/World.hpp"
#include "world/entity/Geometry.hpp"

#include <cmath>
#include <optional>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	std::shared_ptr<Player> makePlayer(LevelFixture& f) {
		auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		player->setPosition(0.5, Y, 0.5);
		player->setOnGround(true);
		return player;
	}
	// A whole bed with its foot at pos, facing north (its default state), the head at z - 1 (placed like the server places both halves)
	void placeBed(LevelFixture& f, int x, int y, int z) {
		int state = f.data.getDefaultBlockState("minecraft:red_bed");
		f.level->setBlock({x, y, z}, state, Level::UPDATE_ALL);
		f.level->setBlock({x, y, z - 1}, f.data.withProperty(state, "part", "head"), Level::UPDATE_ALL);
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
	// The head's position, even at day (vanilla sets it before checking the time)
	CHECK_EQ(player->spawn().x, 0);
	CHECK_EQ(player->spawn().y, Y);
	CHECK_EQ(player->spawn().z, -1);
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
	int state = f.level->getBlockState({0, Y, 0});
	f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
	// BedBlock.findStandUpPosition: next to the bed (the foot faces north by default: the first spot is to its east)
	std::optional<Combat::RespawnPos> at = Combat::findRespawnAndUseSpawnBlock(*f.level, player->spawn(), true);
	CHECK(at.has_value());
	if (at) {
		double dx = at->position.x - 0.5, dz = at->position.z - 0.5;
		CHECK(std::abs(dx) + std::abs(dz) >= 1.0); // Not on the bed itself
		CHECK_EQ(at->position.y, Y);
	}
}

TEST(respawn_without_bed) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	CHECK(!Combat::findRespawnAndUseSpawnBlock(*f.level, player->spawn(), true).has_value());
}

TEST(respawn_when_bed_missing) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->spawn() = {true, 5, Y, 5, "minecraft:overworld", false}; // A bed that is no longer there
	CHECK(!Combat::findRespawnAndUseSpawnBlock(*f.level, player->spawn(), true).has_value());
}

TEST(respawn_forced_spawnpoint) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->spawn() = {true, 5, Y, 5, "minecraft:overworld", true}; // /spawnpoint: no block needed
	std::optional<Combat::RespawnPos> at = Combat::findRespawnAndUseSpawnBlock(*f.level, player->spawn(), true);
	CHECK(at.has_value());
	if (at) {
		CHECK_EQ(at->position.x, 5.5);
		CHECK_EQ(at->position.y, Y + 0.1);
		CHECK_EQ(at->position.z, 5.5);
	}
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