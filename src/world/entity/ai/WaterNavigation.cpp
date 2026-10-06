// The pathfinding of the mobs that swim: SwimNodeEvaluator, AmphibiousNodeEvaluator, WaterBoundPathNavigation,
// AmphibiousPathNavigation, SmoothSwimmingMoveControl and SmoothSwimmingLookControl, ported from vanilla

#include "world/Level.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Navigation.hpp"
#include "world/entity/ai/Pathfinder.hpp"

#include <cmath>

// ===================== SwimNodeEvaluator =====================

void SwimNodeEvaluator::prepare(Level& level, Mob& mob) {
	NodeEvaluator::prepare(level, mob);
	_pathTypesByPos.clear();
}

void SwimNodeEvaluator::done() {
	NodeEvaluator::done();
	_pathTypesByPos.clear();
}

Node* SwimNodeEvaluator::getStart() {
	AABB box = _mob->boundingBox();
	return getNode(Mth::floor(box.minX), Mth::floor(box.minY + 0.5), Mth::floor(box.minZ));
}

int SwimNodeEvaluator::getNeighbors(Node** neighbors, Node& node) {
	int	  count = 0;
	Node* byDirection[6];
	for (Direction direction : Directions::ALL) {
		Node* neighbor = findAcceptedNode(node.x + Directions::stepX(direction), node.y + Directions::stepY(direction), node.z + Directions::stepZ(direction));
		byDirection[static_cast<int>(direction)] = neighbor;
		if (neighbor && !neighbor->closed) neighbors[count++] = neighbor;
	}
	auto hasMalus = [](Node* n) { return n && n->costMalus >= 0.0F; };
	for (Direction direction : Directions::HORIZONTAL) {
		Direction clockWise = Directions::clockWise(direction);
		if (!hasMalus(byDirection[static_cast<int>(direction)]) || !hasMalus(byDirection[static_cast<int>(clockWise)])) continue;
		Node* diagonal = findAcceptedNode(node.x + Directions::stepX(direction) + Directions::stepX(clockWise), node.y,
										  node.z + Directions::stepZ(direction) + Directions::stepZ(clockWise));
		if (diagonal && !diagonal->closed) neighbors[count++] = diagonal;
	}
	return count;
}

Node* SwimNodeEvaluator::findAcceptedNode(int x, int y, int z) {
	PathType type = getCachedBlockType(x, y, z);
	if (!((_allowBreaching && type == PathType::Breach) || type == PathType::Water)) return nullptr;
	float malus = _mob->getPathfindingMalus(type);
	if (malus < 0.0F) return nullptr;
	Node* node		= getNode(x, y, z);
	node->type		= type;
	node->costMalus = std::max(node->costMalus, malus);
	Level& level	= _context->level();
	if (level.fluids().stateOf(level.getBlockState({x, y, z})).type == 0) node->costMalus += 8.0F;
	return node;
}

PathType SwimNodeEvaluator::getCachedBlockType(int x, int y, int z) {
	int64_t key = BlockPos{x, y, z}.asLong();
	auto	it	= _pathTypesByPos.find(key);
	if (it != _pathTypesByPos.end()) return it->second;
	PathType type = getPathType(*_context, x, y, z);
	_pathTypesByPos.emplace(key, type);
	return type;
}

PathType SwimNodeEvaluator::getPathType(PathfindingContext& context, int x, int y, int z) { return getPathTypeOfMob(context, x, y, z, *_mob); }

PathType SwimNodeEvaluator::getPathTypeOfMob(PathfindingContext& context, int x, int y, int z, Mob&) {
	Level&			level = context.level();
	const GameData& data  = level.gameData();
	int				last  = 0;
	for (int bx = x; bx < x + _entityWidth; bx++) {
		for (int by = y; by < y + _entityHeight; by++) {
			for (int bz = z; bz < z + _entityDepth; bz++) {
				last				= context.getBlockState({bx, by, bz});
				FluidState fluid	= level.fluids().stateOf(last);
				bool	   pathable = data.getStateProperties(last).pathfindable & static_cast<int>(PathComputationType::Water);
				if (fluid.type == 0 && pathable && level.blocks().isAir(last)) return PathType::Breach;
				if (!level.fluids().isWater(fluid.type)) return PathType::Blocked;
			}
		}
	}
	return data.getStateProperties(last).pathfindable & static_cast<int>(PathComputationType::Water) ? PathType::Water : PathType::Blocked;
}

// ===================== AmphibiousNodeEvaluator =====================

void AmphibiousNodeEvaluator::prepare(Level& level, Mob& mob) {
	WalkNodeEvaluator::prepare(level, mob);
	mob.setPathfindingMalus(PathType::Water, 0.0F);
	_oldWalkableCost = mob.getPathfindingMalus(PathType::Walkable);
	mob.setPathfindingMalus(PathType::Walkable, 6.0F);
	_oldWaterBorderCost = mob.getPathfindingMalus(PathType::WaterBorder);
	mob.setPathfindingMalus(PathType::WaterBorder, 4.0F);
}

void AmphibiousNodeEvaluator::done() {
	_mob->setPathfindingMalus(PathType::Walkable, _oldWalkableCost);
	_mob->setPathfindingMalus(PathType::WaterBorder, _oldWaterBorderCost);
	WalkNodeEvaluator::done();
}

Node* AmphibiousNodeEvaluator::getStart() {
	if (!_mob->isInWaterNow()) return WalkNodeEvaluator::getStart();
	AABB box = _mob->boundingBox();
	return getStartNode({Mth::floor(box.minX), Mth::floor(box.minY + 0.5), Mth::floor(box.minZ)});
}

int AmphibiousNodeEvaluator::getNeighbors(Node** neighbors, Node& node) {
	int		 count = WalkNodeEvaluator::getNeighbors(neighbors, node);
	PathType above = getCachedPathType(node.x, node.y + 1, node.z);
	PathType here  = getCachedPathType(node.x, node.y, node.z);
	int		 jump  = _mob->getPathfindingMalus(above) >= 0.0F && here != PathType::StickyHoney ? Mth::floor(std::max(1.0F, _mob->maxUpStep())) : 0;
	double	 floor = getFloorLevel(BlockPos{node.x, node.y, node.z});
	Node*	 up	   = findAcceptedNode(node.x, node.y + 1, node.z, std::max(0, jump - 1), floor, Direction::Up, here);
	Node*	 down  = findAcceptedNode(node.x, node.y - 1, node.z, jump, floor, Direction::Down, here);
	if (isNeighborValid(up, node) && up->type == PathType::Water) neighbors[count++] = up;
	if (isNeighborValid(down, node) && down->type == PathType::Water && here != PathType::Trapdoor) neighbors[count++] = down;
	for (int i = 0; i < count; i++) {
		Node* neighbor = neighbors[i];
		if (neighbor->type == PathType::Water && _prefersShallowSwimming && neighbor->y < _mob->level().seaLevel() - 10) neighbor->costMalus++;
	}
	return count;
}

PathType AmphibiousNodeEvaluator::getPathType(PathfindingContext& context, int x, int y, int z) {
	PathType type = context.getPathTypeFromState(x, y, z);
	if (type != PathType::Water) return WalkNodeEvaluator::getPathType(context, x, y, z);
	for (Direction direction : Directions::ALL) {
		PathType neighbor = context.getPathTypeFromState(x + Directions::stepX(direction), y + Directions::stepY(direction), z + Directions::stepZ(direction));
		if (neighbor == PathType::Blocked) return PathType::WaterBorder;
	}
	return PathType::Water;
}

// ===================== WaterBoundPathNavigation =====================

std::unique_ptr<PathFinder> WaterBoundPathNavigation::createPathFinder(int maxVisitedNodes) {
	_allowBreaching = _level.gameData().getStaticName("minecraft:entity_type", _mob.typeId()) == "minecraft:dolphin";
	auto evaluator	= std::make_unique<SwimNodeEvaluator>(_allowBreaching);
	evaluator->setCanPassDoors(false);
	return std::make_unique<PathFinder>(std::move(evaluator), maxVisitedNodes);
}

bool WaterBoundPathNavigation::canUpdatePath() { return _allowBreaching || _mob.isInLiquid(); }

Vec3 WaterBoundPathNavigation::getTempMobPos() { return {_mob.position().x, _mob.position().y + _mob.height() * 0.5, _mob.position().z}; }

bool WaterBoundPathNavigation::canMoveDirectly(const Vec3& from, const Vec3& to) { return isClearForMovementBetween(_mob, from, to, false); }

bool WaterBoundPathNavigation::isStableDestination(const BlockPos& pos) { return !_level.gameData().getStateProperties(_level.getBlockState(pos)).solidRender; }

// ===================== AmphibiousPathNavigation =====================

std::unique_ptr<PathFinder> AmphibiousPathNavigation::createPathFinder(int maxVisitedNodes) {
	return std::make_unique<PathFinder>(std::make_unique<AmphibiousNodeEvaluator>(prefersShallowSwimming()), maxVisitedNodes);
}

Vec3 AmphibiousPathNavigation::getTempMobPos() { return {_mob.position().x, _mob.position().y + _mob.height() * 0.5, _mob.position().z}; }

bool AmphibiousPathNavigation::canMoveDirectly(const Vec3& from, const Vec3& to) {
	return _mob.isInLiquid() ? isClearForMovementBetween(_mob, from, to, false) : false;
}

bool AmphibiousPathNavigation::isStableDestination(const BlockPos& pos) { return !_level.blocks().isAir(_level.getBlockState(pos.below())); }

// ===================== SmoothSwimmingMoveControl =====================

void SmoothSwimmingMoveControl::tick() {
	if (_applyGravity && _mob.isInWaterNow()) _mob.setDeltaMovement(_mob.deltaMovement() + Vec3{0.0, 0.005, 0.0});
	if (_operation != Operation::MoveTo || _mob.navigation().isDone()) {
		_mob.setSpeed(0.0F);
		_mob.setXxa(0.0F);
		_mob.setYya(0.0F);
		_mob.setZza(0.0F);
		return;
	}
	double dx = _wantedX - _mob.position().x, dy = _wantedY - _mob.position().y, dz = _wantedZ - _mob.position().z;
	if (dx * dx + dy * dy + dz * dz < 2.5000003E-7F) {
		_mob.setZza(0.0F);
		return;
	}
	float yaw = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
	_mob.setYRotPublic(rotlerp(_mob.yRot(), yaw, static_cast<float>(_maxTurnY)));
	_mob.setYBodyRot(_mob.yRot());
	_mob.setYHeadRot(_mob.yRot());
	float speed = static_cast<float>(_speedModifier * _mob.getAttributeValue(_mob.attributeIds().movementSpeed));
	if (_mob.isInWaterNow()) {
		_mob.setSpeed(speed * _inWaterSpeedModifier);
		double horizontal = std::sqrt(dx * dx + dz * dz);
		if (std::abs(dy) > 1.0E-5F || std::abs(horizontal) > 1.0E-5F) {
			float pitch = -static_cast<float>(Mth::atan2(dy, horizontal) * 180.0F / (float)M_PI);
			pitch		= Mth::clamp(Mth::wrapDegrees(pitch), static_cast<float>(-_maxTurnX), static_cast<float>(_maxTurnX));
			// Control.rotateTowards
			_mob.setXRot(_mob.xRot() + Mth::clamp(Mth::degreesDifference(_mob.xRot(), pitch), -5.0F, 5.0F));
		}
		const float toRadians = static_cast<float>(M_PI / 180.0);
		_mob.setZza(Mth::cos(_mob.xRot() * toRadians) * speed);
		_mob.setYya(-Mth::sin(_mob.xRot() * toRadians) * speed);
	} else {
		float turn = std::abs(Mth::wrapDegrees(_mob.yRot() - yaw));
		_mob.setSpeed(speed * _outsideWaterSpeedModifier * (1.0F - Mth::clamp((turn - 10.0F) / 50.0F, 0.0F, 1.0F)));
	}
}

// ===================== SmoothSwimmingLookControl =====================

void SmoothSwimmingLookControl::tick() {
	if (_lookAtCooldown > 0) {
		_lookAtCooldown--;
		const Vec3& position = _mob.position();
		double		dx = _wantedX - position.x, dy = _wantedY - _mob.eyeY(), dz = _wantedZ - position.z;
		if (std::abs(dz) > 1.0E-5F || std::abs(dx) > 1.0E-5F) {
			float yRot = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
			_mob.setYHeadRot(rotateTowards(_mob.yHeadRot(), yRot + 20.0F, _yMaxRotSpeed));
		}
		double horizontal = std::sqrt(dx * dx + dz * dz);
		if (std::abs(dy) > 1.0E-5F || std::abs(horizontal) > 1.0E-5F) {
			float xRot = static_cast<float>(-(Mth::atan2(dy, horizontal) * 180.0F / (float)M_PI));
			_mob.setXRot(rotateTowards(_mob.xRot(), xRot + 10.0F, _xMaxRotAngle));
		}
	} else {
		if (_mob.navigation().isDone()) _mob.setXRot(rotateTowards(_mob.xRot(), 0.0F, 5.0F));
		_mob.setYHeadRot(rotateTowards(_mob.yHeadRot(), _mob.yBodyRot(), _yMaxRotSpeed));
	}
	float offset = Mth::wrapDegrees(_mob.yHeadRot() - _mob.yBodyRot());
	if (offset < -_maxYRotFromCenter) {
		_mob.setYBodyRot(_mob.yBodyRot() - 4.0F);
	} else if (offset > _maxYRotFromCenter) {
		_mob.setYBodyRot(_mob.yBodyRot() + 4.0F);
	}
}
