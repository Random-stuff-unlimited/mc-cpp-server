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

// ----- Foliage placers and decorators -----

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
