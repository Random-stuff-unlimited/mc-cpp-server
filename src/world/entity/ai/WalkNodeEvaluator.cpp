#include "data/GameData.hpp"
#include "world/Level.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/ai/Pathfinder.hpp"

#include <algorithm>
#include <cmath>

namespace {
	// Block ids and tags the path types look at, resolved once per game data
	struct PathBlocks {
		const GameData*	  data = nullptr;
		std::vector<bool> trapdoors, fences, walls, cauldrons, doors;
		int				  lilyPad, bigDripleaf, powderSnow, cactus, sweetBerryBush, honey, cocoa, witherRose, pointedDripstone, water;
		int				  open;
	};
	const PathBlocks& pathBlocks(Level& level) {
		static PathBlocks b;
		const GameData&	  data = level.gameData();
		if (b.data != &data) {
			b.data			  = &data;
			b.trapdoors		  = data.blockTag("minecraft:trapdoors");
			b.fences		  = data.blockTag("minecraft:fences");
			b.walls			  = data.blockTag("minecraft:walls");
			b.cauldrons		  = data.blockTag("minecraft:cauldrons");
			b.doors			  = data.blockTag("minecraft:doors");
			auto id			  = [&](const char* name) { return data.getStaticId("minecraft:block", name); };
			b.lilyPad		  = id("minecraft:lily_pad");
			b.bigDripleaf	  = id("minecraft:big_dripleaf");
			b.powderSnow	  = id("minecraft:powder_snow");
			b.cactus		  = id("minecraft:cactus");
			b.sweetBerryBush  = id("minecraft:sweet_berry_bush");
			b.honey			  = id("minecraft:honey_block");
			b.cocoa			  = id("minecraft:cocoa");
			b.witherRose	  = id("minecraft:wither_rose");
			b.pointedDripstone = id("minecraft:pointed_dripstone");
			b.water			  = id("minecraft:water");
			b.open			  = data.getBlocks().property("open");
		}
		return b;
	}

	double maxY(const std::vector<GameData::Box>& boxes) {
		double max = 0.0;
		for (const auto& box : boxes) max = std::max(max, box.maxY);
		return max;
	}
} // namespace

// ----- Path types of the blocks -----

PathType WalkNodeEvaluator::getPathTypeFromState(Level& level, const BlockPos& pos) {
	int					 state	= level.getBlockState(pos);
	const BlockRegistry& blocks = level.blocks();
	if (blocks.isAir(state)) return PathType::Open;
	const PathBlocks& b		= pathBlocks(level);
	const GameData&	  data	= level.gameData();
	int				  block = blocks.blockOf(state);
	if (b.trapdoors[block] || block == b.lilyPad || block == b.bigDripleaf) return PathType::Trapdoor;
	if (block == b.powderSnow) return PathType::PowderSnow;
	if (block == b.cactus || block == b.sweetBerryBush) return PathType::DamageOther;
	if (block == b.honey) return PathType::StickyHoney;
	if (block == b.cocoa) return PathType::Cocoa;
	if (block == b.witherRose || block == b.pointedDripstone) return PathType::DamageCautious;
	FluidState fluid = level.fluids().stateOf(state);
	if (level.fluids().isLava(fluid.type)) return PathType::Lava;
	if (NodeEvaluator::isBurningBlock(level, state)) return PathType::DamageFire;
	if (data.isInstanceOf(block, "DoorBlock")) {
		if (blocks.getBool(state, b.open)) return PathType::DoorOpen;
		// BlockSetType.canOpenByHand: every door but iron's
		return data.getStaticName("minecraft:block", block) == "minecraft:iron_door" ? PathType::DoorIronClosed : PathType::DoorWoodClosed;
	}
	if (data.isInstanceOf(block, "BaseRailBlock")) return PathType::Rail;
	if (data.isInstanceOf(block, "LeavesBlock")) return PathType::Leaves;
	bool fenceGate = data.isInstanceOf(block, "FenceGateBlock");
	if (b.fences[block] || b.walls[block] || (fenceGate && !blocks.getBool(state, b.open))) return PathType::Fence;
	if (!(data.getStateProperties(state).pathfindable & static_cast<int>(PathComputationType::Land))) return PathType::Blocked;
	return level.fluids().isWater(fluid.type) ? PathType::Water : PathType::Open;
}

PathType WalkNodeEvaluator::getPathTypeStatic(PathfindingContext& context, int x, int y, int z) {
	PathType type = context.getPathTypeFromState(x, y, z);
	if (type != PathType::Open || y < context.level().minY() + 1) return type;
	switch (context.getPathTypeFromState(x, y - 1, z)) {
	case PathType::Open:
	case PathType::Water:
	case PathType::Lava:
	case PathType::Walkable: return PathType::Open;
	case PathType::DamageFire: return PathType::DamageFire;
	case PathType::DamageOther: return PathType::DamageOther;
	case PathType::StickyHoney: return PathType::StickyHoney;
	case PathType::PowderSnow: return PathType::DangerPowderSnow;
	case PathType::DamageCautious: return PathType::DamageCautious;
	case PathType::Trapdoor: return PathType::DangerTrapdoor;
	default: return checkNeighbourBlocks(context, x, y, z, PathType::Walkable);
	}
}

PathType WalkNodeEvaluator::getPathTypeStatic(Level& level, Mob& mob, const BlockPos& pos) {
	PathfindingContext context(level, mob);
	return getPathTypeStatic(context, pos.x, pos.y, pos.z);
}

PathType WalkNodeEvaluator::checkNeighbourBlocks(PathfindingContext& context, int x, int y, int z, PathType type) {
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dz = -1; dz <= 1; dz++) {
				if (dx == 0 && dz == 0) continue;
				PathType around = context.getPathTypeFromState(x + dx, y + dy, z + dz);
				if (around == PathType::DamageOther) return PathType::DangerOther;
				if (around == PathType::DamageFire || around == PathType::Lava) return PathType::DangerFire;
				if (around == PathType::Water) return PathType::WaterBorder;
				if (around == PathType::DamageCautious) return PathType::DamageCautious;
			}
		}
	}
	return type;
}

PathType WalkNodeEvaluator::getPathType(PathfindingContext& context, int x, int y, int z) { return getPathTypeStatic(context, x, y, z); }

double WalkNodeEvaluator::getFloorLevel(Level& level, const BlockPos& pos) {
	BlockPos below = pos.below();
	return below.y + maxY(level.gameData().getCollisionShape(level.getBlockState(below)));
}

double WalkNodeEvaluator::getFloorLevel(const BlockPos& pos) {
	Level& level = _context->level();
	if ((canFloat() || isAmphibious()) && level.fluids().isWater(level.getFluidState(pos).type)) return pos.y + 0.5;
	return getFloorLevel(level, pos);
}

// ----- Search -----

void WalkNodeEvaluator::prepare(Level& level, Mob& mob) {
	NodeEvaluator::prepare(level, mob);
	mob.onPathfindingStart();
}

void WalkNodeEvaluator::done() {
	_mob->onPathfindingDone();
	_pathTypesByPos.clear();
	_collisionCache.clear();
	NodeEvaluator::done();
}

Node* WalkNodeEvaluator::getStart() {
	Mob&   mob	 = *_mob;
	Level& level = _context->level();
	int	   y	 = mob.blockPosition().y;
	int	   state = level.getBlockState({Mth::floor(mob.position().x), y, Mth::floor(mob.position().z)});
	if (!mob.canStandOnFluid(level.fluids().stateOf(state))) {
		if (canFloat() && mob.isInWaterNow()) {
			int water = pathBlocks(level).water;
			while (true) {
				FluidState fluid = level.fluids().stateOf(state);
				bool	   isSource = fluid.type == level.fluids().water() && fluid.amount == 8 && !fluid.falling;
				if (level.blocks().blockOf(state) != water && !isSource) {
					y--;
					break;
				}
				state = level.getBlockState({Mth::floor(mob.position().x), ++y, Mth::floor(mob.position().z)});
			}
		} else if (mob.onGround()) {
			y = Mth::floor(mob.position().y + 0.5);
		} else {
			BlockPos at{Mth::floor(mob.position().x), Mth::floor(mob.position().y + 1.0), Mth::floor(mob.position().z)};
			while (at.y > level.minY()) {
				y	 = at.y;
				at.y = at.y - 1;
				int below = level.getBlockState(at);
				if (!level.blocks().isAir(below) && !(level.gameData().getStateProperties(below).pathfindable & static_cast<int>(PathComputationType::Land))) break;
			}
		}
	} else {
		while (mob.canStandOnFluid(level.fluids().stateOf(state))) state = level.getBlockState({Mth::floor(mob.position().x), ++y, Mth::floor(mob.position().z)});
		y--;
	}
	BlockPos feet = mob.blockPosition();
	if (!canStartAt({feet.x, y, feet.z})) {
		AABB box = mob.boundingBox();
		for (BlockPos corner : {BlockPos{Mth::floor(box.minX), y, Mth::floor(box.minZ)}, BlockPos{Mth::floor(box.minX), y, Mth::floor(box.maxZ)},
								BlockPos{Mth::floor(box.maxX), y, Mth::floor(box.minZ)}, BlockPos{Mth::floor(box.maxX), y, Mth::floor(box.maxZ)}}) {
			if (canStartAt(corner)) return getStartNode(corner);
		}
	}
	return getStartNode({feet.x, y, feet.z});
}

Node* WalkNodeEvaluator::getStartNode(const BlockPos& pos) {
	Node* node		= getNode(pos);
	node->type		= getCachedPathType(node->x, node->y, node->z);
	node->costMalus = _mob->getPathfindingMalus(node->type);
	return node;
}

bool WalkNodeEvaluator::canStartAt(const BlockPos& pos) {
	PathType type = getCachedPathType(pos.x, pos.y, pos.z);
	return type != PathType::Open && _mob->getPathfindingMalus(type) >= 0.0F;
}

int WalkNodeEvaluator::getNeighbors(Node** neighbors, Node& node) {
	int		 count	  = 0;
	int		 jumpSize = 0;
	PathType above	  = getCachedPathType(node.x, node.y + 1, node.z);
	PathType here	  = getCachedPathType(node.x, node.y, node.z);
	if (_mob->getPathfindingMalus(above) >= 0.0F && here != PathType::StickyHoney) jumpSize = Mth::floor(std::max(1.0F, _mob->maxUpStep()));
	double floor = getFloorLevel({node.x, node.y, node.z});
	for (Direction direction : Directions::HORIZONTAL) {
		Node* neighbor = findAcceptedNode(node.x + Directions::stepX(direction), node.y, node.z + Directions::stepZ(direction), jumpSize, floor, direction, here);
		_reusableNeighbors[Directions::to2DDataValue(direction)] = neighbor;
		if (isNeighborValid(neighbor, node)) neighbors[count++] = neighbor;
	}
	for (Direction direction : Directions::HORIZONTAL) {
		Direction side = Directions::clockWise(direction);
		if (!isDiagonalValid(node, _reusableNeighbors[Directions::to2DDataValue(direction)], _reusableNeighbors[Directions::to2DDataValue(side)])) continue;
		Node* diagonal = findAcceptedNode(node.x + Directions::stepX(direction) + Directions::stepX(side), node.y,
										  node.z + Directions::stepZ(direction) + Directions::stepZ(side), jumpSize, floor, direction, here);
		if (isDiagonalValid(diagonal)) neighbors[count++] = diagonal;
	}
	return count;
}

bool WalkNodeEvaluator::isNeighborValid(Node* neighbor, Node& node) const {
	return neighbor && !neighbor->closed && (neighbor->costMalus >= 0.0F || node.costMalus < 0.0F);
}

bool WalkNodeEvaluator::isDiagonalValid(Node& node, Node* a, Node* b) const {
	if (!b || !a || b->y > node.y || a->y > node.y) return false;
	if (a->type == PathType::WalkableDoor || b->type == PathType::WalkableDoor) return false;
	bool thinFences = b->type == PathType::Fence && a->type == PathType::Fence && _mob->width() < 0.5;
	return (b->y < node.y || b->costMalus >= 0.0F || thinFences) && (a->y < node.y || a->costMalus >= 0.0F || thinFences);
}

bool WalkNodeEvaluator::isDiagonalValid(Node* node) const {
	if (!node || node->closed) return false;
	return node->type == PathType::WalkableDoor ? false : node->costMalus >= 0.0F;
}

namespace {
	bool hasPartialCollision(PathType type) { return type == PathType::Fence || type == PathType::DoorWoodClosed || type == PathType::DoorIronClosed; }
} // namespace

bool WalkNodeEvaluator::canReachWithoutCollision(Node& node) {
	AABB   box = _mob->boundingBox();
	double sx = box.maxX - box.minX, sy = box.maxY - box.minY, sz = box.maxZ - box.minZ;
	Vec3   step{node.x - _mob->position().x + sx / 2.0, node.y - _mob->position().y + sy / 2.0, node.z - _mob->position().z + sz / 2.0};
	double size	 = (sx + sy + sz) / 3.0; // AABB.getSize
	int	   steps = Mth::ceil(step.length() / size);
	step		 = step.scale(1.0F / steps);
	for (int i = 1; i <= steps; i++) {
		box = box.move(step);
		if (hasCollisions(box)) return false;
	}
	return true;
}

double WalkNodeEvaluator::mobJumpHeight() const { return std::max(1.125, static_cast<double>(_mob->maxUpStep())); }

Node* WalkNodeEvaluator::findAcceptedNode(int x, int y, int z, int jumpSize, double nodeFloor, Direction direction, PathType fromType) {
	Node*  node	 = nullptr;
	double floor = getFloorLevel({x, y, z});
	if (floor - nodeFloor > mobJumpHeight()) return nullptr;
	PathType type  = getCachedPathType(x, y, z);
	float	 malus = _mob->getPathfindingMalus(type);
	if (malus >= 0.0F) node = getNodeAndUpdateCostToMax(x, y, z, type, malus);
	if (hasPartialCollision(fromType) && node && node->costMalus >= 0.0F && !canReachWithoutCollision(*node)) node = nullptr;
	if (type == PathType::Walkable || (isAmphibious() && type == PathType::Water)) return node;
	if ((!node || node->costMalus < 0.0F) && jumpSize > 0 && (type != PathType::Fence || canWalkOverFences()) && type != PathType::UnpassableRail &&
		type != PathType::Trapdoor && type != PathType::PowderSnow) {
		node = tryJumpOn(x, y, z, jumpSize, nodeFloor, direction, fromType);
	} else if (!isAmphibious() && type == PathType::Water && !canFloat()) {
		node = tryFindFirstNonWaterBelow(x, y, z, node);
	} else if (type == PathType::Open) {
		node = tryFindFirstGroundNodeBelow(x, y, z);
	} else if (hasPartialCollision(type) && !node) {
		node = getClosedNode(x, y, z, type);
	}
	return node;
}

Node* WalkNodeEvaluator::getNodeAndUpdateCostToMax(int x, int y, int z, PathType type, float malus) {
	Node* node		= getNode(x, y, z);
	node->type		= type;
	node->costMalus = std::max(node->costMalus, malus);
	return node;
}

Node* WalkNodeEvaluator::getBlockedNode(int x, int y, int z) {
	Node* node		= getNode(x, y, z);
	node->type		= PathType::Blocked;
	node->costMalus = -1.0F;
	return node;
}

Node* WalkNodeEvaluator::getClosedNode(int x, int y, int z, PathType type) {
	Node* node		= getNode(x, y, z);
	node->closed	= true;
	node->type		= type;
	node->costMalus = defaultMalus(type);
	return node;
}

Node* WalkNodeEvaluator::tryJumpOn(int x, int y, int z, int jumpSize, double nodeFloor, Direction direction, PathType fromType) {
	Node* above = findAcceptedNode(x, y + 1, z, jumpSize - 1, nodeFloor, direction, fromType);
	if (!above) return nullptr;
	if (_mob->width() >= 1.0F) return above;
	if (above->type != PathType::Open && above->type != PathType::Walkable) return above;
	// Room to jump from the node before, onto this one
	double cx = x - Directions::stepX(direction) + 0.5, cz = z - Directions::stepZ(direction) + 0.5;
	double half = _mob->width() / 2.0;
	AABB   box{cx - half, getFloorLevel({Mth::floor(cx), y + 1, Mth::floor(cz)}) + 0.001, cz - half, cx + half,
			   _mob->height() + getFloorLevel({above->x, above->y, above->z}) - 0.002, cz + half};
	return hasCollisions(box) ? nullptr : above;
}

Node* WalkNodeEvaluator::tryFindFirstNonWaterBelow(int x, int y, int z, Node* node) {
	y--;
	while (y > _context->level().minY()) {
		PathType type = getCachedPathType(x, y, z);
		if (type != PathType::Water) return node;
		node = getNodeAndUpdateCostToMax(x, y, z, type, _mob->getPathfindingMalus(type));
		y--;
	}
	return node;
}

Node* WalkNodeEvaluator::tryFindFirstGroundNodeBelow(int x, int y, int z) {
	for (int below = y - 1; below >= _context->level().minY(); below--) {
		if (y - below > _mob->getMaxFallDistance()) return getBlockedNode(x, below, z);
		PathType type  = getCachedPathType(x, below, z);
		float	 malus = _mob->getPathfindingMalus(type);
		if (type != PathType::Open) {
			if (malus >= 0.0F) return getNodeAndUpdateCostToMax(x, below, z, type, malus);
			return getBlockedNode(x, below, z);
		}
	}
	return getBlockedNode(x, y, z);
}

bool WalkNodeEvaluator::hasCollisions(const AABB& box) {
	std::array<double, 6> key{box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
	auto				  it = _collisionCache.find(key);
	if (it != _collisionCache.end()) return it->second;
	bool collides		 = _context->level().hasBlockCollision(box);
	_collisionCache[key] = collides;
	return collides;
}

PathType WalkNodeEvaluator::getCachedPathType(int x, int y, int z) {
	int64_t key = BlockPos{x, y, z}.asLong();
	auto	it	= _pathTypesByPos.find(key);
	if (it != _pathTypesByPos.end()) return it->second;
	PathType type		 = getPathTypeOfMob(*_context, x, y, z, *_mob);
	_pathTypesByPos[key] = type;
	return type;
}

std::vector<PathType> WalkNodeEvaluator::getPathTypeWithinMobBB(PathfindingContext& context, int x, int y, int z) {
	// An EnumSet: each type once, iterated in declaration order
	bool				  seen[static_cast<int>(PathType::Count)] = {};
	BlockPos			  feet									 = _mob->blockPosition();
	for (int dx = 0; dx < _entityWidth; dx++) {
		for (int dy = 0; dy < _entityHeight; dy++) {
			for (int dz = 0; dz < _entityDepth; dz++) {
				PathType type = getPathType(context, x + dx, y + dy, z + dz);
				if (type == PathType::DoorWoodClosed && canOpenDoors() && canPassDoors()) type = PathType::WalkableDoor;
				if (type == PathType::DoorOpen && !canPassDoors()) type = PathType::Blocked;
				if (type == PathType::Rail && getPathType(context, feet.x, feet.y, feet.z) != PathType::Rail &&
					getPathType(context, feet.x, feet.y - 1, feet.z) != PathType::Rail) {
					type = PathType::UnpassableRail;
				}
				seen[static_cast<int>(type)] = true;
			}
		}
	}
	std::vector<PathType> types;
	for (int t = 0; t < static_cast<int>(PathType::Count); t++) {
		if (seen[t]) types.push_back(static_cast<PathType>(t));
	}
	return types;
}

PathType WalkNodeEvaluator::getPathTypeOfMob(PathfindingContext& context, int x, int y, int z, Mob& mob) {
	std::vector<PathType> types = getPathTypeWithinMobBB(context, x, y, z);
	if (std::find(types.begin(), types.end(), PathType::Fence) != types.end()) return PathType::Fence;
	if (std::find(types.begin(), types.end(), PathType::UnpassableRail) != types.end()) return PathType::UnpassableRail;
	PathType best = PathType::Blocked;
	for (PathType type : types) {
		if (mob.getPathfindingMalus(type) < 0.0F) return type;
		if (mob.getPathfindingMalus(type) >= mob.getPathfindingMalus(best)) best = type;
	}
	if (_entityWidth <= 1 && best != PathType::Open && mob.getPathfindingMalus(best) == 0.0F && getPathType(context, x, y, z) == PathType::Open) {
		return PathType::Open;
	}
	return best;
}
