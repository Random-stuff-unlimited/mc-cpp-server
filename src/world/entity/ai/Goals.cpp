#include "world/entity/ai/Goals.hpp"

#include "data/GameData.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/Arrow.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/ai/RandomPos.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

// ===================== Targeting =====================

bool isOfClass(Actor& actor, const std::string& javaClass) {
	if (actor.isPlayer()) return javaClass == "Player" || javaClass == "LivingEntity" || javaClass == "Entity";
	Level* level = actor.actorLevel();
	if (!level) return false;
	const GameData::EntityTypeInfo* type = level->gameData().getEntityType(actor.typeId());
	return type && type->is(javaClass);
}

double visibilityPercent(Actor& target, Actor*) {
	double percent = 1.0;
	if (Player* player = target.asPlayer()) {
		if (player->isShiftKeyDown()) percent *= 0.8; // isDiscrete
	}
	return percent;
}

bool notCreativeOrSpectator(Actor& actor) {
	Player* player = actor.asPlayer();
	return !player || (!player->isSpectator() && !player->isCreative());
}

bool TargetingConditions::test(Level& level, Mob* looker, Actor& target) const {
	if (looker == &target) return false;
	if (target.isSpectator() || !target.isAlive()) return false; // canBeSeenByAnyone
	if (selector && !selector(target, level)) return false;
	if (!looker) {
		if (isCombat) {
			Player* player = target.asPlayer();
			if ((player && player->isCreative()) || level.difficulty() == 0) return false; // canBeSeenAsEnemy
		}
		return true;
	}
	if (isCombat && (!looker->canAttack(target) || !looker->canAttackType(target.typeId()))) return false;
	if (range > 0.0) {
		double visibility = testInvisible ? visibilityPercent(target, looker) : 1.0;
		double reach	  = std::max(range * visibility, 2.0);
		if (looker->distanceToSqr(target.position()) > reach * reach) return false;
	}
	if (checkLineOfSight && !looker->sensing().hasLineOfSight(target)) return false;
	return true;
}

Actor* nearestPlayer(Level& level, const TargetingConditions& conditions, Mob* looker, double x, double y, double z) {
	Actor* best		= nullptr;
	double bestDist = -1.0;
	for (const auto& player : level.players()) {
		if (player->isDisconnected() || !conditions.test(level, looker, *player)) continue;
		double d = (player->position() - Vec3{x, y, z}).lengthSqr();
		if (bestDist == -1.0 || d < bestDist) {
			bestDist = d;
			best	 = player.get();
		}
	}
	return best;
}

Actor* nearestActor(Level& level, const std::vector<Actor*>& candidates, const TargetingConditions& conditions, Mob* looker, double x, double y, double z) {
	Actor* best		= nullptr;
	double bestDist = -1.0;
	for (Actor* actor : candidates) {
		if (!conditions.test(level, looker, *actor)) continue;
		double d = (actor->position() - Vec3{x, y, z}).lengthSqr();
		if (bestDist == -1.0 || d < bestDist) {
			bestDist = d;
			best	 = actor;
		}
	}
	return best;
}

std::vector<Actor*> actorsOfClass(Level& level, const std::string& javaClass, const AABB& box, const std::function<bool(Actor&)>& filter) {
	std::vector<Actor*> found;
	for (Actor* actor : level.actorsIn(box)) {
		if (isOfClass(*actor, javaClass) && (!filter || filter(*actor))) found.push_back(actor);
	}
	return found;
}

std::optional<BlockPos> findClosestMatch(const BlockPos& center, int horizontal, int vertical, const std::function<bool(const BlockPos&)>& test) {
	// BlockPos.withinManhattan(center, horizontal, vertical, horizontal): by manhattan distance, x then y, z mirrored
	int maxDepth = horizontal + vertical + horizontal;
	for (int depth = 0; depth <= maxDepth; depth++) {
		int maxX = std::min(horizontal, depth);
		for (int x = -maxX; x <= maxX; x++) {
			int maxY = std::min(vertical, depth - std::abs(x));
			for (int y = -maxY; y <= maxY; y++) {
				int z = depth - std::abs(x) - std::abs(y);
				if (z > horizontal) continue;
				BlockPos pos{center.x + x, center.y + y, center.z + z};
				if (test(pos)) return pos;
				if (z != 0) {
					BlockPos mirrored{center.x + x, center.y + y, center.z - z};
					if (test(mirrored)) return mirrored;
				}
			}
		}
	}
	return std::nullopt;
}

// ===================== Movement =====================

FloatGoal::FloatGoal(Mob& mob) : _mob(mob) {
	setFlags({Flag::Jump});
	mob.navigation().setCanFloat(true);
}

bool FloatGoal::canUse() { return (_mob.isInWaterNow() && _mob.waterHeight() > _mob.fluidJumpThreshold()) || _mob.isInLavaNow(); }

void FloatGoal::tick() {
	if (_mob.random().nextFloat() < 0.8F) _mob.jumpControl().jump();
}

RandomStrollGoal::RandomStrollGoal(Mob& mob, double speed, int interval, bool checkNoActionTime)
	: _mob(mob), _speedModifier(speed), _interval(interval), _checkNoActionTime(checkNoActionTime) {
	setFlags({Flag::Move});
}

bool RandomStrollGoal::canUse() {
	if (!_forceTrigger) {
		if (_checkNoActionTime && _mob.noActionTime() >= 100) return false;
		if (_mob.random().nextInt(reducedTickDelay(_interval)) != 0) return false;
	}
	std::optional<Vec3> pos = getPosition();
	if (!pos) return false;
	_wantedX	  = pos->x;
	_wantedY	  = pos->y;
	_wantedZ	  = pos->z;
	_forceTrigger = false;
	return true;
}

std::optional<Vec3> RandomStrollGoal::getPosition() { return RandomPos::defaultPos(_mob, 10, 7); }

bool RandomStrollGoal::canContinueToUse() { return !_mob.navigation().isDone(); }

void RandomStrollGoal::start() { _mob.navigation().moveTo(_wantedX, _wantedY, _wantedZ, _speedModifier); }

void RandomStrollGoal::stop() { _mob.navigation().stop(); }

std::optional<Vec3> WaterAvoidingRandomStrollGoal::getPosition() {
	if (_mob.isInWaterNow()) {
		std::optional<Vec3> land = RandomPos::landPos(_mob, 15, 7);
		return land ? land : RandomStrollGoal::getPosition();
	}
	return _mob.random().nextFloat() >= _probability ? RandomPos::landPos(_mob, 10, 7) : RandomStrollGoal::getPosition();
}

LookAtPlayerGoal::LookAtPlayerGoal(Mob& mob, std::string lookAtClass, float distance, float probability, bool onlyHorizontal)
	: _mob(mob), _lookDistance(distance), _probability(probability), _onlyHorizontal(onlyHorizontal), _lookAtClass(std::move(lookAtClass)) {
	setFlags({Flag::Look});
	_lookAtContext = TargetingConditions::forNonCombat().withRange(distance);
}

bool LookAtPlayerGoal::canUse() {
	if (_mob.random().nextFloat() >= _probability) return false;
	Actor* found = nullptr;
	if (Actor* target = _mob.getTarget()) found = target;
	if (_lookAtClass == "Player") {
		found = nearestPlayer(_mob.level(), _lookAtContext, &_mob, _mob.position().x, _mob.eyeY(), _mob.position().z);
	} else {
		AABB box = _mob.boundingBox().inflate(_lookDistance, 3.0, _lookDistance);
		found	 = nearestActor(_mob.level(), actorsOfClass(_mob.level(), _lookAtClass, box), _lookAtContext, &_mob, _mob.position().x, _mob.eyeY(), _mob.position().z);
	}
	if (!found) return false;
	_lookAt = EntityRef(*found);
	return true;
}

bool LookAtPlayerGoal::canContinueToUse() {
	Actor* target = _mob.level().actorByRef(_lookAt);
	if (!target || !target->isAlive()) return false;
	if (_mob.distanceToSqr(*target) > _lookDistance * _lookDistance) return false;
	return _lookTime > 0;
}

void LookAtPlayerGoal::start() { _lookTime = adjustedTickDelay(40 + _mob.random().nextInt(40)); }

void LookAtPlayerGoal::tick() {
	Actor* target = _mob.level().actorByRef(_lookAt);
	if (!target || !target->isAlive()) return;
	double y = _onlyHorizontal ? _mob.eyeY() : target->eyeY();
	_mob.lookControl().setLookAt(target->position().x, y, target->position().z);
	_lookTime--;
}

RandomLookAroundGoal::RandomLookAroundGoal(Mob& mob) : _mob(mob) { setFlags({Flag::Move, Flag::Look}); }

bool RandomLookAroundGoal::canUse() { return _mob.random().nextFloat() < 0.02F; }

void RandomLookAroundGoal::start() {
	double angle = (M_PI * 2) * _mob.random().nextDouble();
	_relX		 = std::cos(angle);
	_relZ		 = std::sin(angle);
	_lookTime	 = 20 + _mob.random().nextInt(20);
}

void RandomLookAroundGoal::tick() {
	_lookTime--;
	_mob.lookControl().setLookAt(_mob.position().x + _relX, _mob.eyeY(), _mob.position().z + _relZ);
}

PanicGoal::PanicGoal(Mob& mob, double speed, std::string panicTag) : _mob(mob), _speedModifier(speed), _panicTag(std::move(panicTag)) {
	setFlags({Flag::Move});
}

bool PanicGoal::shouldPanic() {
	std::string type = _mob.getLastDamageType();
	if (type.empty()) return false;
	const GameData& data = _mob.level().gameData();
	return data.isInTag("minecraft:damage_type", _panicTag, data.getSyncedId("minecraft:damage_type", type));
}

bool PanicGoal::canUse() {
	if (!shouldPanic()) return false;
	if (_mob.isOnFire()) {
		if (std::optional<BlockPos> water = lookForWater(5)) {
			_posX = water->x;
			_posY = water->y;
			_posZ = water->z;
			return true;
		}
	}
	return findRandomPosition();
}

bool PanicGoal::findRandomPosition() {
	std::optional<Vec3> pos = RandomPos::defaultPos(_mob, 5, 4);
	if (!pos) return false;
	_posX = pos->x;
	_posY = pos->y;
	_posZ = pos->z;
	return true;
}

void PanicGoal::start() {
	_mob.navigation().moveTo(_posX, _posY, _posZ, _speedModifier);
	_isRunning = true;
}

bool PanicGoal::canContinueToUse() { return !_mob.navigation().isDone(); }

std::optional<BlockPos> PanicGoal::lookForWater(int distance) {
	Level&	 level = _mob.level();
	BlockPos pos   = _mob.blockPosition();
	if (!level.gameData().getCollisionShape(level.getBlockState(pos)).empty()) return std::nullopt;
	return findClosestMatch(pos, distance, 1, [&](const BlockPos& at) { return level.fluids().isWater(level.getFluidState(at).type); });
}

// ----- Melee -----

MeleeAttackGoal::MeleeAttackGoal(Mob& mob, double speed, bool followingTargetEvenIfNotSeen)
	: _mob(mob), _speedModifier(speed), _followingTargetEvenIfNotSeen(followingTargetEvenIfNotSeen) {
	setFlags({Flag::Move, Flag::Look});
}

bool MeleeAttackGoal::canUse() {
	int64_t now = _mob.level().getGameTime();
	if (now - _lastCanUseCheck < 20) return false;
	_lastCanUseCheck = now;
	Actor* target	 = _mob.getTarget();
	if (!target || !target->isAlive()) return false;
	_path = _mob.navigation().createPath(*target, 0);
	return _path ? true : _mob.isWithinMeleeAttackRange(*target);
}

bool MeleeAttackGoal::canContinueToUse() {
	Actor* target = _mob.getTarget();
	if (!target || !target->isAlive()) return false;
	if (!_followingTargetEvenIfNotSeen) return !_mob.navigation().isDone();
	if (!_mob.isWithinHome(BlockPos{Mth::floor(target->position().x), Mth::floor(target->position().y), Mth::floor(target->position().z)})) return false;
	return notCreativeOrSpectator(*target);
}

void MeleeAttackGoal::start() {
	_mob.navigation().moveTo(std::move(_path), _speedModifier);
	_mob.setAggressive(true);
	_ticksUntilNextPathRecalculation = 0;
	_ticksUntilNextAttack			 = 0;
}

void MeleeAttackGoal::stop() {
	Actor* target = _mob.getTarget();
	if (target && !notCreativeOrSpectator(*target)) _mob.setTarget(nullptr);
	_mob.setAggressive(false);
	_mob.navigation().stop();
}

void MeleeAttackGoal::tick() {
	Actor* target = _mob.getTarget();
	if (!target) return;
	_mob.lookControl().setLookAt(*target, 30.0F, 30.0F);
	_ticksUntilNextPathRecalculation = std::max(_ticksUntilNextPathRecalculation - 1, 0);
	const Vec3& at					 = target->position();
	if ((_followingTargetEvenIfNotSeen || _mob.sensing().hasLineOfSight(*target)) && _ticksUntilNextPathRecalculation <= 0 &&
		((_pathedTargetX == 0.0 && _pathedTargetY == 0.0 && _pathedTargetZ == 0.0) ||
		 target->distanceToSqr(Vec3{_pathedTargetX, _pathedTargetY, _pathedTargetZ}) >= 1.0 || _mob.random().nextFloat() < 0.05F)) {
		_pathedTargetX					 = at.x;
		_pathedTargetY					 = at.y;
		_pathedTargetZ					 = at.z;
		_ticksUntilNextPathRecalculation = 4 + _mob.random().nextInt(7);
		double distance					 = _mob.distanceToSqr(*target);
		if (distance > 1024.0) {
			_ticksUntilNextPathRecalculation += 10;
		} else if (distance > 256.0) {
			_ticksUntilNextPathRecalculation += 5;
		}
		if (!_mob.navigation().moveTo(*target, _speedModifier)) _ticksUntilNextPathRecalculation += 15;
		_ticksUntilNextPathRecalculation = adjustedTickDelay(_ticksUntilNextPathRecalculation);
	}
	_ticksUntilNextAttack = std::max(_ticksUntilNextAttack - 1, 0);
	checkAndPerformAttack(*target);
}

void MeleeAttackGoal::checkAndPerformAttack(Actor& target) {
	if (!canPerformAttack(target)) return;
	resetAttackCooldown();
	_mob.swing(0);
	_mob.doHurtTarget(target);
}

bool MeleeAttackGoal::canPerformAttack(Actor& target) {
	return isTimeToAttack() && _mob.isWithinMeleeAttackRange(target) && _mob.sensing().hasLineOfSight(target);
}

void ZombieAttackGoal::start() {
	MeleeAttackGoal::start();
	_raiseArmTicks = 0;
}

void ZombieAttackGoal::stop() {
	MeleeAttackGoal::stop();
	_mob.setAggressive(false);
}

void ZombieAttackGoal::tick() {
	MeleeAttackGoal::tick();
	_raiseArmTicks++;
	_mob.setAggressive(_raiseArmTicks >= 5 && getTicksUntilNextAttack() < getAttackInterval() / 2);
}

LeapAtTargetGoal::LeapAtTargetGoal(Mob& mob, float yd) : _mob(mob), _yd(yd) { setFlags({Flag::Jump, Flag::Move}); }

bool LeapAtTargetGoal::canUse() {
	Actor* target = _mob.getTarget();
	if (!target) return false;
	_target			= EntityRef(*target);
	double distance = _mob.distanceToSqr(*target);
	if (distance < 4.0 || distance > 16.0) return false;
	return _mob.onGround() && _mob.random().nextInt(reducedTickDelay(5)) == 0;
}

bool LeapAtTargetGoal::canContinueToUse() { return !_mob.onGround(); }

void LeapAtTargetGoal::start() {
	Actor* target = _mob.level().actorByRef(_target);
	if (!target) return;
	Vec3 delta = _mob.deltaMovement();
	Vec3 toward{target->position().x - _mob.position().x, 0.0, target->position().z - _mob.position().z};
	if (toward.lengthSqr() > 1.0E-7) toward = toward.normalize().scale(0.4) + delta.scale(0.2);
	_mob.setDeltaMovement({toward.x, _yd, toward.z});
}

AvoidEntityGoal::AvoidEntityGoal(Mob& mob, std::string avoidClass, float maxDist, double walkSpeed, double sprintSpeed,
								 std::function<bool(Actor&)> avoidPredicate, std::function<bool(Actor&)> predicateOnAvoidEntity)
	: _mob(mob), _walkSpeedModifier(walkSpeed), _sprintSpeedModifier(sprintSpeed), _maxDist(maxDist), _avoidClass(std::move(avoidClass)) {
	setFlags({Flag::Move});
	_avoidEntityTargeting = TargetingConditions::forCombat().withRange(maxDist).withSelector([avoidPredicate, predicateOnAvoidEntity](Actor& actor, Level&) {
		return (!predicateOnAvoidEntity || predicateOnAvoidEntity(actor)) && (!avoidPredicate || avoidPredicate(actor));
	});
}

bool AvoidEntityGoal::canUse() {
	AABB   box	   = _mob.boundingBox().inflate(_maxDist, 3.0, _maxDist);
	Actor* toAvoid = nearestActor(_mob.level(), actorsOfClass(_mob.level(), _avoidClass, box), _avoidEntityTargeting, &_mob, _mob.position().x,
								  _mob.position().y, _mob.position().z);
	if (!toAvoid) return false;
	_toAvoid			= EntityRef(*toAvoid);
	std::optional<Vec3> away = RandomPos::defaultPosAway(_mob, 16, 7, toAvoid->position());
	if (!away) return false;
	if (toAvoid->distanceToSqr(*away) < toAvoid->distanceToSqr(_mob)) return false;
	_path = _mob.navigation().createPath(away->x, away->y, away->z, 0);
	return _path != nullptr;
}

bool AvoidEntityGoal::canContinueToUse() { return !_mob.navigation().isDone(); }

void AvoidEntityGoal::start() { _mob.navigation().moveTo(std::move(_path), _walkSpeedModifier); }

void AvoidEntityGoal::tick() {
	Actor* toAvoid = _mob.level().actorByRef(_toAvoid);
	if (!toAvoid) return;
	_mob.navigation().setSpeedModifier(_mob.distanceToSqr(*toAvoid) < 49.0 ? _sprintSpeedModifier : _walkSpeedModifier);
}

MoveTowardsTargetGoal::MoveTowardsTargetGoal(Mob& mob, double speed, float within) : _mob(mob), _speedModifier(speed), _within(within) {
	setFlags({Flag::Move});
}

bool MoveTowardsTargetGoal::canUse() {
	Actor* target = _mob.getTarget();
	if (!target) return false;
	_target = EntityRef(*target);
	if (target->distanceToSqr(_mob) > _within * _within) return false;
	std::optional<Vec3> pos = RandomPos::defaultPosTowards(_mob, 16, 7, target->position(), (float)(M_PI / 2));
	if (!pos) return false;
	_wantedX = pos->x;
	_wantedY = pos->y;
	_wantedZ = pos->z;
	return true;
}

bool MoveTowardsTargetGoal::canContinueToUse() {
	Actor* target = _mob.level().actorByRef(_target);
	return !_mob.navigation().isDone() && target && target->isAlive() && target->distanceToSqr(_mob) < _within * _within;
}

void MoveTowardsTargetGoal::start() { _mob.navigation().moveTo(_wantedX, _wantedY, _wantedZ, _speedModifier); }

bool RestrictSunGoal::canUse() {
	return _mob.level().isBrightOutside() && _mob.getItemBySlot(EquipmentSlot::Head).isEmpty() && _mob.navigation().canNavigateGround();
}

void RestrictSunGoal::start() {
	if (auto* ground = dynamic_cast<GroundPathNavigation*>(&_mob.navigation())) ground->setAvoidSun(true);
}

void RestrictSunGoal::stop() {
	if (auto* ground = dynamic_cast<GroundPathNavigation*>(&_mob.navigation())) ground->setAvoidSun(false);
}

FleeSunGoal::FleeSunGoal(Mob& mob, double speed) : _mob(mob), _speedModifier(speed) { setFlags({Flag::Move}); }

bool FleeSunGoal::canUse() {
	if (_mob.getTarget() || !_mob.level().isBrightOutside() || !_mob.isOnFire()) return false;
	if (!_mob.level().canSeeSky(_mob.blockPosition())) return false;
	return _mob.getItemBySlot(EquipmentSlot::Head).isEmpty() && setWantedPos();
}

bool FleeSunGoal::setWantedPos() {
	std::optional<Vec3> pos = getHidePos();
	if (!pos) return false;
	_wantedX = pos->x;
	_wantedY = pos->y;
	_wantedZ = pos->z;
	return true;
}

bool FleeSunGoal::canContinueToUse() { return !_mob.navigation().isDone(); }

void FleeSunGoal::start() { _mob.navigation().moveTo(_wantedX, _wantedY, _wantedZ, _speedModifier); }

std::optional<Vec3> FleeSunGoal::getHidePos() {
	JavaRandom& random = _mob.random();
	BlockPos	pos	   = _mob.blockPosition();
	for (int i = 0; i < 10; i++) {
		BlockPos at = pos.offset(random.nextInt(20) - 10, random.nextInt(6) - 3, random.nextInt(20) - 10);
		if (!_mob.level().canSeeSky(at) && _mob.getWalkTargetValue(at) < 0.0F) return Vec3{at.x + 0.5, static_cast<double>(at.y), at.z + 0.5};
	}
	return std::nullopt;
}

// ===================== Targets =====================

double TargetGoal::getFollowDistance() { return _mob.getAttributeValue(_mob.attributeIds().followRange); }

bool TargetGoal::canContinueToUse() {
	Actor* target = _mob.getTarget();
	if (!target) target = _mob.level().actorByRef(_targetMob);
	if (!target || !_mob.canAttack(*target)) return false;
	double follow = getFollowDistance();
	if (_mob.distanceToSqr(*target) > follow * follow) return false;
	if (_mustSee) {
		if (_mob.sensing().hasLineOfSight(*target)) {
			_unseenTicks = 0;
		} else if (++_unseenTicks > reducedTickDelay(_unseenMemoryTicks)) {
			return false;
		}
	}
	_mob.setTarget(target);
	return true;
}

void TargetGoal::start() {
	_reachCache		= 0;
	_reachCacheTime = 0;
	_unseenTicks	= 0;
}

void TargetGoal::stop() {
	_mob.setTarget(nullptr);
	_targetMob.clear();
}

bool TargetGoal::canAttack(Actor* target, const TargetingConditions& conditions) {
	if (!target || !conditions.test(_mob.level(), &_mob, *target)) return false;
	if (!_mob.isWithinHome(BlockPos{Mth::floor(target->position().x), Mth::floor(target->position().y), Mth::floor(target->position().z)})) return false;
	if (_mustReach) {
		if (--_reachCacheTime <= 0) _reachCache = 0;
		if (_reachCache == 0) _reachCache = canReach(*target) ? 1 : 2;
		if (_reachCache == 2) return false;
	}
	return true;
}

bool TargetGoal::canReach(Actor& target) {
	_reachCacheTime			   = reducedTickDelay(10 + _mob.random().nextInt(5));
	std::unique_ptr<Path> path = _mob.navigation().createPath(target, 0);
	if (!path) return false;
	const Path::PathNode* end = path->getEndNode();
	if (!end) return false;
	int dx = end->x - Mth::floor(target.position().x), dz = end->z - Mth::floor(target.position().z);
	return dx * dx + dz * dz <= 2.25;
}

NearestAttackableTargetGoal::NearestAttackableTargetGoal(Mob& mob, std::string targetClass, int randomInterval, bool mustSee, bool mustReach,
														 std::function<bool(Actor&, Level&)> selector)
	: TargetGoal(mob, mustSee, mustReach), _targetClass(std::move(targetClass)), _randomInterval(reducedTickDelay(randomInterval)) {
	setFlags({Flag::Target});
	_targetConditions = TargetingConditions::forCombat().withRange(getFollowDistance()).withSelector(std::move(selector));
}

bool NearestAttackableTargetGoal::canUse() {
	if (_randomInterval > 0 && _mob.random().nextInt(_randomInterval) != 0) return false;
	findTarget();
	return _target.isSet();
}

AABB NearestAttackableTargetGoal::getTargetSearchArea(double distance) { return _mob.boundingBox().inflate(distance, distance, distance); }

void NearestAttackableTargetGoal::findTarget() {
	TargetingConditions conditions = _targetConditions;
	conditions.range			   = getFollowDistance();
	Actor* found;
	if (_targetClass == "Player") {
		found = nearestPlayer(_mob.level(), conditions, &_mob, _mob.position().x, _mob.eyeY(), _mob.position().z);
	} else {
		found = nearestActor(_mob.level(), actorsOfClass(_mob.level(), _targetClass, getTargetSearchArea(getFollowDistance())), conditions, &_mob,
							 _mob.position().x, _mob.eyeY(), _mob.position().z);
	}
	setTarget(found);
}

void NearestAttackableTargetGoal::start() {
	_mob.setTarget(_mob.level().actorByRef(_target));
	TargetGoal::start();
}

HurtByTargetGoal::HurtByTargetGoal(Mob& mob, std::vector<std::string> toIgnoreDamage) : TargetGoal(mob, true), _toIgnoreDamage(std::move(toIgnoreDamage)) {
	setFlags({Flag::Target});
}

bool HurtByTargetGoal::canUse() {
	int	   timestamp = _mob.getLastHurtByMobTimestamp();
	Actor* attacker	 = _mob.getLastHurtByMob();
	if (timestamp == _timestamp || !attacker) return false;
	// universalAnger is off by default
	for (const std::string& ignored : _toIgnoreDamage) {
		if (isOfClass(*attacker, ignored)) return false;
	}
	static const TargetingConditions HURT_BY = TargetingConditions::forCombat().ignoreLineOfSight().ignoreInvisibilityTesting();
	return canAttack(attacker, HURT_BY);
}

HurtByTargetGoal& HurtByTargetGoal::setAlertOthers(std::vector<std::string> toIgnoreAlert) {
	_alertSameType = true;
	_toIgnoreAlert = std::move(toIgnoreAlert);
	return *this;
}

void HurtByTargetGoal::start() {
	_mob.setTarget(_mob.getLastHurtByMob());
	if (Actor* target = _mob.getTarget()) _targetMob = EntityRef(*target);
	_timestamp		   = _mob.getLastHurtByMobTimestamp();
	_unseenMemoryTicks = 300;
	if (_alertSameType) alertOthers();
	TargetGoal::start();
}

void HurtByTargetGoal::alertOthers() {
	double follow = getFollowDistance();
	Vec3   at	  = _mob.position();
	AABB   box{static_cast<double>(Mth::floor(at.x)), static_cast<double>(Mth::floor(at.y)), static_cast<double>(Mth::floor(at.z)),
			   Mth::floor(at.x) + 1.0, Mth::floor(at.y) + 1.0, Mth::floor(at.z) + 1.0};
	box			 = AABB{at.x, at.y, at.z, at.x + 1.0, at.y + 1.0, at.z + 1.0}.inflate(follow, 10.0, follow); // unitCubeFromLowerCorner
	Actor* attacker = _mob.getLastHurtByMob();
	if (!attacker) return;
	for (Actor* actor : _mob.level().actorsIn(box)) {
		Mob* other = dynamic_cast<Mob*>(actor);
		// The same Java class: the same entity type here
		if (!other || other == &_mob || other->typeId() != _mob.typeId() || other->getTarget() || other->isSpectator()) continue;
		bool ignored = false;
		for (const std::string& name : _toIgnoreAlert) ignored |= isOfClass(*other, name);
		if (ignored) continue;
		alertOther(*other, *attacker);
	}
}

void HurtByTargetGoal::alertOther(Mob& other, Actor& target) { other.setTarget(&target); }

// ===================== TemptGoal =====================

TemptGoal::TemptGoal(Mob& mob, double speed, std::function<bool(const ItemStack&)> items, bool canScare, double stopDistance)
	: _mob(mob), _speedModifier(speed), _items(std::move(items)), _canScare(canScare), _stopDistance(stopDistance) {
	setFlags({Flag::Move, Flag::Look});
	// TEMPT_TARGETING, with shouldFollow: one of its foods in either hand
	_targetingConditions = TargetingConditions::forNonCombat().ignoreLineOfSight().withSelector([this](Actor& target, Level&) {
		Player* player = target.asPlayer();
		return player && (_items(player->getStackInHand(0)) || _items(player->getStackInHand(1)));
	});
}

bool TemptGoal::canUse() {
	if (_calmDown > 0) {
		_calmDown--;
		return false;
	}
	TargetingConditions conditions = _targetingConditions;
	conditions.withRange(_mob.getAttributeValue(_mob.attributeIds().temptRange));
	const Vec3& at	   = _mob.position();
	Actor*		player = nearestPlayer(_mob.level(), conditions, &_mob, at.x, _mob.eyeY(), at.z);
	if (player) {
		_player = EntityRef(*player);
	} else {
		_player.clear();
	}
	return player != nullptr;
}

bool TemptGoal::canContinueToUse() {
	Player* player = _player.isSet() && _mob.level().actorByRef(_player) ? _mob.level().actorByRef(_player)->asPlayer() : nullptr;
	if (_canScare && player) {
		if (_mob.distanceToSqr(*player) < 36.0) {
			double dx = player->getX() - _px, dy = player->getY() - _py, dz = player->getZ() - _pz;
			if (dx * dx + dy * dy + dz * dz > 0.010000000000000002) return false;
			if (std::abs(player->getPitch() - _pRotX) > 5.0 || std::abs(player->getYaw() - _pRotY) > 5.0) return false;
		} else {
			_px = player->getX();
			_py = player->getY();
			_pz = player->getZ();
		}
		_pRotX = player->getPitch();
		_pRotY = player->getYaw();
	}
	return canUse();
}

void TemptGoal::start() {
	if (Actor* player = _mob.level().actorByRef(_player)) {
		_px = player->position().x;
		_py = player->position().y;
		_pz = player->position().z;
	}
	_isRunning = true;
}

void TemptGoal::stop() {
	_player.clear();
	_mob.navigation().stop();
	_calmDown  = reducedTickDelay(100);
	_isRunning = false;
}

void TemptGoal::tick() {
	Actor* player = _mob.level().actorByRef(_player);
	if (!player) return;
	_mob.lookControl().setLookAt(*player, static_cast<float>(_mob.maxHeadYRot() + 20), static_cast<float>(_mob.maxHeadXRot()));
	if (_mob.distanceToSqr(*player) < _stopDistance * _stopDistance) {
		_mob.navigation().stop();
	} else {
		_mob.navigation().moveTo(*player, _speedModifier);
	}
}

// ===================== RangedAttackGoal =====================

bool RangedAttackGoal::canUse() {
	Actor* target = _mob.getTarget();
	return target && target->isAlive() && _mob.distanceToSqr(*target) <= static_cast<double>(_range) * _range;
}

void RangedAttackGoal::tick() {
	Actor* target = _mob.getTarget();
	if (!target) return;
	double distance = _mob.distanceToSqr(*target);
	bool   visible  = _mob.sensing().hasLineOfSight(*target);
	_mob.lookControl().setLookAt(*target, 30.0F, 30.0F);
	// Keep the distance: closer than half the range, walk away; else toward the target
	if (distance > static_cast<double>(_range) * _range * 0.5) {
		_mob.navigation().moveTo(*target, _speed);
	} else {
		_mob.navigation().stop();
	}
	if (visible && --_attackTime <= 0) {
		shoot(*target);
		_attackTime = _interval + _mob.random().nextInt(10);
	}
}

void RangedAttackGoal::shoot(Actor& target) {
	Level& level = _mob.level();
	// AbstractSkeleton.shootProjectile: aimed at the target's body, raised by 20 % of the horizontal distance to
	// clear the arc, at 1.6 blocks/tick with a spread by difficulty
	double dx = target.position().x - _mob.position().x;
	double dy = target.position().y + 0.5 - (_mob.position().y + _mob.eyeHeight());
	double dz = target.position().z - _mob.position().z;
	double d	= std::sqrt(dx * dx + dz * dz);
	Vec3	dir{dx, dy + d * 0.2, dz};
	double length = dir.length();
	if (length <= 1.0E-4) return;
	dir = dir.scale(1.6 / length);
	JavaRandom& random = _mob.random();
	double		 spread = 14 - level.difficulty() * 4;
	dir = dir + Vec3{random.nextFloat() * spread / 1000.0 - spread / 2000.0, random.nextFloat() * spread / 1000.0 - spread / 2000.0,
					random.nextFloat() * spread / 1000.0 - spread / 2000.0};
	level.entities().add(std::make_unique<Arrow>(level, Vec3{_mob.position().x, _mob.position().y + _mob.eyeHeight(), _mob.position().z}, dir, &_mob));
	level.playSoundAt(nullptr, _mob.position().x, _mob.position().y, _mob.position().z, "minecraft:entity.skeleton.shoot", Level::SoundSource::Hostile, 1.0F,
					  1.0F / (_mob.random().nextFloat() * 0.4F + 0.8F));
}

// ===================== FollowOwnerGoal =====================

bool FollowOwnerGoal::canUse() {
	Actor* owner = _mob.getOwnerEntity();
	return owner && owner->isAlive() && _mob.distanceToSqr(*owner) > static_cast<double>(_startDistance) * _startDistance;
}

bool FollowOwnerGoal::canContinueToUse() {
	Actor* owner = _mob.getOwnerEntity();
	return owner && owner->isAlive() && !_mob.navigation().isDone() && _mob.distanceToSqr(*owner) > static_cast<double>(_stopDistance) * _stopDistance;
}

void FollowOwnerGoal::start() { _recalculateTicks = 0; _startedFollowing = false; }

void FollowOwnerGoal::stop() {
	_mob.navigation().stop();
	_startedFollowing = false;
}

void FollowOwnerGoal::tick() {
	Actor* owner = _mob.getOwnerEntity();
	if (!owner) return;
	_mob.lookControl().setLookAt(*owner, 10.0F, static_cast<float>(_mob.maxHeadXRot()));
	// FollowOwnerGoal.tryTeleportTo: the owner is too far (144 blocks): teleport next to it (25 % chance a tick)
	if (_mob.distanceToSqr(*owner) >= 144.0 * 144.0) {
		if (_mob.random().nextFloat() < 0.25F) {
			Vec3 at = owner->position();
			_mob.snapTo({at.x, at.y, at.z}, _mob.yRot(), _mob.xRot());
			_mob.navigation().stop();
		}
		return;
	}
	if (--_recalculateTicks <= 0) {
		_recalculateTicks = 10;
		if (!_mob.navigation().moveTo(*owner, _speed)) _mob.navigation().moveTo(owner->position().x, owner->position().y, owner->position().z, _speed);
	}
}
