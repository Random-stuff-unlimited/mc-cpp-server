#include "Test.hpp"
#include "data/GameData.hpp"
#include "world/ChunkStorage.hpp"

#include <filesystem>

// Blocks, scheduled ticks and comparator outputs survive a save and a load
TEST(chunk_storage_keeps_blocks_and_ticks) {
	GameData data;
	data.load("resources/gamedata");
	std::filesystem::path directory = std::filesystem::temp_directory_path() / "mc-cpp-server-test-storage";
	std::filesystem::remove_all(directory);

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
	ChunkStorage storage(directory, layout, blocks, biomes, types);

	int	  repeater = data.getStaticId("minecraft:block", "minecraft:repeater");
	int	  water	   = data.getStaticId("minecraft:fluid", "minecraft:water");
	Chunk chunk(3, -2, -64, 24, blockConfig, biomeConfig, air, 0);
	int	  stone = data.getDefaultBlockState("minecraft:stone");
	chunk.setBlock(5, 70, 9, static_cast<uint32_t>(stone));
	chunk.blockTicks().schedule({repeater, {3 * 16 + 5, 71, -2 * 16 + 9}, 104, HIGH, 0});
	chunk.fluidTicks().schedule({water, {3 * 16 + 1, -60, -2 * 16 + 15}, 99, NORMAL, 1});
	chunk.comparatorOutputs()[(70 + 64) << 8 | 9 << 4 | 5] = 13;
	storage.write(3, -2, storage.encode(chunk, 100));

	std::unique_ptr<Chunk> loaded = storage.load(3, -2);
	CHECK(loaded != nullptr);
	if (!loaded) return;
	CHECK_EQ(static_cast<int>(loaded->getBlock(5, 70, 9)), stone);
	CHECK_EQ(int(loaded->comparatorOutputs()[(70 + 64) << 8 | 9 << 4 | 5]), 13);
	loaded->blockTicks().unpack(500);
	loaded->fluidTicks().unpack(500);
	const ScheduledTick* block = loaded->blockTicks().peek();
	const ScheduledTick* fluid = loaded->fluidTicks().peek();
	CHECK(block && block->type == repeater && block->pos == (BlockPos{53, 71, -23}) && block->triggerTick == 504 && block->priority == HIGH);
	CHECK(fluid && fluid->type == water && fluid->pos == (BlockPos{49, -60, -17}) && fluid->triggerTick == 499);
	std::filesystem::remove_all(directory);
}
