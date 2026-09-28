#include "world/feature/Trees.hpp"

#include "data/GameData.hpp"
#include "lib/JavaHashSet.hpp"
#include "lib/JavaRandom.hpp"
#include "world/Level.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/entity/Geometry.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

using json = nlohmann::json;

namespace {
	constexpr int TREE_FLAGS = Level::UPDATE_ALL | Level::UPDATE_KNOWN_SHAPE; // TreeFeature.BLOCK_UPDATE_FLAGS: 19

	// Mth.floor, with Java's (int) of NaN (0)
	int javaFloor(double value) {
		if (std::isnan(value)) return 0;
		int truncated = static_cast<int>(value);
		return value < truncated ? truncated - 1 : truncated;
	}

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

// Everything placed by one tree, in vanilla's hash sets (their order decides a few things)
struct TreeFeature::Placement {
	Level&					 level;
	JavaRandom&				 random;
	JavaHashSet<BlockPos>	 roots, trunk, foliage, decorations;

	void set(JavaHashSet<BlockPos>& into, const BlockPos& pos, int state) {
		into.add(pos);
		level.setBlock(pos, state, TREE_FLAGS);
	}
};

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

// ----- Trunk placers -----

void TreeFeature::setDirtAt(Placement& p, const BlockPos& pos) const {
	int							   state	= p.level.getBlockState(pos);
	bool						   isDirt	= _context.inTag(_ids.dirt, state) && !_context.is(state, _ids.grass) && !_context.is(state, _ids.mycelium);
	// Through the trunk's setter: the dirt counts as trunk afterwards
	if (_forceDirt || !isDirt) p.set(p.trunk, pos, _dirtProvider.get(p.random));
}

bool TreeFeature::placeLog(Placement& p, const BlockPos& pos, int axis) const {
	if (!validTreePos(p.level, pos)) return false;
	int state = _trunkProvider.get(p.random);
	if (axis >= 0) {
		// trySetValue(RotatedPillarBlock.AXIS, axis)
		int				 changed	  = _context.blocks.with(state, _ids.axis, _ids.axes[axis]);
		if (changed >= 0) state = changed;
	}
	p.set(p.trunk, pos, state);
	return true;
}

void TreeFeature::placeLogIfFree(Placement& p, const BlockPos& pos) const {
	if (isFree(p.level, pos)) placeLog(p, pos);
}

std::vector<TreeFeature::Attachment> TreeFeature::placeTrunk(Placement& p, int height, const BlockPos& origin) const {
	JavaRandom&				random = p.random;
	std::vector<Attachment> attachments;
	auto					randomHorizontal = [&random] { return Directions::HORIZONTAL[random.nextInt(4)]; };
	auto					step			 = [](Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; };

	switch (_trunk) {
	case TrunkType::Straight:
		setDirtAt(p, origin.below());
		for (int y = 0; y < height; y++) placeLog(p, origin.relative(Direction::Up, y));
		attachments.push_back({origin.relative(Direction::Up, height), 0, false});
		break;

	case TrunkType::Forking: {
		setDirtAt(p, origin.below());
		Direction lean	   = randomHorizontal();
		int		  leanFrom = height - random.nextInt(4) - 1;
		int		  leanLeft = 3 - random.nextInt(3);
		int		  x = origin.x, z = origin.z;
		int		  top	   = INT32_MIN;
		for (int y = 0; y < height; y++) {
			int worldY = origin.y + y;
			if (y >= leanFrom && leanLeft > 0) {
				x += step(lean, 0);
				z += step(lean, 2);
				leanLeft--;
			}
			if (placeLog(p, {x, worldY, z})) top = worldY + 1;
		}
		if (top != INT32_MIN) attachments.push_back({{x, top, z}, 1, false});
		x				 = origin.x;
		z				 = origin.z;
		Direction branch = randomHorizontal();
		if (branch != lean) {
			int start = leanFrom - random.nextInt(2) - 1;
			int left  = 1 + random.nextInt(3);
			top		  = INT32_MIN;
			for (int y = start; y < height && left > 0; left--) {
				if (y >= 1) {
					int worldY = origin.y + y;
					x += step(branch, 0);
					z += step(branch, 2);
					if (placeLog(p, {x, worldY, z})) top = worldY + 1;
				}
				y++;
			}
			if (top != INT32_MIN) attachments.push_back({{x, top, z}, 0, false});
		}
		break;
	}

	case TrunkType::Giant:
	case TrunkType::MegaJungle: {
		BlockPos below = origin.below();
		setDirtAt(p, below);
		setDirtAt(p, below.east());
		setDirtAt(p, below.south());
		setDirtAt(p, below.south().east());
		for (int y = 0; y < height; y++) {
			placeLogIfFree(p, origin.offset(0, y, 0));
			if (y < height - 1) {
				placeLogIfFree(p, origin.offset(1, y, 0));
				placeLogIfFree(p, origin.offset(1, y, 1));
				placeLogIfFree(p, origin.offset(0, y, 1));
			}
		}
		attachments.push_back({origin.relative(Direction::Up, height), 0, true});
		if (_trunk == TrunkType::Giant) break;
		// MegaJungleTrunkPlacer: branches going out
		for (int y = height - 2 - random.nextInt(4); y > height / 2; y -= 2 + random.nextInt(4)) {
			float angle = random.nextFloat() * static_cast<float>(M_PI * 2);
			int	  x = 0, z = 0;
			for (int i = 0; i < 5; i++) {
				x = static_cast<int>(1.5F + Mth::cos(angle) * i);
				z = static_cast<int>(1.5F + Mth::sin(angle) * i);
				placeLog(p, origin.offset(x, y - 3 + i / 2, z));
			}
			attachments.push_back({origin.offset(x, y, z), -2, false});
		}
		break;
	}

	case TrunkType::DarkOak: {
		BlockPos below = origin.below();
		setDirtAt(p, below);
		setDirtAt(p, below.east());
		setDirtAt(p, below.south());
		setDirtAt(p, below.south().east());
		Direction					   lean	  = randomHorizontal();
		int							   leanFrom = height - random.nextInt(4);
		int							   leanLeft = 2 - random.nextInt(3);
		int							   x = origin.x, z = origin.z;
		int							   topY = origin.y + height - 1;
		for (int y = 0; y < height; y++) {
			if (y >= leanFrom && leanLeft > 0) {
				x += step(lean, 0);
				z += step(lean, 2);
				leanLeft--;
			}
			BlockPos pos   = {x, origin.y + y, z};
			int		 state = p.level.getBlockState(pos);
			if (_context.isAir(state) || _context.inTag(_ids.leavesTag, state)) {
				placeLog(p, pos);
				placeLog(p, pos.east());
				placeLog(p, pos.south());
				placeLog(p, pos.east().south());
			}
		}
		attachments.push_back({{x, topY, z}, 0, true});
		for (int dx = -1; dx <= 2; dx++) {
			for (int dz = -1; dz <= 2; dz++) {
				if ((dx < 0 || dx > 1 || dz < 0 || dz > 1) && random.nextInt(3) <= 0) {
					int length = random.nextInt(3) + 2;
					for (int i = 0; i < length; i++) placeLog(p, {origin.x + dx, topY - i - 1, origin.z + dz});
					attachments.push_back({{origin.x + dx, topY, origin.z + dz}, 0, false});
				}
			}
		}
		break;
	}

	case TrunkType::Fancy: {
		int treeHeight	 = height + 2;
		int trunkTop	 = javaFloor(treeHeight * 0.618);
		setDirtAt(p, origin.below());
		int clusters	 = std::min(1, javaFloor(1.382 + std::pow(1.0 * treeHeight / 13.0, 2.0)));
		int branchTop	 = origin.y + trunkTop;
		int y			 = treeHeight - 5;
		struct FoliageCoords {
			Attachment attachment;
			int		   branchBase;
		};
		std::vector<FoliageCoords> coords;
		coords.push_back({{origin.relative(Direction::Up, y), 0, false}, branchTop});
		auto treeShape = [](int size, int layer) -> float {
			if (layer < size * 0.3F) return -1.0F;
			float half	 = size / 2.0F;
			float offset = half - layer;
			float radius = static_cast<float>(std::sqrt(half * half - offset * offset));
			if (offset == 0.0F) {
				radius = half;
			} else if (std::abs(offset) >= half) {
				return 0.0F;
			}
			return radius * 0.5F;
		};
		for (; y >= 0; y--) {
			float shape = treeShape(treeHeight, y);
			if (shape < 0.0F) continue;
			for (int i = 0; i < clusters; i++) {
				double	 length = 1.0 * shape * (random.nextFloat() + 0.328);
				double	 angle	= random.nextFloat() * 2.0F * M_PI;
				double	 bx		= length * std::sin(angle) + 0.5;
				double	 bz		= length * std::cos(angle) + 0.5;
				BlockPos start	= origin.offset(javaFloor(bx), y - 1, javaFloor(bz));
				BlockPos end	= start.relative(Direction::Up, 5);
				if (!makeLimb(p, start, end, false)) continue;
				int		 dx	  = origin.x - start.x;
				int		 dz	  = origin.z - start.z;
				double	 base = start.y - std::sqrt(static_cast<double>(dx * dx + dz * dz)) * 0.381;
				int		 baseY = base > branchTop ? branchTop : static_cast<int>(base);
				BlockPos trunkPos{origin.x, baseY, origin.z};
				if (makeLimb(p, trunkPos, start, false)) coords.push_back({{start, 0, false}, trunkPos.y});
			}
		}
		makeLimb(p, origin, origin.relative(Direction::Up, trunkTop), true);
		// makeBranches
		for (const FoliageCoords& coord : coords) {
			BlockPos base{origin.x, coord.branchBase, origin.z};
			if (base != coord.attachment.pos && coord.branchBase - origin.y >= treeHeight * 0.2) makeLimb(p, base, coord.attachment.pos, true);
		}
		for (const FoliageCoords& coord : coords) {
			if (coord.branchBase - origin.y >= treeHeight * 0.2) attachments.push_back(coord.attachment);
		}
		break;
	}

	case TrunkType::Cherry: {
		setDirtAt(p, origin.below());
		int first  = std::max(0, height - 1 + _branchStart.sample(random));
		int second = std::max(0, height - 1 + _secondBranchStart.sample(random));
		if (second >= first) second++;
		int	 count = _branchCount.sample(random);
		bool three = count == 3;
		bool two   = count >= 2;
		int	 trunk = three ? height : two ? std::max(first, second) + 1 : first + 1;
		for (int y = 0; y < trunk; y++) placeLog(p, origin.relative(Direction::Up, y));
		if (three) attachments.push_back({origin.relative(Direction::Up, trunk), 0, false});
		Direction direction = randomHorizontal();
		int		  axis		= direction == Direction::East || direction == Direction::West ? 0 : 2;
		attachments.push_back(cherryBranch(p, height, origin, direction, first, first < trunk - 1, axis));
		if (two) attachments.push_back(cherryBranch(p, height, origin, Directions::opposite(direction), second, second < trunk - 1, axis));
		break;
	}
	}
	return attachments;
}

// FancyTrunkPlacer.makeLimb: a line of logs (place) or whether one fits
bool TreeFeature::makeLimb(Placement& p, const BlockPos& start, const BlockPos& end, bool place) const {
	if (!place && start == end) return true;
	int	  dx = end.x - start.x, dy = end.y - start.y, dz = end.z - start.z;
	int	  steps = std::max(std::abs(dx), std::max(std::abs(dy), std::abs(dz)));
	float sx = static_cast<float>(dx) / steps, sy = static_cast<float>(dy) / steps, sz = static_cast<float>(dz) / steps;
	for (int i = 0; i <= steps; i++) {
		BlockPos pos = start.offset(javaFloor(0.5F + i * sx), javaFloor(0.5F + i * sy), javaFloor(0.5F + i * sz));
		if (place) {
			// getLogAxis: along the longest horizontal distance from the start, y if none
			int ax = std::abs(pos.x - start.x), az = std::abs(pos.z - start.z);
			int longest = std::max(ax, az);
			placeLog(p, pos, longest > 0 ? (ax == longest ? 0 : 2) : 1);
		} else if (!isFree(p.level, pos)) {
			return false;
		}
	}
	return true;
}

TreeFeature::Attachment TreeFeature::cherryBranch(Placement& p, int height, const BlockPos& origin, Direction direction, int start, bool trunkAbove,
												  int axis) const {
	BlockPos pos	   = origin.relative(Direction::Up, start);
	int		 endY	   = height - 1 + _branchEnd.sample(p.random);
	bool	 extended  = trunkAbove || endY < start;
	int		 length	   = _branchHorizontalLength.sample(p.random) + (extended ? 1 : 0);
	BlockPos end	   = origin.relative(direction, length).relative(Direction::Up, endY);
	int		 sideways = extended ? 2 : 1;
	for (int i = 0; i < sideways; i++) {
		pos = pos.relative(direction);
		placeLog(p, pos, axis);
	}
	Direction vertical = end.y > pos.y ? Direction::Up : Direction::Down;
	while (true) {
		int distance = std::abs(pos.x - end.x) + std::abs(pos.y - end.y) + std::abs(pos.z - end.z);
		if (distance == 0) return {end.above(), 0, false};
		float chance = static_cast<float>(std::abs(end.y - pos.y)) / distance;
		bool  up	 = p.random.nextFloat() < chance;
		pos			 = pos.relative(up ? vertical : direction);
		placeLog(p, pos, up ? -1 : axis);
	}
}

// ----- Foliage placers -----

int TreeFeature::foliageHeight(JavaRandom& random, int height) const {
	switch (_foliage) {
	case FoliageType::Spruce:
		return std::max(4, height - _foliageHeightProvider.sample(random));
	case FoliageType::Pine:
	case FoliageType::MegaPine:
	case FoliageType::Cherry:
		return _foliageHeightProvider.sample(random);
	case FoliageType::Acacia:
		return 0;
	case FoliageType::DarkOak:
		return 4;
	default:
		return _foliageHeight;
	}
}

int TreeFeature::foliageRadius(JavaRandom& random, int trunkHeight) const {
	int radius = _radius.sample(random);
	if (_foliage == FoliageType::Pine) radius += random.nextInt(std::max(trunkHeight + 1, 1));
	return radius;
}

void TreeFeature::createFoliage(Placement& p, int, const Attachment& a, int height, int radius, int offset) const {
	switch (_foliage) {
	case FoliageType::Blob:
		for (int y = offset; y >= offset - height; y--) {
			placeLeavesRow(p, a.pos, std::max(radius + a.radiusOffset - 1 - y / 2, 0), y, a.doubleTrunk);
		}
		break;
	case FoliageType::Fancy:
		for (int y = offset; y >= offset - height; y--) {
			placeLeavesRow(p, a.pos, radius + (y != offset && y != offset - height ? 1 : 0), y, a.doubleTrunk);
		}
		break;
	case FoliageType::Spruce: {
		int current = p.random.nextInt(2), max = 1, next = 0;
		for (int y = offset; y >= -height; y--) {
			placeLeavesRow(p, a.pos, current, y, a.doubleTrunk);
			if (current >= max) {
				current = next;
				next	= 1;
				max		= std::min(max + 1, radius + a.radiusOffset);
			} else {
				current++;
			}
		}
		break;
	}
	case FoliageType::Pine: {
		int current = 0;
		for (int y = offset; y >= offset - height; y--) {
			placeLeavesRow(p, a.pos, current, y, a.doubleTrunk);
			if (current >= 1 && y == offset - height + 1) {
				current--;
			} else if (current < radius + a.radiusOffset) {
				current++;
			}
		}
		break;
	}
	case FoliageType::MegaPine: {
		int previous = 0;
		for (int y = a.pos.y - height + offset; y <= a.pos.y + offset; y++) {
			int below = a.pos.y - y;
			int r	  = radius + a.radiusOffset + javaFloor(static_cast<float>(below) / height * 3.5F);
			int row	  = below > 0 && r == previous && (y & 1) == 0 ? r + 1 : r;
			placeLeavesRow(p, {a.pos.x, y, a.pos.z}, row, 0, a.doubleTrunk);
			previous = r;
		}
		break;
	}
	case FoliageType::Acacia: {
		BlockPos top = a.pos.relative(Direction::Up, offset);
		placeLeavesRow(p, top, radius + a.radiusOffset, -1 - height, a.doubleTrunk);
		placeLeavesRow(p, top, radius - 1, -height, a.doubleTrunk);
		placeLeavesRow(p, top, radius + a.radiusOffset - 1, 0, a.doubleTrunk);
		break;
	}
	case FoliageType::DarkOak: {
		BlockPos top = a.pos.relative(Direction::Up, offset);
		if (a.doubleTrunk) {
			placeLeavesRow(p, top, radius + 2, -1, true);
			placeLeavesRow(p, top, radius + 3, 0, true);
			placeLeavesRow(p, top, radius + 2, 1, true);
			if (p.random.nextBoolean()) placeLeavesRow(p, top, radius, 2, true);
		} else {
			placeLeavesRow(p, top, radius + 2, -1, false);
			placeLeavesRow(p, top, radius + 1, 0, false);
		}
		break;
	}
	case FoliageType::Jungle: {
		int layers = a.doubleTrunk ? height : 1 + p.random.nextInt(2);
		for (int y = offset; y >= offset - layers; y--) placeLeavesRow(p, a.pos, radius + a.radiusOffset + 1 - y, y, a.doubleTrunk);
		break;
	}
	case FoliageType::Cherry: {
		BlockPos top = a.pos.relative(Direction::Up, offset);
		int		 r	 = radius + a.radiusOffset - 1;
		placeLeavesRow(p, top, r - 2, height - 3, a.doubleTrunk);
		placeLeavesRow(p, top, r - 1, height - 4, a.doubleTrunk);
		for (int y = height - 5; y >= 0; y--) placeLeavesRow(p, top, r, y, a.doubleTrunk);
		placeLeavesRowWithHangingLeavesBelow(p, top, r, -1, a.doubleTrunk, _hangingLeavesChance, _hangingLeavesExtensionChance);
		placeLeavesRowWithHangingLeavesBelow(p, top, r - 1, -2, a.doubleTrunk, _hangingLeavesChance, _hangingLeavesExtensionChance);
		break;
	}
	}
}

bool TreeFeature::shouldSkipLocationSigned(JavaRandom& random, int dx, int y, int dz, int radius, bool doubleTrunk) const {
	// DarkOakFoliagePlacer: no corners on the far sides of the middle layer of a double trunk
	if (_foliage == FoliageType::DarkOak && y == 0 && doubleTrunk && (dx == -radius || dx >= radius) && (dz == -radius || dz >= radius)) return true;
	int x = doubleTrunk ? std::min(std::abs(dx), std::abs(dx - 1)) : std::abs(dx);
	int z = doubleTrunk ? std::min(std::abs(dz), std::abs(dz - 1)) : std::abs(dz);
	return shouldSkipLocation(random, x, y, z, radius, doubleTrunk);
}

bool TreeFeature::shouldSkipLocation(JavaRandom& random, int x, int y, int z, int radius, bool doubleTrunk) const {
	switch (_foliage) {
	case FoliageType::Blob:
		return x == radius && z == radius && (random.nextInt(2) == 0 || y == 0);
	case FoliageType::Fancy: {
		float fx = x + 0.5F, fz = z + 0.5F;
		return fx * fx + fz * fz > radius * radius;
	}
	case FoliageType::Spruce:
	case FoliageType::Pine:
		return x == radius && z == radius && radius > 0;
	case FoliageType::MegaPine:
	case FoliageType::Jungle:
		return x + z >= 7 ? true : x * x + z * z > radius * radius;
	case FoliageType::Acacia:
		return y == 0 ? (x > 1 || z > 1) && x != 0 && z != 0 : x == radius && z == radius && radius > 0;
	case FoliageType::DarkOak:
		if (y == -1 && !doubleTrunk) return x == radius && z == radius;
		return y == 1 ? x + z > radius * 2 - 2 : false;
	case FoliageType::Cherry: {
		if (y == -1 && (x == radius || z == radius) && random.nextFloat() < _wideBottomLayerHoleChance) return true;
		bool corner = x == radius && z == radius;
		bool wide	= radius > 2;
		return wide ? corner || (x + z > radius * 2 - 2 && random.nextFloat() < _cornerHoleChance) : corner && random.nextFloat() < _cornerHoleChance;
	}
	}
	return false;
}

void TreeFeature::placeLeavesRow(Placement& p, const BlockPos& origin, int radius, int y, bool doubleTrunk) const {
	int extra = doubleTrunk ? 1 : 0;
	for (int dx = -radius; dx <= radius + extra; dx++) {
		for (int dz = -radius; dz <= radius + extra; dz++) {
			if (!shouldSkipLocationSigned(p.random, dx, y, dz, radius, doubleTrunk)) tryPlaceLeaf(p, origin.offset(dx, y, dz));
		}
	}
}

void TreeFeature::placeLeavesRowWithHangingLeavesBelow(Placement& p, const BlockPos& origin, int radius, int y, bool doubleTrunk, float chance,
													   float extensionChance) const {
	placeLeavesRow(p, origin, radius, y, doubleTrunk);
	int		 extra = doubleTrunk ? 1 : 0;
	BlockPos below = origin.below();
	for (Direction side : Directions::HORIZONTAL) {
		// Along each side of the row, the clockwise direction going along it
		Direction clockwise = side == Direction::North ? Direction::East
							: side == Direction::East  ? Direction::South
							: side == Direction::South ? Direction::West
													   : Direction::North;
		bool	  positive	= clockwise == Direction::East || clockwise == Direction::South;
		BlockPos  pos		= origin.offset(0, y - 1, 0).relative(clockwise, positive ? radius + extra : radius).relative(side, -radius);
		for (int i = -radius; i < radius + extra; i++) {
			bool above = p.foliage.contains(pos.above());
			if (above && tryPlaceExtension(p, chance, below, pos)) tryPlaceExtension(p, extensionChance, below, pos.below());
			pos = pos.relative(side);
		}
	}
}

bool TreeFeature::tryPlaceExtension(Placement& p, float chance, const BlockPos& logPos, const BlockPos& pos) const {
	if (std::abs(pos.x - logPos.x) + std::abs(pos.y - logPos.y) + std::abs(pos.z - logPos.z) >= 7) return false;
	return p.random.nextFloat() > chance ? false : tryPlaceLeaf(p, pos);
}

bool TreeFeature::tryPlaceLeaf(Placement& p, const BlockPos& pos) const {
	int				 current	= p.level.getBlockState(pos);
	bool			 isPersistent = _context.blocks.has(current, _ids.persistent) && _context.blocks.getBool(current, _ids.persistent);
	if (isPersistent || !validTreePos(p.level, pos)) return false;
	int state = _foliageProvider.get(p.random);
	if (_context.blocks.has(state, _context.waterlogged)) {
		// Waterlogged in a water source
		FluidState fluid = p.level.getFluidState(pos);
		state			 = _context.blocks.withBool(state, _context.waterlogged, fluid.type == p.level.fluids().water());
	}
	p.set(p.foliage, pos, state);
	return true;
}

// ----- Decorators -----

void TreeFeature::decorate(Placement& p) const {
	if (_decorators.empty()) return;
	// TreeDecorator.Context: the logs and leaves in the sets' order, then by height (a stable sort)
	auto byHeight = [](JavaHashSet<BlockPos>& set) {
		std::vector<BlockPos> list = set.values();
		std::stable_sort(list.begin(), list.end(), [](const BlockPos& a, const BlockPos& b) { return a.y < b.y; });
		return list;
	};
	std::vector<BlockPos> logs	 = byHeight(p.trunk);
	std::vector<BlockPos> leaves = byHeight(p.foliage);
	JavaRandom&			  random = p.random;
	auto				  isAir	 = [&](const BlockPos& pos) { return _context.isAir(p.level.getBlockState(pos)); };
	auto placeVine = [&](const BlockPos& pos, const char* side) {
		int state = _context.blocks.withBool(_context.blocks.defaultState(_ids.vine), _context.blocks.property(side), true);
		p.set(p.decorations, pos, state);
	};
	// Vines on the sides of a block: the vine faces back to it
	struct Side {
		int			dx, dz;
		const char* property;
	};
	const Side sides[4] = {{-1, 0, "east"}, {1, 0, "west"}, {0, -1, "south"}, {0, 1, "north"}};

	for (const Decorator& decorator : _decorators) {
		switch (decorator.type) {
		case DecoratorType::Beehive: {
			if (logs.empty() || random.nextFloat() >= decorator.probability) break;
			int y = !leaves.empty() ? std::max(leaves.front().y - 1, logs.front().y + 1) : std::min(logs.front().y + 1 + random.nextInt(3), logs.back().y);
			// Next to a log at that height, except on the north side (the nest faces south)
			std::vector<BlockPos> spots;
			for (const BlockPos& log : logs) {
				if (log.y != y) continue;
				for (Direction direction : {Direction::East, Direction::South, Direction::West}) spots.push_back(log.relative(direction));
			}
			if (spots.empty()) break;
			for (int i = static_cast<int>(spots.size()); i > 1; i--) std::swap(spots[i - 1], spots[random.nextInt(i)]); // Util.shuffle
			for (const BlockPos& spot : spots) {
				if (!isAir(spot) || !isAir(spot.south())) continue;
				p.set(p.decorations, spot, _context.blocks.with(_context.blocks.defaultState(_ids.beeNest), _context.facing, _context.directionValue(Direction::South)));
				// The nest's bees (2 or 3): no block entities yet, only their random draws
				int bees = 2 + random.nextInt(2);
				for (int i = 0; i < bees; i++) random.nextInt(599);
				break;
			}
			break;
		}
		case DecoratorType::AlterGround: {
			if (logs.empty()) break;
			auto placeBlockAt = [&](const BlockPos& at) {
				for (int dy = 2; dy >= -3; dy--) {
					BlockPos pos = at.relative(Direction::Up, dy);
					if (_context.inTag(_ids.dirt, p.level.getBlockState(pos))) {
						p.set(p.decorations, pos, decorator.state);
						break;
					}
					if (!isAir(pos) && dy < 0) break;
				}
			};
			auto placeCircle = [&](const BlockPos& center) {
				for (int dx = -2; dx <= 2; dx++) {
					for (int dz = -2; dz <= 2; dz++) {
						if (std::abs(dx) != 2 || std::abs(dz) != 2) placeBlockAt(center.offset(dx, 0, dz));
					}
				}
			};
			int lowest = logs.front().y;
			for (const BlockPos& log : logs) {
				if (log.y != lowest) continue;
				placeCircle(log.west().north());
				placeCircle(log.offset(2, 0, -1));
				placeCircle(log.offset(-1, 0, 2));
				placeCircle(log.offset(2, 0, 2));
				for (int i = 0; i < 5; i++) {
					int value = random.nextInt(64);
					int x = value % 8, z = value / 8;
					if (x == 0 || x == 7 || z == 0 || z == 7) placeCircle(log.offset(-3 + x, 0, -3 + z));
				}
			}
			break;
		}
		case DecoratorType::TrunkVine:
			for (const BlockPos& log : logs) {
				for (const Side& side : sides) {
					if (random.nextInt(3) > 0) {
						BlockPos pos = log.offset(side.dx, 0, side.dz);
						if (isAir(pos)) placeVine(pos, side.property);
					}
				}
			}
			break;
		case DecoratorType::LeaveVine:
			for (const BlockPos& leaf : leaves) {
				for (const Side& side : sides) {
					if (random.nextFloat() < decorator.probability) {
						BlockPos pos = leaf.offset(side.dx, 0, side.dz);
						if (!isAir(pos)) continue;
						// addHangingVine: up to 4 more below
						placeVine(pos, side.property);
						BlockPos down = pos.below();
						for (int left = 4; isAir(down) && left > 0; left--) {
							placeVine(down, side.property);
							down = down.below();
						}
					}
				}
			}
			break;
		}
	}
}

// TreeFeature.updateLeaves and StructureTemplate.updateShapeAtEdge: the leaves' distances to the logs, then shape
// updates between the tree and what is around it
void TreeFeature::updateLeaves(Placement& p) const {
	// The box around everything placed
	BlockPos min{INT32_MAX, INT32_MAX, INT32_MAX}, max{INT32_MIN, INT32_MIN, INT32_MIN};
	for (JavaHashSet<BlockPos>* set : {&p.roots, &p.trunk, &p.foliage, &p.decorations}) {
		for (const BlockPos& pos : set->values()) {
			min = {std::min(min.x, pos.x), std::min(min.y, pos.y), std::min(min.z, pos.z)};
			max = {std::max(max.x, pos.x), std::max(max.y, pos.y), std::max(max.z, pos.z)};
		}
	}
	int				  sizeX = max.x - min.x + 1, sizeY = max.y - min.y + 1, sizeZ = max.z - min.z + 1;
	std::vector<bool> full(static_cast<size_t>(sizeX) * sizeY * sizeZ, false);
	auto			  inside = [&](const BlockPos& pos) {
		 return pos.x >= min.x && pos.x <= max.x && pos.y >= min.y && pos.y <= max.y && pos.z >= min.z && pos.z <= max.z;
	};
	auto index = [&](int x, int y, int z) { return (static_cast<size_t>(x) * sizeY + y) * sizeZ + z; };
	auto fill  = [&](const BlockPos& pos) { full[index(pos.x - min.x, pos.y - min.y, pos.z - min.z)] = true; };

	for (JavaHashSet<BlockPos>* set : {&p.decorations, &p.roots}) {
		for (const BlockPos& pos : set->values()) fill(pos);
	}
	// LeavesBlock.getOptionalDistanceAt: 0 for logs, the distance of leaves, -1 for the rest
	auto optionalDistance = [&](int state) {
		if (_context.inTag(_ids.logs, state)) return 0;
		return _ids.leavesBlocks[_context.blocks.blockOf(state)] ? _context.blocks.getInt(state, _ids.distance) : -1;
	};

	std::vector<JavaHashSet<BlockPos>> byDistance(7);
	for (const BlockPos& pos : p.trunk.values()) byDistance[0].add(pos);
	// Lowest distance first; a log found meanwhile brings it back to 0
	for (int distance = 0; distance < 7;) {
		if (byDistance[distance].empty()) {
			distance++;
			continue;
		}
		BlockPos pos = byDistance[distance].first();
		byDistance[distance].remove(pos);
		if (!inside(pos)) continue;
		// A position can be reached twice at different distances: vanilla sets it again (the larger one)
		if (distance != 0) p.level.setBlock(pos, _context.blocks.withInt(p.level.getBlockState(pos), _ids.distance, distance), TREE_FLAGS);
		fill(pos);
		for (Direction direction : Directions::ALL) {
			BlockPos next = pos.relative(direction);
			if (!inside(next) || full[index(next.x - min.x, next.y - min.y, next.z - min.z)]) continue;
			int known = optionalDistance(p.level.getBlockState(next));
			if (known < 0) continue;
			int nextDistance = std::min(known, distance + 1);
			if (nextDistance < 7) {
				byDistance[nextDistance].add(next);
				distance = std::min(distance, nextDistance);
			}
		}
	}

	// DiscreteVoxelShape.forAllFaces: faces along z (x, y, then z), then along y (z, x, y), then along x (y, z, x)
	auto face = [&](int x, int y, int z, Direction direction) {
		BlockPos pos{min.x + x, min.y + y, min.z + z};
		BlockPos neighbor		= pos.relative(direction);
		int		 state			= p.level.getBlockState(pos);
		int		 neighborState	= p.level.getBlockState(neighbor);
		int		 updated		= p.level.behavior(state).updateShape(p.level, pos, state, direction, neighbor, neighborState);
		if (updated != state) p.level.setBlock(pos, updated, Level::UPDATE_CLIENTS);
		int neighborUpdated = p.level.behavior(neighborState).updateShape(p.level, neighbor, neighborState, Directions::opposite(direction), pos, updated);
		if (neighborUpdated != neighborState) p.level.setBlock(neighbor, neighborUpdated, Level::UPDATE_CLIENTS);
	};
	auto isFull = [&](int x, int y, int z) { return full[index(x, y, z)]; };
	for (int x = 0; x < sizeX; x++) {
		for (int y = 0; y < sizeY; y++) {
			bool previous = false;
			for (int z = 0; z <= sizeZ; z++) {
				bool current = z != sizeZ && isFull(x, y, z);
				if (!previous && current) face(x, y, z, Direction::North);
				if (previous && !current) face(x, y, z - 1, Direction::South);
				previous = current;
			}
		}
	}
	for (int z = 0; z < sizeZ; z++) {
		for (int x = 0; x < sizeX; x++) {
			bool previous = false;
			for (int y = 0; y <= sizeY; y++) {
				bool current = y != sizeY && isFull(x, y, z);
				if (!previous && current) face(x, y, z, Direction::Down);
				if (previous && !current) face(x, y - 1, z, Direction::Up);
				previous = current;
			}
		}
	}
	for (int y = 0; y < sizeY; y++) {
		for (int z = 0; z < sizeZ; z++) {
			bool previous = false;
			for (int x = 0; x <= sizeX; x++) {
				bool current = x != sizeX && isFull(x, y, z);
				if (!previous && current) face(x, y, z, Direction::West);
				if (previous && !current) face(x - 1, y, z, Direction::East);
				previous = current;
			}
		}
	}
}

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
