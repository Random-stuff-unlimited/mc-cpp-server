#include "Test.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/PoiManager.hpp"
#include "world/Portals.hpp"
#include "world/World.hpp"
#include "world/blocks/Fire.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/PrimedTnt.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

namespace {
	// A server with its three dimensions (superflats) in a temporary folder
	struct Dimensions {
		Server				  server;
		std::filesystem::path directory;

		Dimensions() {
			server.loadGameData("resources/gamedata");
			directory = std::filesystem::temp_directory_path() / ("mc-cpp-server-dims-" + std::to_string(reinterpret_cast<uintptr_t>(this)));
			std::filesystem::remove_all(directory);
			World::Settings settings;
			settings.directory = directory;
			settings.ioThreads = 2;
			for (const char* dimension : {"minecraft:overworld", "minecraft:the_nether", "minecraft:the_end"}) server.loadDimension(settings, dimension);
		}
		~Dimensions() {
			for (const auto& level : server.getLevels()) level->world().shutdown();
			std::filesystem::remove_all(directory);
		}
		Level& overworld() { return server.getLevel(); }
		Level& nether() { return *server.getLevel("minecraft:the_nether"); }
		Level& end() { return *server.getLevel("minecraft:the_end"); }
		// A block state by name, the block's default state when no properties are given
		int state(const std::string& name) {
			const GameData& data = server.getGameData();
			return name.find('[') == std::string::npos ? data.getDefaultBlockState(name) : data.getBlockStateFromName(name);
		}
		std::string nameAt(Level& level, const BlockPos& pos) { return server.getGameData().getBlockStateName(level.getBlockState(pos)); }
		// Loads the chunks around a position and keeps them (a ticket each), until they are lit and tick
		void load(Level& level, int x, int z, int radius = 1) {
			World&			 world = level.world();
			std::atomic<int> lit{0};
			for (int cx = (x >> 4) - radius - 1; cx <= (x >> 4) + radius + 1; cx++) {
				for (int cz = (z >> 4) - radius - 1; cz <= (z >> 4) + radius + 1; cz++) world.acquireChunk(cx, cz);
			}
			for (int cx = (x >> 4) - radius; cx <= (x >> 4) + radius; cx++) {
				for (int cz = (z >> 4) - radius; cz <= (z >> 4) + radius; cz++) world.whenLit(cx, cz, [&lit](const std::shared_ptr<Chunk>&) { lit++; });
			}
			int wanted = (2 * radius + 1) * (2 * radius + 1);
			for (int i = 0; i < 1000 && lit < wanted; i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
			for (int cx = (x >> 4) - radius; cx <= (x >> 4) + radius; cx++) {
				for (int cz = (z >> 4) - radius; cz <= (z >> 4) + radius; cz++) level.loadChunkNow(cx, cz);
			}
		}
		// An obsidian frame (4 wide, 5 high) along x from (x, y, z)
		void frame(Level& level, int x, int y, int z) {
			int obsidian = state("minecraft:obsidian");
			for (int i = 0; i < 4; i++) {
				level.setBlock({x + i, y, z}, obsidian, Level::UPDATE_ALL);
				level.setBlock({x + i, y + 4, z}, obsidian, Level::UPDATE_ALL);
			}
			for (int j = 1; j < 4; j++) {
				level.setBlock({x, y + j, z}, obsidian, Level::UPDATE_ALL);
				level.setBlock({x + 3, y + j, z}, obsidian, Level::UPDATE_ALL);
			}
		}
	};
} // namespace

TEST(dimensions_load_with_their_types) {
	Dimensions d;
	CHECK(d.server.getLevels().size() == 3);
	CHECK(d.nether().dimensionType().ultraWarm);
	CHECK(!d.nether().dimensionType().bedWorks);
	CHECK(d.end().minY() == 0);
	CHECK(d.overworld().minY() == -64);
	// The nether and the end run on the overworld's clock
	d.overworld().world().tickTime();
	CHECK_EQ(d.nether().getGameTime(), d.overworld().getGameTime());
	// Superflat nether: netherrack up to y = 63 on a bedrock floor
	d.load(d.nether(), 0, 0);
	CHECK(d.nameAt(d.nether(), {0, 63, 0}) == "minecraft:netherrack");
	CHECK(d.nameAt(d.nether(), {0, 0, 0}) == "minecraft:bedrock");
}

TEST(fire_lights_a_nether_portal) {
	Dimensions d;
	Level&	   level = d.overworld();
	d.load(level, 0, 0);
	int y = -60;
	d.frame(level, 0, y, 0);
	level.setBlock({1, y + 1, 0}, BaseFireBlock::getState(level, {1, y + 1, 0}), Level::UPDATE_ALL);
	for (int i = 1; i <= 2; i++) {
		for (int j = 1; j <= 3; j++) CHECK(d.nameAt(level, {i, y + j, 0}) == "minecraft:nether_portal[axis=x]");
	}
	// The portal blocks are points of interest; breaking the frame breaks the portal
	CHECK_EQ(level.poi().getInSquare(PoiManager::Type::NetherPortal, {0, y, 0}, 16, PoiManager::Occupancy::Any).size(), size_t(6));
	level.setBlock({0, y + 2, 0}, d.state("minecraft:air"), Level::UPDATE_ALL);
	CHECK(d.nameAt(level, {1, y + 2, 0}) == "minecraft:air");
	CHECK(level.poi().getInSquare(PoiManager::Type::NetherPortal, {0, y, 0}, 16, PoiManager::Occupancy::Any).empty());
}

TEST(entity_goes_to_the_nether_and_a_portal_is_made) {
	Dimensions d;
	Level&	   level = d.overworld();
	d.load(level, 0, 0);
	int y = -60;
	d.frame(level, 0, y, 0);
	level.setBlock({1, y + 1, 0}, BaseFireBlock::getState(level, {1, y + 1, 0}), Level::UPDATE_ALL);
	// An item in the portal: goes through at its next base tick
	auto  item	  = ItemEntity::create(level, {1.5, y + 1.0, 0.5}, ItemStack(d.server.getGameData().getStaticId("minecraft:item", "minecraft:stone"), 1));
	Entity* added = level.addFreshEntity(std::move(item));
	Portals::setAsInsidePortal(*added, Portals::Kind::Nether, {1, y + 1, 0});
	Portals::handlePortal(level, *added);
	CHECK(added->isRemoved());
	// The nether got the copy, and a portal at 1/8 of the coordinates
	int found = 0;
	d.nether().entities().processPendingLoads();
	d.nether().entities().forEach([&](Entity& entity) {
		if (dynamic_cast<ItemEntity*>(&entity) && !entity.isRemoved()) found++;
	});
	CHECK_EQ(found, 1);
	CHECK(!d.nether().poi().getInSquare(PoiManager::Type::NetherPortal, {0, 64, 0}, 32, PoiManager::Occupancy::Any).empty());
}

TEST(tnt_explodes_and_breaks_blocks) {
	Dimensions d;
	Level&	   level = d.overworld();
	d.load(level, 0, 0);
	// Dirt all around the TNT, which is primed by a redstone block next to it
	for (int x = -2; x <= 2; x++) {
		for (int z = -2; z <= 2; z++) level.setBlock({x, -60, z}, d.state("minecraft:dirt"), Level::UPDATE_ALL);
	}
	level.setBlock({0, -59, 0}, d.state("minecraft:tnt"), Level::UPDATE_ALL);
	level.setBlock({1, -59, 0}, d.state("minecraft:redstone_block"), Level::UPDATE_ALL);
	CHECK(d.nameAt(level, {0, -59, 0}) == "minecraft:air");
	PrimedTnt* tnt = nullptr;
	level.entities().forEach([&](Entity& entity) {
		if (auto* primed = dynamic_cast<PrimedTnt*>(&entity)) tnt = primed;
	});
	CHECK(tnt != nullptr);
	for (int i = 0; i < 90; i++) level.tickEntities();
	CHECK(d.nameAt(level, {0, -60, 0}) == "minecraft:air"); // Right under it
	CHECK(d.nameAt(level, {0, -64, 0}) == "minecraft:bedrock");
}

TEST(bed_explodes_in_the_nether) {
	Dimensions d;
	Level&	   level = d.nether();
	d.load(level, 0, 0);
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, d.server);
	player->setLevel(&level);
	player->setPosition(0.5, 64, 2.5);
	int foot = d.server.getGameData().getDefaultBlockState("minecraft:red_bed");
	level.setBlock({0, 64, 0}, foot, Level::UPDATE_ALL);
	level.setBlock({0, 64, -1}, d.server.getGameData().withProperty(foot, "part", "head"), Level::UPDATE_ALL);
	level.behavior(foot).useWithoutItem(level, {0, 64, 0}, foot, *player);
	CHECK(!player->spawn().valid);
	CHECK(d.nameAt(level, {0, 64, -1}) != "minecraft:red_bed[facing=north,occupied=false,part=head]");
	CHECK(d.nameAt(level, {0, 63, 0}) == "minecraft:air"); // The netherrack under it blew up
}
