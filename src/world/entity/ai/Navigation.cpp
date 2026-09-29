#include "world/entity/ai/Navigation.hpp"

#include "data/GameData.hpp"
#include "world/Clip.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"

#include <algorithm>
#include <cmath>

// ===================== PathNavigation =====================

PathFinder& PathNavigation::pathFinder() {
	if (!_pathFinder) _pathFinder = createPathFinder(Mth::floor(_mob.attributes().getBaseValue(_mob.attributeIds().followRange) * 16.0));
	return *_pathFinder;
}

NodeEvaluator& PathNavigation::getNodeEvaluator() { return pathFinder().evaluator(); }

float PathNavigation::getMaxPathLength() { return std::max(static_cast<float>(_mob.getAttributeValue(_mob.attributeIds().followRange)), _requiredPathLength); }

void PathNavigation::updatePathfinderMaxVisitedNodes() { pathFinder().setMaxVisitedNodes(Mth::floor(getMaxPathLength() * 16.0F)); }

void PathNavigation::setRequiredPathLength(float length) {
	_requiredPathLength = length;
	updatePathfinderMaxVisitedNodes();
}

void PathNavigation::recomputePath() {
	if (_level.getGameTime() - _timeLastRecompute > 20) {
		if (_targetPos) {
			_path.reset();
			_path			   = createPath(*_targetPos, _reachRange);
			_timeLastRecompute = _level.getGameTime();
			_hasDelayedRecomputation = false;
		}
	} else {
		_hasDelayedRecomputation = true;
	}
}

std::unique_ptr<Path> PathNavigation::createPath(double x, double y, double z, int reach) {
	return createPath(BlockPos{Mth::floor(x), Mth::floor(y), Mth::floor(z)}, reach);
}

std::unique_ptr<Path> PathNavigation::createPath(const BlockPos& pos, int reach) { return createPath(std::vector<BlockPos>{pos}, 8, false, reach, getMaxPathLength()); }

std::unique_ptr<Path> PathNavigation::createPath(const std::vector<BlockPos>& positions, int reach) {
	return createPath(positions, 8, false, reach, getMaxPathLength());
}

std::unique_ptr<Path> PathNavigation::createPath(Actor& target, int reach) {
	BlockPos pos{Mth::floor(target.position().x), Mth::floor(target.position().y), Mth::floor(target.position().z)};
	return createPath(std::vector<BlockPos>{pos}, 16, true, reach, getMaxPathLength());
}

std::unique_ptr<Path> PathNavigation::createPath(const std::vector<BlockPos>& positions, int, bool, int reach, float maxPathLength) {
	if (positions.empty() || _mob.position().y < _level.minY() || !canUpdatePath()) return nullptr;
	// The current path already goes there
	if (_path && !_path->isDone() && _targetPos && std::find(positions.begin(), positions.end(), *_targetPos) != positions.end()) {
		return std::make_unique<Path>(*_path);
	}
	std::unique_ptr<Path> path = pathFinder().findPath(_level, _mob, positions, maxPathLength, reach, _maxVisitedNodesMultiplier);
	if (path) {
		_targetPos	= path->getTarget();
		_reachRange = reach;
		resetStuckTimeout();
	}
	return path;
}

bool PathNavigation::moveTo(double x, double y, double z, double speed) { return moveTo(createPath(x, y, z, 1), speed); }

bool PathNavigation::moveTo(double x, double y, double z, int reach, double speed) { return moveTo(createPath(x, y, z, reach), speed); }

bool PathNavigation::moveTo(Actor& target, double speed) {
	std::unique_ptr<Path> path = createPath(target, 1);
	return path && moveTo(std::move(path), speed);
}

bool PathNavigation::moveTo(std::unique_ptr<Path> path, double speed) {
	if (!path) {
		_path.reset();
		return false;
	}
	if (!path->sameAs(_path.get())) _path = std::move(path);
	if (isDone()) return false;
	trimPath();
	if (_path->getNodeCount() <= 0) return false;
	_speedModifier	   = speed;
	_lastStuckCheck	   = _tick;
	_lastStuckCheckPos = getTempMobPos();
	return true;
}

void PathNavigation::tick() {
	_tick++;
	if (_hasDelayedRecomputation) recomputePath();
	if (isDone()) return;
	if (canUpdatePath()) {
		followThePath();
	} else if (_path && !_path->isDone()) {
		Vec3 position = getTempMobPos();
		Vec3 next	  = _path->getNextEntityPos(_mob.width());
		if (position.y > next.y && !_mob.onGround() && Mth::floor(position.x) == Mth::floor(next.x) && Mth::floor(position.z) == Mth::floor(next.z)) {
			_path->advance();
		}
	}
	if (!isDone()) {
		Vec3 next = _path->getNextEntityPos(_mob.width());
		_mob.moveControl().setWantedPosition(next.x, getGroundY(next), next.z, _speedModifier);
	}
}

double PathNavigation::getGroundY(const Vec3& position) {
	BlockPos pos{Mth::floor(position.x), Mth::floor(position.y), Mth::floor(position.z)};
	return _level.blocks().isAir(_level.getBlockState(pos.below())) ? position.y : WalkNodeEvaluator::getFloorLevel(_level, pos);
}

void PathNavigation::followThePath() {
	Vec3  position = getTempMobPos();
	float width	   = _mob.width();
	_maxDistanceToWaypoint = width > 0.75F ? width / 2.0F : 0.75F - width / 2.0F;
	BlockPos next		   = _path->getNextNodePos();
	double	 dx = std::abs(_mob.position().x - (next.x + 0.5)), dy = std::abs(_mob.position().y - next.y), dz = std::abs(_mob.position().z - (next.z + 0.5));
	bool	 close = dx < _maxDistanceToWaypoint && dz < _maxDistanceToWaypoint && dy < 1.0;
	if (close || (canCutCorner(_path->getNextNode().type) && shouldTargetNextNodeInDirection(position))) _path->advance();
	doStuckDetection(position);
}

bool PathNavigation::shouldTargetNextNodeInDirection(const Vec3& position) {
	if (_path->getNextNodeIndex() + 1 >= _path->getNodeCount()) return false;
	BlockPos nextPos = _path->getNextNodePos();
	Vec3	 next{nextPos.x + 0.5, static_cast<double>(nextPos.y), nextPos.z + 0.5};
	if (!((next - position).lengthSqr() < 4.0)) return false; // closerThan(next, 2)
	if (canMoveDirectly(position, _path->getNextEntityPos(_mob.width()))) return true;
	BlockPos afterPos = _path->getNodePos(_path->getNextNodeIndex() + 1);
	Vec3	 after{afterPos.x + 0.5, static_cast<double>(afterPos.y), afterPos.z + 0.5};
	Vec3	 toNext = next - position, toAfter = after - position;
	double	 nextSqr = toNext.lengthSqr(), afterSqr = toAfter.lengthSqr();
	bool	 afterCloser = afterSqr < nextSqr, nextVeryClose = nextSqr < 0.5;
	if (!afterCloser && !nextVeryClose) return false;
	Vec3 a = toNext.normalize(), b = toAfter.normalize();
	return b.x * a.x + b.y * a.y + b.z * a.z < 0.0;
}

void PathNavigation::doStuckDetection(const Vec3& position) {
	if (_tick - _lastStuckCheck > 100) {
		float speed		= _mob.speed() >= 1.0F ? _mob.speed() : _mob.speed() * _mob.speed();
		float threshold = speed * 100.0F * 0.25F;
		if ((position - _lastStuckCheckPos).lengthSqr() < threshold * threshold) {
			_isStuck = true;
			stop();
		} else {
			_isStuck = false;
		}
		_lastStuckCheck	   = _tick;
		_lastStuckCheckPos = position;
	}
	if (_path && !_path->isDone()) {
		BlockPos next = _path->getNextNodePos();
		int64_t	 now  = _level.getGameTime();
		if (next == _timeoutCachedNode) {
			_timeoutTimer += now - _lastTimeoutCheck;
		} else {
			_timeoutCachedNode = next;
			double distance	   = (position - Vec3{next.x + 0.5, static_cast<double>(next.y), next.z + 0.5}).length();
			_timeoutLimit	   = _mob.speed() > 0.0F ? distance / _mob.speed() * 20.0 : 0.0;
		}
		if (_timeoutLimit > 0.0 && _timeoutTimer > _timeoutLimit * 3.0) {
			resetStuckTimeout();
			stop();
		}
		_lastTimeoutCheck = now;
	}
}

void PathNavigation::resetStuckTimeout() {
	_timeoutCachedNode = {};
	_timeoutTimer	   = 0;
	_timeoutLimit	   = 0.0;
	_isStuck		   = false;
}

void PathNavigation::trimPath() {
	if (!_path) return;
	static std::vector<bool> cauldrons;
	static const GameData*	 cached = nullptr;
	if (cached != &_level.gameData()) {
		cached	  = &_level.gameData();
		cauldrons = cached->blockTag("minecraft:cauldrons");
	}
	for (int i = 0; i < _path->getNodeCount(); i++) {
		Path::PathNode node = _path->getNode(i);
		if (!cauldrons[_level.blocks().blockOf(_level.getBlockState(node.asBlockPos()))]) continue;
		// Walk on top of cauldrons, not into them
		_path->replaceNode(i, {node.x, node.y + 1, node.z, node.type});
		if (i + 1 < _path->getNodeCount()) {
			Path::PathNode next = _path->getNode(i + 1);
			if (node.y >= next.y) _path->replaceNode(i + 1, {next.x, node.y + 1, next.z, node.type});
		}
	}
}

bool PathNavigation::isClearForMovementBetween(Mob& mob, const Vec3& from, const Vec3& to, bool fluids) {
	Vec3 target{to.x, to.y + mob.height() * 0.5, to.z};
	return !Clip::clip(mob.level(), from, target, Clip::BlockMode::Collider, fluids ? Clip::FluidMode::Any : Clip::FluidMode::None).hit;
}

bool PathNavigation::isStableDestination(const BlockPos& pos) { return _level.gameData().getStateProperties(_level.getBlockState(pos.below())).solidRender; }

bool PathNavigation::shouldRecomputePath(const BlockPos& pos) const {
	if (_hasDelayedRecomputation || !_path || _path->isDone() || _path->getNodeCount() == 0) return false;
	const Path::PathNode* end = _path->getEndNode();
	Vec3				  mid{(end->x + _mob.position().x) / 2.0, (end->y + _mob.position().y) / 2.0, (end->z + _mob.position().z) / 2.0};
	double				  radius = _path->getNodeCount() - _path->getNextNodeIndex();
	Vec3				  center{pos.x + 0.5, pos.y + 0.5, pos.z + 0.5};
	return (center - mid).lengthSqr() < radius * radius; // closerToCenterThan
}

// ===================== GroundPathNavigation =====================

std::unique_ptr<PathFinder> GroundPathNavigation::createPathFinder(int maxVisitedNodes) {
	return std::make_unique<PathFinder>(std::make_unique<WalkNodeEvaluator>(), maxVisitedNodes);
}

bool GroundPathNavigation::canUpdatePath() { return _mob.onGround() || _mob.isInLiquid(); }

Vec3 GroundPathNavigation::getTempMobPos() { return {_mob.position().x, static_cast<double>(getSurfaceY()), _mob.position().z}; }

std::unique_ptr<Path> GroundPathNavigation::createPath(const BlockPos& target, int reach) {
	if (!_level.loadedChunk(target.chunkX(), target.chunkZ())) return nullptr;
	BlockPos pos = _canPathToTargetsBelowSurface ? target : findSurfacePosition(target);
	return PathNavigation::createPath(pos, reach);
}

std::unique_ptr<Path> GroundPathNavigation::createPath(Actor& target, int reach) {
	return createPath(BlockPos{Mth::floor(target.position().x), Mth::floor(target.position().y), Mth::floor(target.position().z)}, reach);
}

BlockPos GroundPathNavigation::findSurfacePosition(const BlockPos& target) {
	BlockPos pos = target;
	if (_level.blocks().isAir(_level.getBlockState(pos))) {
		BlockPos below = pos.below();
		while (below.y >= _level.minY() && _level.blocks().isAir(_level.getBlockState(below))) below = below.below();
		if (below.y >= _level.minY()) return below.above();
		BlockPos above{pos.x, pos.y + 1, pos.z};
		while (above.y <= _level.maxY() - 1 && _level.blocks().isAir(_level.getBlockState(above))) above = above.above();
		pos = above;
	}
	if (!_level.gameData().getStateProperties(_level.getBlockState(pos)).solid) return pos;
	BlockPos up = pos.above();
	while (up.y <= _level.maxY() - 1 && _level.gameData().getStateProperties(_level.getBlockState(up)).solid) up = up.above();
	return up;
}

int GroundPathNavigation::getSurfaceY() {
	if (_mob.isInWaterNow() && canFloat()) {
		int y	  = _mob.blockPosition().y;
		int water = _level.gameData().getStaticId("minecraft:block", "minecraft:water");
		int state = _level.getBlockState({Mth::floor(_mob.position().x), y, Mth::floor(_mob.position().z)});
		int count = 0;
		while (_level.blocks().blockOf(state) == water) {
			state = _level.getBlockState({Mth::floor(_mob.position().x), ++y, Mth::floor(_mob.position().z)});
			if (++count > 16) return _mob.blockPosition().y;
		}
		return y;
	}
	return Mth::floor(_mob.position().y + 0.5);
}

void GroundPathNavigation::trimPath() {
	PathNavigation::trimPath();
	if (!_avoidSun) return;
	if (_level.canSeeSky({Mth::floor(_mob.position().x), Mth::floor(_mob.position().y + 0.5), Mth::floor(_mob.position().z)})) return;
	for (int i = 0; i < _path->getNodeCount(); i++) {
		if (_level.canSeeSky(_path->getNode(i).asBlockPos())) {
			_path->truncateNodes(i);
			return;
		}
	}
}

// ----- WallClimberNavigation -----

namespace {
	// Vec3i.closerToCenterThan
	bool closerToCenterThan(const BlockPos& pos, const Vec3& position, double distance) {
		Vec3 center{pos.x + 0.5, pos.y + 0.5, pos.z + 0.5};
		return (center - position).lengthSqr() < distance * distance;
	}
} // namespace

std::unique_ptr<Path> WallClimberNavigation::createPath(const BlockPos& pos, int reach) {
	_pathToPosition = pos;
	return GroundPathNavigation::createPath(pos, reach);
}

std::unique_ptr<Path> WallClimberNavigation::createPath(Actor& target, int reach) {
	const Vec3& p	= target.position();
	_pathToPosition = BlockPos{Mth::floor(p.x), Mth::floor(p.y), Mth::floor(p.z)};
	return GroundPathNavigation::createPath(target, reach);
}

bool WallClimberNavigation::moveTo(Actor& target, double speed) {
	std::unique_ptr<Path> path = createPath(target, 0);
	if (path) return PathNavigation::moveTo(std::move(path), speed);
	const Vec3& p	= target.position();
	_pathToPosition = BlockPos{Mth::floor(p.x), Mth::floor(p.y), Mth::floor(p.z)};
	_speedModifier	= speed;
	return true;
}

void WallClimberNavigation::tick() {
	if (!isDone()) {
		GroundPathNavigation::tick();
		return;
	}
	if (!_pathToPosition) return;
	const Vec3& pos	  = _mob.position();
	double		width = _mob.width();
	const BlockPos& to = *_pathToPosition;
	if (!closerToCenterThan(to, pos, width) &&
		(!(pos.y > to.y) || !closerToCenterThan(BlockPos{to.x, Mth::floor(pos.y), to.z}, pos, width))) {
		_mob.moveControl().setWantedPosition(to.x, to.y, to.z, _speedModifier);
	} else {
		_pathToPosition.reset();
	}
}
