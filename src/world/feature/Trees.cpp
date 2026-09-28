#include "world/feature/Trees.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "world/Level.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/entity/Geometry.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

using json = nlohmann::json;

// ----- TreeBlocks, TreeFeatures, SaplingTreeGrower -----

// ----- TreeFeatures -----

TreeBlocks::TreeBlocks(const BlockContext& context) {
	replaceableByTrees = context.tag("minecraft:replaceable_by_trees");
	logs			   = context.tag("minecraft:logs");
	leavesTag		   = context.tag("minecraft:leaves");
	dirt			   = context.tag("minecraft:dirt");
	leavesBlocks.assign(context.data.getBlockCount(), false);
	for (size_t block = 0; block < leavesBlocks.size(); block++) leavesBlocks[block] = context.data.isInstanceOf(static_cast<int>(block), "LeavesBlock");
	vine	   = context.block("minecraft:vine");
	grass	   = context.block("minecraft:grass_block");
	mycelium   = context.block("minecraft:mycelium");
	beeNest	   = context.block("minecraft:bee_nest");
	axis	   = context.blocks.property("axis");
	axes[0]	   = context.blocks.value("x");
	axes[1]	   = context.blocks.value("y");
	axes[2]	   = context.blocks.value("z");
	persistent = context.blocks.property("persistent");
	distance   = context.blocks.property("distance");
}

TreeFeatures::TreeFeatures(const std::filesystem::path& file, const BlockContext& context) : _blocks(context) {
	std::ifstream in(file);
	if (!in) throw std::runtime_error("cannot open " + file.string());
	json trees = json::parse(in);
	for (auto& [name, config] : trees.items()) _trees[name] = std::make_unique<TreeFeature>(config, context, _blocks);
}

const TreeFeature* TreeFeatures::get(const std::string& name) const {
	auto it = _trees.find(name);
	return it == _trees.end() ? nullptr : it->second.get();
}

// ----- SaplingTreeGrower -----

std::shared_ptr<const SaplingTreeGrower> SaplingTreeGrower::forSapling(const std::string& sapling, std::shared_ptr<const TreeFeatures> trees,
																	   std::shared_ptr<const BlockContext> context) {
	// TreeGrower's constants
	static const std::unordered_map<std::string, Config> growers = {
			{"minecraft:oak_sapling",
			 {0.1F, "", "", "minecraft:oak", "minecraft:fancy_oak", "minecraft:oak_bees_005", "minecraft:fancy_oak_bees_005"}},
			{"minecraft:spruce_sapling", {0.5F, "minecraft:mega_spruce", "minecraft:mega_pine", "minecraft:spruce", "", "", ""}},
			{"minecraft:mangrove_propagule", {0.85F, "", "", "minecraft:mangrove", "minecraft:tall_mangrove", "", ""}},
			{"minecraft:birch_sapling", {0.0F, "", "", "minecraft:birch", "", "minecraft:birch_bees_005", ""}},
			{"minecraft:jungle_sapling", {0.0F, "minecraft:mega_jungle_tree", "", "minecraft:jungle_tree_no_vine", "", "", ""}},
			{"minecraft:acacia_sapling", {0.0F, "", "", "minecraft:acacia", "", "", ""}},
			{"minecraft:cherry_sapling", {0.0F, "", "", "minecraft:cherry", "", "minecraft:cherry_bees_005", ""}},
			{"minecraft:dark_oak_sapling", {0.0F, "minecraft:dark_oak", "", "", "", "", ""}},
			{"minecraft:pale_oak_sapling", {0.0F, "minecraft:pale_oak_bonemeal", "", "", "", "", ""}},
	};
	auto it = growers.find(sapling);
	if (it == growers.end()) return nullptr;
	return std::make_shared<SaplingTreeGrower>(it->second, std::move(trees), std::move(context));
}

SaplingTreeGrower::SaplingTreeGrower(Config config, std::shared_ptr<const TreeFeatures> trees, std::shared_ptr<const BlockContext> context)
	: _config(std::move(config)), _trees(std::move(trees)), _context(std::move(context)) {
	_flowers = _context->tag("minecraft:flowers");
	_air	 = _context->defaultState("minecraft:air");
}

bool SaplingTreeGrower::hasFlowers(Level& level, const BlockPos& pos) const {
	for (int x = -2; x <= 2; x++) {
		for (int y = -1; y <= 1; y++) {
			for (int z = -2; z <= 2; z++) {
				if (_context->inTag(_flowers, level.getBlockState(pos.offset(x, y, z)))) return true;
			}
		}
	}
	return false;
}

bool SaplingTreeGrower::growTree(Level& level, const BlockPos& pos, int sapling) const {
	JavaRandom&	  random = level.random();
	constexpr int flags	 = Level::UPDATE_INVISIBLE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS; // 260

	// getConfiguredMegaFeature
	const std::string& mega = !_config.secondaryMegaTree.empty() && random.nextFloat() < _config.secondaryChance ? _config.secondaryMegaTree : _config.megaTree;
	if (const TreeFeature* feature = mega.empty() ? nullptr : _trees->get(mega)) {
		int block = _context->blocks.blockOf(sapling);
		for (int x = 0; x >= -1; x--) {
			for (int z = 0; z >= -1; z--) {
				BlockPos corner = pos.offset(x, 0, z);
				BlockPos square[4] = {corner, corner.offset(1, 0, 0), corner.offset(0, 0, 1), corner.offset(1, 0, 1)};
				bool	 twoByTwo  = true;
				for (const BlockPos& at : square) twoByTwo = twoByTwo && _context->is(level.getBlockState(at), block);
				if (!twoByTwo) continue;
				if (!feature->supported()) return false; // Not ported: the saplings stay
				for (const BlockPos& at : square) level.setBlock(at, _air, flags);
				if (feature->place(level, random, corner)) return true;
				for (const BlockPos& at : square) level.setBlock(at, sapling, flags);
				return false;
			}
		}
	}

	// getConfiguredFeature
	bool			   flowers = hasFlowers(level, pos);
	const std::string* chosen  = nullptr;
	if (random.nextFloat() < _config.secondaryChance) {
		if (flowers && !_config.secondaryFlowers.empty()) {
			chosen = &_config.secondaryFlowers;
		} else if (!_config.secondaryTree.empty()) {
			chosen = &_config.secondaryTree;
		}
	}
	if (!chosen) chosen = flowers && !_config.flowers.empty() ? &_config.flowers : &_config.tree;
	const TreeFeature* feature = chosen->empty() ? nullptr : _trees->get(*chosen);
	if (!feature || !feature->supported()) return false;

	int left = level.fluidLegacyBlock(level.getBlockState(pos));
	level.setBlock(pos, left, flags);
	if (feature->place(level, random, pos)) {
		// The sapling's removal wasn't sent (UPDATE_INVISIBLE)
		if (level.getBlockState(pos) == left) level.sendBlockUpdated(pos);
		return true;
	}
	level.setBlock(pos, sapling, flags);
	return false;
}
