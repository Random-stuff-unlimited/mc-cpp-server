#include "world/entity/mobs/TamableAnimal.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Pathfinder.hpp"

namespace {
	constexpr int SAVE_VERSION						  = 1;
	constexpr int SERIALIZER_OPTIONAL_LIVING_REFERENCE = 13;

	// The owner's last attacker / last victim, and when (its tick count)
	struct Hurt {
		Actor* actor = nullptr;
		int	   timestamp = 0;
	};
	Hurt ownerHurt(Level& level, Actor& owner, bool by) {
		if (Player* player = owner.asPlayer()) {
			const EntityRef& ref = by ? player->lastHurtByMob() : player->lastHurtMob();
			return {ref.isSet() ? level.actorByRef(ref) : nullptr, by ? player->lastHurtByMobTimestamp() : player->lastHurtMobTimestamp()};
		}
		if (LivingEntity* living = owner.asLiving()) {
			return {by ? living->getLastHurtByMob() : living->getLastHurtMob(), by ? living->getLastHurtByMobTimestamp() : living->getLastHurtMobTimestamp()};
		}
		return {};
	}
} // namespace

void TamableAnimal::setTame(bool tame, bool applySideEffects) {
	uint8_t flags = static_cast<uint8_t>(tame ? _tameFlags | 4 : _tameFlags & ~4);
	if (flags != _tameFlags) {
		_tameFlags = flags;
		markData(DATA_FLAGS);
	}
	if (applySideEffects) applyTamingSideEffects();
}

void TamableAnimal::setInSittingPose(bool sitting) {
	uint8_t flags = static_cast<uint8_t>(sitting ? _tameFlags | 1 : _tameFlags & ~1);
	if (flags == _tameFlags) return;
	_tameFlags = flags;
	markData(DATA_FLAGS);
}

Actor* TamableAnimal::getOwner() { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

void TamableAnimal::setOwnerReference(const EntityRef& owner) {
	_owner = owner;
	markData(DATA_OWNER);
}

void TamableAnimal::tame(Player& player) {
	setTame(true, true);
	setOwnerReference(EntityRef(player));
}

bool TamableAnimal::canAttack(Actor& target) { return isOwnedBy(target) ? false : Animal::canAttack(target); }

bool TamableAnimal::wantsToAttack(Actor&, Actor&) { return true; }

bool TamableAnimal::shouldTryTeleportToOwner() {
	Actor* owner = getOwner();
	return owner && distanceToSqr(*owner) >= 144.0;
}

bool TamableAnimal::unableToMoveToOwner() {
	Actor* owner = getOwner();
	return isOrderedToSit() || (owner && owner->isSpectator()); // Riding and leashes aren't ported
}

void TamableAnimal::tryToTeleportToOwner() {
	Actor* owner = getOwner();
	if (!owner) return;
	BlockPos around{Mth::floor(owner->position().x), Mth::floor(owner->position().y), Mth::floor(owner->position().z)};
	for (int i = 0; i < 10; i++) {
		int dx = _random.nextInt(7) - 3; // nextIntBetweenInclusive(-3, 3)
		int dz = _random.nextInt(7) - 3;
		if (std::abs(dx) < 2 && std::abs(dz) < 2) continue;
		int		 dy = _random.nextInt(3) - 1;
		BlockPos pos{around.x + dx, around.y + dy, around.z + dz};
		if (!canTeleportTo(pos)) continue;
		snapTo({pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}, _yRot, _xRot);
		navigation().stop();
		hasImpulse = true; // Its viewers see it there
		return;
	}
}

bool TamableAnimal::canTeleportTo(const BlockPos& pos) {
	if (WalkNodeEvaluator::getPathTypeStatic(_level, *this, pos) != PathType::Walkable) return false;
	int below = _level.blocks().blockOf(_level.getBlockState(pos.below()));
	if (!canFlyToOwner() && _level.gameData().isInTag("minecraft:block", "minecraft:leaves", below)) return false;
	BlockPos from = blockPosition();
	return !_level.hasBlockCollision(boundingBox().move(pos.x - from.x, pos.y - from.y, pos.z - from.z));
}

void TamableAnimal::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _tameFlags == 0)) writeByteData(buf, DATA_FLAGS, _tameFlags);
	if (wantsData(mask, DATA_OWNER, onlyNonDefault, !_owner.isSet())) {
		buf.writeUByte(DATA_OWNER);
		buf.writeVarInt(SERIALIZER_OPTIONAL_LIVING_REFERENCE);
		buf.writeBool(_owner.isSet());
		if (_owner.isSet()) buf.writeUUID(_owner.uuid);
	}
}

void TamableAnimal::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_owner.isSet());
	if (_owner.isSet()) buf.writeUUID(_owner.uuid);
	buf.writeBool(_orderedToSit);
}

void TamableAnimal::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown tamable animal data version");
	_owner.clear();
	if (buf.readBool()) {
		_owner.uuid = buf.readUUID();
		_owner.id	= -2; // Found again by its UUID
		setTame(true, false);
	} else {
		setTame(false, true);
	}
	_orderedToSit = buf.readBool();
	setInSittingPose(_orderedToSit);
	_dirtyData = 0;
}

// ===================== Goals =====================

void TamableAnimalPanicGoal::tick() {
	if (!_tamable.unableToMoveToOwner() && _tamable.shouldTryTeleportToOwner()) _tamable.tryToTeleportToOwner();
}

bool SitWhenOrderedToGoal::canUse() {
	bool ordered = _mob.isOrderedToSit();
	if (!ordered && !_mob.isTame()) return false;
	if (_mob.isInWaterNow() || !_mob.onGround()) return false;
	Actor* owner = _mob.getOwner();
	if (!owner) return true;
	return _mob.distanceToSqr(*owner) < 144.0 && ownerHurt(_mob.level(), *owner, true).actor ? false : ordered;
}

void SitWhenOrderedToGoal::start() {
	_mob.navigation().stop();
	_mob.setInSittingPose(true);
}

bool FollowOwnerGoal::canUse() {
	Actor* owner = _tamable.getOwner();
	if (!owner || _tamable.unableToMoveToOwner()) return false;
	if (_tamable.distanceToSqr(*owner) < static_cast<double>(_startDistance) * _startDistance) return false;
	_owner = EntityRef(*owner);
	return true;
}

bool FollowOwnerGoal::canContinueToUse() {
	if (_tamable.navigation().isDone() || _tamable.unableToMoveToOwner()) return false;
	Actor* owner = _tamable.level().actorByRef(_owner);
	return owner && !(_tamable.distanceToSqr(*owner) <= static_cast<double>(_stopDistance) * _stopDistance);
}

void FollowOwnerGoal::start() {
	_timeToRecalcPath = 0;
	_oldWaterCost	  = _tamable.getPathfindingMalus(PathType::Water);
	_tamable.setPathfindingMalus(PathType::Water, 0.0F);
}

void FollowOwnerGoal::stop() {
	_owner.clear();
	_tamable.navigation().stop();
	_tamable.setPathfindingMalus(PathType::Water, _oldWaterCost);
}

void FollowOwnerGoal::tick() {
	Actor* owner = _owner.isSet() ? _tamable.level().actorByRef(_owner) : nullptr;
	if (!owner) return;
	bool teleport = _tamable.shouldTryTeleportToOwner();
	if (!teleport) _tamable.lookControl().setLookAt(*owner, 10.0F, static_cast<float>(_tamable.maxHeadXRot()));
	if (--_timeToRecalcPath <= 0) {
		_timeToRecalcPath = adjustedTickDelay(10);
		if (teleport) {
			_tamable.tryToTeleportToOwner();
		} else {
			_tamable.navigation().moveTo(*owner, _speedModifier);
		}
	}
}

bool OwnerHurtByTargetGoal::canUse() {
	if (!_tamable.isTame() || _tamable.isOrderedToSit()) return false;
	Actor* owner = _tamable.getOwner();
	if (!owner) return false;
	Hurt hurt = ownerHurt(_mob.level(), *owner, _ownerAttacked);
	if (hurt.timestamp == _timestamp || !canAttack(hurt.actor, TargetingConditions::forCombat()) || !_tamable.wantsToAttack(*hurt.actor, *owner)) return false;
	_wanted = EntityRef(*hurt.actor);
	return true;
}

void OwnerHurtByTargetGoal::start() {
	_mob.setTarget(_mob.level().actorByRef(_wanted));
	if (Actor* owner = _tamable.getOwner()) _timestamp = ownerHurt(_mob.level(), *owner, _ownerAttacked).timestamp;
	TargetGoal::start();
}

bool NonTameRandomTargetGoal::canContinueToUse() {
	Actor* target = _target.isSet() ? _mob.level().actorByRef(_target) : nullptr;
	return target && _targetConditions.test(_mob.level(), &_mob, *target);
}
