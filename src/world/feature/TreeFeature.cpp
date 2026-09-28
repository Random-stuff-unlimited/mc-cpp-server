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
namespace {
	// A state from {"Name": ..., "Properties": {...}}
	int parseState(const json& state, const BlockContext& context) {
		int block = context.block(state.value("Name", "minecraft:air"));
		if (block < 0) return -1;
		int result = context.blocks.defaultState(block);
		if (state.contains("Properties")) {
			for (const auto& [property, value] : state["Properties"].items()) {
				int changed = context.blocks.with(result, context.blocks.property(property), context.blocks.value(value.get<std::string>()));
				if (changed >= 0) result = changed;
			}
		}
		return result;
	}
} // namespace

void TreeFeature::Placement::set(JavaHashSet<BlockPos>& into, const BlockPos& pos, int state) {
	into.add(pos);
	level.setBlock(pos, state, TREE_FLAGS);
}

// ----- Providers -----

// ----- Providers -----

TreeFeature::IntProvider TreeFeature::IntProvider::parse(const json& value) {
	IntProvider provider;
	if (value.is_number()) {
		provider.min = provider.max = value.get<int>();
		return provider;
	}
	std::string type = value.value("type", "minecraft:uniform"); // UniformInt fields have no type
	if (type == "minecraft:constant") {
		provider.min = provider.max = value.value("value", 0);
	} else if (type == "minecraft:uniform") {
		provider.min = value.value("min_inclusive", 0);
		provider.max = value.value("max_inclusive", 0);
	} else if (type == "minecraft:weighted_list") {
		for (const json& entry : value["distribution"]) {
			int weight = entry.value("weight", 1);
			provider.weighted.emplace_back(weight, parse(entry["data"]));
			provider.totalWeight += weight;
		}
	} else {
		throw std::runtime_error("int provider " + type + " not supported");
	}
	return provider;
}

int TreeFeature::IntProvider::sample(JavaRandom& random) const {
	if (!weighted.empty()) {
		// WeightedList.getRandomOrThrow
		int pick = random.nextInt(totalWeight);
		for (const auto& [weight, value] : weighted) {
			pick -= weight;
			if (pick < 0) return value.sample(random);
		}
	}
	if (min == max) return min;
	return random.nextInt(max - min + 1) + min; // Mth.randomBetweenInclusive
}

int TreeFeature::StateProvider::get(JavaRandom& random) const {
	if (states.size() == 1) return states[0].second;
	int pick = random.nextInt(totalWeight);
	for (const auto& [weight, state] : states) {
		pick -= weight;
		if (pick < 0) return state;
	}
	return states.back().second;
}

// ----- Configuration -----

// ----- Configuration -----

TreeFeature::TreeFeature(const json& config, const BlockContext& context, const TreeBlocks& blocks) : _context(context), _ids(blocks) {
	auto provider = [&](const json& value) {
		StateProvider result;
		std::string	  type = value.value("type", "");
		if (type == "minecraft:simple_state_provider") {
			result.states.emplace_back(1, parseState(value["state"], context));
		} else if (type == "minecraft:weighted_state_provider") {
			for (const json& entry : value["entries"]) {
				int weight = entry.value("weight", 1);
				result.states.emplace_back(weight, parseState(entry["data"], context));
				result.totalWeight += weight;
			}
		} else {
			_supported = false;
			result.states.emplace_back(1, context.defaultState("minecraft:air"));
		}
		result.totalWeight = std::max(result.totalWeight, 1);
		return result;
	};
	_trunkProvider	 = provider(config["trunk_provider"]);
	_foliageProvider = provider(config["foliage_provider"]);
	_dirtProvider	 = provider(config["dirt_provider"]);
	_forceDirt		 = config.value("force_dirt", false);
	_ignoreVines	 = config.value("ignore_vines", false);
	if (config.contains("root_placer")) _supported = false;

	const json& size  = config["minimum_size"];
	_threeLayers	  = size.value("type", "") == "minecraft:three_layers_feature_size";
	_sizeLimit		  = size.value("limit", _threeLayers ? 1 : 1);
	_sizeUpperLimit	  = size.value("upper_limit", 1);
	_lowerSize		  = size.value("lower_size", 0);
	_middleSize		  = size.value("middle_size", 1);
	_upperSize		  = size.value("upper_size", _threeLayers ? 1 : 1);
	_minClippedHeight = size.value("min_clipped_height", -1);

	try {
		const json& trunk = config["trunk_placer"];
		std::string type  = trunk.value("type", "");
		_baseHeight		  = trunk.value("base_height", 0);
		_heightRandA	  = trunk.value("height_rand_a", 0);
		_heightRandB	  = trunk.value("height_rand_b", 0);
		if (type == "minecraft:straight_trunk_placer") {
			_trunk = TrunkType::Straight;
		} else if (type == "minecraft:forking_trunk_placer") {
			_trunk = TrunkType::Forking;
		} else if (type == "minecraft:giant_trunk_placer") {
			_trunk = TrunkType::Giant;
		} else if (type == "minecraft:mega_jungle_trunk_placer") {
			_trunk = TrunkType::MegaJungle;
		} else if (type == "minecraft:dark_oak_trunk_placer") {
			_trunk = TrunkType::DarkOak;
		} else if (type == "minecraft:fancy_trunk_placer") {
			_trunk = TrunkType::Fancy;
		} else if (type == "minecraft:cherry_trunk_placer") {
			_trunk						  = TrunkType::Cherry;
			_branchCount				  = IntProvider::parse(trunk["branch_count"]);
			_branchHorizontalLength		  = IntProvider::parse(trunk["branch_horizontal_length"]);
			_branchStart				  = IntProvider::parse(trunk["branch_start_offset_from_top"]);
			_secondBranchStart			  = _branchStart;
			_secondBranchStart.max		  = _branchStart.max - 1;
			_branchEnd					  = IntProvider::parse(trunk["branch_end_offset_from_top"]);
		} else {
			_supported = false;
		}

		const json& foliage = config["foliage_placer"];
		type				= foliage.value("type", "");
		_radius				= IntProvider::parse(foliage["radius"]);
		_offset				= IntProvider::parse(foliage["offset"]);
		if (type == "minecraft:blob_foliage_placer") {
			_foliage	   = FoliageType::Blob;
			_foliageHeight = foliage.value("height", 0);
		} else if (type == "minecraft:fancy_foliage_placer") {
			_foliage	   = FoliageType::Fancy;
			_foliageHeight = foliage.value("height", 0);
		} else if (type == "minecraft:jungle_foliage_placer") {
			_foliage	   = FoliageType::Jungle;
			_foliageHeight = foliage.value("height", 0);
		} else if (type == "minecraft:spruce_foliage_placer") {
			_foliage			   = FoliageType::Spruce;
			_foliageHeightProvider = IntProvider::parse(foliage["trunk_height"]);
		} else if (type == "minecraft:pine_foliage_placer") {
			_foliage			   = FoliageType::Pine;
			_foliageHeightProvider = IntProvider::parse(foliage["height"]);
		} else if (type == "minecraft:mega_pine_foliage_placer") {
			_foliage			   = FoliageType::MegaPine;
			_foliageHeightProvider = IntProvider::parse(foliage["crown_height"]);
		} else if (type == "minecraft:acacia_foliage_placer") {
			_foliage = FoliageType::Acacia;
		} else if (type == "minecraft:dark_oak_foliage_placer") {
			_foliage = FoliageType::DarkOak;
		} else if (type == "minecraft:cherry_foliage_placer") {
			_foliage					  = FoliageType::Cherry;
			_foliageHeightProvider		  = IntProvider::parse(foliage["height"]);
			_wideBottomLayerHoleChance	  = foliage.value("wide_bottom_layer_hole_chance", 0.0F);
			_cornerHoleChance			  = foliage.value("corner_hole_chance", 0.0F);
			_hangingLeavesChance		  = foliage.value("hanging_leaves_chance", 0.0F);
			_hangingLeavesExtensionChance = foliage.value("hanging_leaves_extension_chance", 0.0F);
		} else {
			_supported = false;
		}
	} catch (const std::exception&) {
		_supported = false;
	}

	for (const json& decorator : config.value("decorators", json::array())) {
		std::string type = decorator.value("type", "");
		if (type == "minecraft:beehive") {
			_decorators.push_back({DecoratorType::Beehive, decorator.value("probability", 0.0F)});
		} else if (type == "minecraft:alter_ground") {
			StateProvider ground = provider(decorator["provider"]);
			_decorators.push_back({DecoratorType::AlterGround, 0.0F, ground.states[0].second});
		} else if (type == "minecraft:trunk_vine") {
			_decorators.push_back({DecoratorType::TrunkVine});
		} else if (type == "minecraft:leave_vine") {
			_decorators.push_back({DecoratorType::LeaveVine, decorator.value("probability", 0.0F)});
		} else {
			_supported = false;
		}
	}
}

// ----- TreeFeature -----

// ----- TreeFeature -----

bool TreeFeature::validTreePos(Level& level, const BlockPos& pos) const {
	int							   state	   = level.getBlockState(pos);
	return _context.isAir(state) || _context.inTag(_ids.replaceableByTrees, state);
}

bool TreeFeature::isFree(Level& level, const BlockPos& pos) const {
	return validTreePos(level, pos) || _context.inTag(_ids.logs, level.getBlockState(pos));
}

int TreeFeature::sizeAtHeight(int height, int y) const {
	if (!_threeLayers) return y < _sizeLimit ? _lowerSize : _upperSize;
	if (y < _sizeLimit) return _lowerSize;
	return y >= height - _sizeUpperLimit ? _upperSize : _middleSize;
}

int TreeFeature::maxFreeTreeHeight(Level& level, int height, const BlockPos& origin) const {
	for (int y = 0; y <= height + 1; y++) {
		int size = sizeAtHeight(height, y);
		for (int x = -size; x <= size; x++) {
			for (int z = -size; z <= size; z++) {
				BlockPos pos = origin.offset(x, y, z);
				if (!isFree(level, pos) || (!_ignoreVines && _context.is(level.getBlockState(pos), _ids.vine))) return y - 2;
			}
		}
	}
	return height;
}

bool TreeFeature::place(Level& level, JavaRandom& random, const BlockPos& origin) const {
	if (!_supported) return false;
	Placement p{level, random, {}, {}, {}, {}};

	// TreeFeature.doPlace
	int height			  = _baseHeight + random.nextInt(_heightRandA + 1) + random.nextInt(_heightRandB + 1);
	int foliageHeightHere = foliageHeight(random, height);
	int trunkHeight		  = height - foliageHeightHere;
	int radius			  = foliageRadius(random, trunkHeight);
	if (origin.y < level.minY() + 1 || origin.y + height + 1 > level.maxY()) return false;
	int free = maxFreeTreeHeight(level, height, origin);
	if (free < height && (_minClippedHeight < 0 || free < _minClippedHeight)) return false;
	std::vector<Attachment> attachments = placeTrunk(p, free, origin);
	for (const Attachment& attachment : attachments) {
		int offset = _offset.sample(random);
		createFoliage(p, free, attachment, foliageHeightHere, radius, offset);
	}

	if (p.trunk.empty() && p.foliage.empty()) return false;
	decorate(p);
	updateLeaves(p);
	return true;
}
