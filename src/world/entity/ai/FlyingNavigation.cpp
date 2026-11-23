// The pathfinding of the mobs that fly: FlyNodeEvaluator, FlyingPathNavigation and FlyingMoveControl, ported from
// vanilla

#include "world/Level.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Navigation.hpp"
#include "world/entity/ai/Pathfinder.hpp"

#include <cmath>

// ===================== FlyNodeEvaluator =====================

void FlyNodeEvaluator::prepare(Level& level, Mob& mob) {
	NodeEvaluator::prepare(level, mob);
	_pathTypesByPos.clear();
	_oldWaterCost = mob.getPathfindingMalus(PathType::Water);
}

void FlyNodeEvaluator::done() {
	_mob->setPathfindingMalus(PathType::Water, _oldWaterCost);
	_pathTypesByPos.clear();
	NodeEvaluator::done();
}

Node* FlyNodeEvaluator::getStart() {
	AABB	 box = _mob->boundingBox();
	BlockPos start{Mth::floor(_mob->position().x), Mth::floor(box.minY + 0.5), Mth::floor(_mob->position().z)};
	if (!isOpen(findAcceptedNode(start.x, start.y, start.z))) {
		// One of the corners of its box, when its center isn't in an open cell
		for (double x : {box.minX, box.maxX}) {
			for (double z : {box.minZ, box.maxZ}) {
				Node* corner = findAcceptedNode(Mth::floor(x), start.y, Mth::floor(z));
				if (isOpen(corner)) return corner;
			}
		}
	}
	Node* node = getNode(start);
	node->type = getCachedPathType(start.x, start.y, start.z);
	node->costMalus = _mob->getPathfindingMalus(node->type);
	return node;
}

int FlyNodeEvaluator::getNeighbors(Node** neighbors, Node& node) {
	int count = 0;
	// The six faces first, then the edges and corners whose faces are open (no cutting through blocks)
	Node* face[3][3][3] = {};
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dz = -1; dz <= 1; dz++) {
				int axes = (dx != 0) + (dy != 0) + (dz != 0);
				if (axes != 1) continue;
				Node* n					= findAcceptedNode(node.x + dx, node.y + dy, node.z + dz);
				face[dx + 1][dy + 1][dz + 1] = n;
				if (isOpen(n) && !n->closed) neighbors[count++] = n;
			}
		}
	}
	auto open = [&](int dx, int dy, int dz) { return isOpen(face[dx + 1][dy + 1][dz + 1]); };
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dz = -1; dz <= 1; dz++) {
				int axes = (dx != 0) + (dy != 0) + (dz != 0);
				if (axes < 2) continue;
				if ((dx && !open(dx, 0, 0)) || (dy && !open(0, dy, 0)) || (dz && !open(0, 0, dz))) continue;
				Node* n = findAcceptedNode(node.x + dx, node.y + dy, node.z + dz);
				if (isOpen(n) && !n->closed) neighbors[count++] = n;
			}
		}
	}
	return count;
}

Node* FlyNodeEvaluator::findAcceptedNode(int x, int y, int z) {
	PathType type  = getCachedPathType(x, y, z);
	float	 malus = _mob->getPathfindingMalus(type);
	if (malus < 0.0F) return nullptr;
	Node* node		= getNode(x, y, z);
	node->type		= type;
	node->costMalus = std::max(node->costMalus, malus);
	if (type == PathType::Walkable) node->costMalus += 1.0F; // FlyNodeEvaluator: it rather flies than walks
	return node;
}

PathType FlyNodeEvaluator::getCachedPathType(int x, int y, int z) {
	int64_t key = BlockPos{x, y, z}.asLong();
	auto	it	= _pathTypesByPos.find(key);
	if (it != _pathTypesByPos.end()) return it->second;
	PathType type = getPathTypeOfMob(*_context, x, y, z, *_mob);
	_pathTypesByPos.emplace(key, type);
	return type;
}

PathType FlyNodeEvaluator::getPathType(PathfindingContext& context, int x, int y, int z) {
	PathType type = context.getPathTypeFromState(x, y, z);
	if (type != PathType::Open || y <= context.level().minY()) return type;
	// Open air: what is below makes it dangerous, or walkable just above the ground
	PathType below = context.getPathTypeFromState(x, y - 1, z);
	if (below == PathType::DamageFire || below == PathType::Lava) return PathType::DamageFire;
	if (below == PathType::DamageOther) return PathType::DamageOther;
	if (below == PathType::Blocked || below == PathType::Fence) return PathType::Walkable;
	return PathType::Open;
}

PathType FlyNodeEvaluator::getPathTypeOfMob(PathfindingContext& context, int x, int y, int z, Mob& mob) {
	// The worst cell its box covers (any blocked one blocks it)
	PathType worst		= PathType::Open;
	float	 worstMalus = 0.0F;
	for (int bx = x; bx < x + _entityWidth; bx++) {
		for (int by = y; by < y + _entityHeight; by++) {
			for (int bz = z; bz < z + _entityDepth; bz++) {
				PathType type = getPathType(context, bx, by, bz);
				if (type == PathType::Blocked) return PathType::Blocked;
				float malus = mob.getPathfindingMalus(type);
				if (malus < 0.0F) return type;
				if (bx == x && by == y && bz == z) {
					worst	   = type;
					worstMalus = malus;
				} else if (malus > worstMalus) {
					worst	   = type;
					worstMalus = malus;
				}
			}
		}
	}
	return worst;
}

// ===================== FlyingPathNavigation =====================

std::unique_ptr<PathFinder> FlyingPathNavigation::createPathFinder(int maxVisitedNodes) {
	auto evaluator = std::make_unique<FlyNodeEvaluator>();
	evaluator->setCanPassDoors(true);
	return std::make_unique<PathFinder>(std::move(evaluator), maxVisitedNodes);
}

Vec3 FlyingPathNavigation::getTempMobPos() { return _mob.position(); }

bool FlyingPathNavigation::canMoveDirectly(const Vec3& from, const Vec3& to) { return isClearForMovementBetween(_mob, from, to, true); }

bool FlyingPathNavigation::isStableDestination(const BlockPos& pos) {
	// FlyingPathNavigation.isStableDestination: something to land on
	return _level.gameData().getStateProperties(_level.getBlockState(pos.below())).solidRender;
}

// ===================== FlyingMoveControl =====================

void FlyingMoveControl::tick() {
	if (_operation == Operation::MoveTo) {
		_operation = Operation::Wait;
		_mob.setNoGravity(true);
		double dx = _wantedX - _mob.position().x, dy = _wantedY - _mob.position().y, dz = _wantedZ - _mob.position().z;
		if (dx * dx + dy * dy + dz * dz < 2.5E-7) {
			_mob.setYya(0.0F);
			_mob.setZza(0.0F);
			return;
		}
		float yaw = static_cast<float>(Mth::atan2(dz, dx) * 180.0 / M_PI) - 90.0F;
		_mob.setYRotPublic(rotlerp(_mob.yRot(), yaw, 90.0F));
		const AttributeIds& ids	  = _mob.attributeIds();
		float				speed = static_cast<float>(_speedModifier * _mob.getAttributeValue(_mob.onGround() ? ids.movementSpeed : ids.flyingSpeed));
		_mob.setSpeed(speed);
		double horizontal = std::sqrt(dx * dx + dz * dz);
		if (std::abs(dy) > 1.0E-5 || std::abs(horizontal) > 1.0E-5) {
			float pitch = static_cast<float>(-(Mth::atan2(dy, horizontal) * 180.0 / M_PI));
			_mob.setXRot(rotlerp(_mob.xRot(), pitch, static_cast<float>(_maxTurn)));
			_mob.setYya(dy > 0.0 ? speed : -speed);
		}
	} else {
		if (!_hoversInPlace) _mob.setNoGravity(false);
		_mob.setYya(0.0F);
		_mob.setZza(0.0F);
	}
}
