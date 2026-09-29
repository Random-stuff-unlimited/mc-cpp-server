#ifndef LEVEL_FIXTURE_HPP
#define LEVEL_FIXTURE_HPP

#include "data/GameData.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/World.hpp"
#include "world/entity/ItemEntity.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

// A real World and Level on a flat world (bedrock, 2 dirt, grass: the surface is at y = -61, air from -60) in a
// temporary folder, with the chunks around 0, 0 loaded and ticking. Ticks are run by hand, in vanilla's order
struct LevelFixture {
	static constexpr int SURFACE = -61;

	GameData				data;
	Server					server;
	std::filesystem::path	directory;
	std::unique_ptr<World>	world;
	std::unique_ptr<Level>	level;
	int						radius;

	explicit LevelFixture(int chunkRadius = 2) : radius(chunkRadius) {
		data.load("resources/gamedata");
		directory = std::filesystem::temp_directory_path() / ("mc-cpp-server-test-" + std::to_string(reinterpret_cast<uintptr_t>(this)));
		std::filesystem::remove_all(directory);
		World::Settings settings;
		settings.directory = directory;
		settings.ioThreads = 2;
		world			   = std::make_unique<World>(data, settings);

		// Chunks one ring further, so every chunk in the radius is lit (and ticks)
		std::atomic<int> lit{0};
		for (int x = -radius - 1; x <= radius + 1; x++) {
			for (int z = -radius - 1; z <= radius + 1; z++) world->acquireChunk(x, z);
		}
		for (int x = -radius; x <= radius; x++) {
			for (int z = -radius; z <= radius; z++) world->whenLit(x, z, [&lit](const std::shared_ptr<Chunk>&) { lit++; });
		}
		int wanted = (2 * radius + 1) * (2 * radius + 1);
		for (int i = 0; i < 1000 && lit < wanted; i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
		level = std::make_unique<Level>(server, *world, data);
		level->setRandomTickSpeed(0); // Tests that want random ticks ask for them
		level->setMobSpawning(false); // Same for natural spawning
		// The server's chunk load listener attaches them to the level: here, by hand
		for (int x = -radius - 1; x <= radius + 1; x++) {
			for (int z = -radius - 1; z <= radius + 1; z++) level->loadedChunk(x, z);
		}
	}

	~LevelFixture() {
		level.reset();
		world.reset();
		std::filesystem::remove_all(directory);
	}

	// The player joins the game in this level (Server::addGamePlayer, with the level set as enterPlay does)
	void addPlayer(const std::shared_ptr<Player>& player) {
		player->setLevel(level.get());
		server.addGamePlayer(player);
	}

	int state(const std::string& name) const { return data.getBlockStateFromName(name); }
	int block(const std::string& name) const { return data.getStaticId("minecraft:block", name); }
	int at(int x, int y, int z) { return level->getBlockState({x, y, z}); }
	std::string nameAt(int x, int y, int z) { return data.getBlockStateName(at(x, y, z)); }
	void set(int x, int y, int z, const std::string& name) { level->setBlock({x, y, z}, state(name), Level::UPDATE_ALL); }
	int	 item(const std::string& name) const { return data.getStaticId("minecraft:item", name); }
	// The item entities, as "item id x count"
	std::vector<ItemEntity*> items() {
		std::vector<ItemEntity*> found;
		level->entities().forEach([&](Entity& entity) {
			if (auto* item = dynamic_cast<ItemEntity*>(&entity); item && !item->isRemoved()) found.push_back(item);
		});
		return found;
	}

	// A random tick of the block at this position, as if chosen by tickChunk
	void randomTick(int x, int y, int z) {
		int state = at(x, y, z);
		if (data.getStateProperties(state).randomTicking) level->behaviors().randomTicker(data.getBlocks().blockOf(state)).randomTick(*level, {x, y, z}, state);
	}

	// One game tick: the phases of Server::tick that concern blocks
	void tick(int count = 1) {
		for (int i = 0; i < count; i++) {
			level->setHandlingTick(true);
			level->updateSkyBrightness();
			world->tickTime();
			level->tickScheduled();
			level->tickChunks();
			level->runBlockEvents();
			level->setHandlingTick(false);
			level->tickEntities();
			level->tickBlockEntities();
			level->sendChanges();
			level->sendEntityChanges();
		}
	}
};

#endif
