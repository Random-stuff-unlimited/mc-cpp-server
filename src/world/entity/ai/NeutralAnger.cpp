#include "world/entity/ai/NeutralAnger.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"

void NeutralAnger::setTarget(const UUID* target) {
	_hasTarget = target != nullptr;
	if (target) _target = *target;
}

void NeutralAnger::startTimer() { _remaining = _mob.random().nextInt(_maxTicks - _minTicks + 1) + _minTicks; } // UniformInt.sample

bool NeutralAnger::isAngryAt(Actor& target) const { return _mob.canAttack(target) && _hasTarget && target.uuid() == _target; }

void NeutralAnger::update(bool keepWhileTargetingPlayer) {
	Actor* target = _mob.getTarget();
	bool   dead	  = !target || !target->isAlive();
	if (dead && _hasTarget) {
		// Angry at a mob that is gone or dead: over
		EntityRef ref;
		ref.id	   = -2;
		ref.uuid   = _target;
		Actor* who = _mob.level().actorByRef(ref);
		if (who && who->asLiving() && !who->asPlayer()) {
			stopBeingAngry();
			return;
		}
	}
	if (target && !(_hasTarget && _target == target->uuid())) {
		UUID uuid = target->uuid();
		setTarget(&uuid);
		startTimer();
	}
	if (_remaining > 0 && (!target || !target->asPlayer() || !keepWhileTargetingPlayer)) {
		if (--_remaining == 0) stopBeingAngry();
	}
}

void NeutralAnger::stopBeingAngry() {
	_mob.setLastHurtByMob(nullptr);
	setTarget(nullptr);
	_mob.setTarget(nullptr);
	_remaining = 0;
}

void NeutralAnger::playerDied(const Actor& player) {
	if (_hasTarget && player.uuid() == _target) stopBeingAngry();
}

void NeutralAnger::save(Buffer& buf) const {
	buf.writeVarInt(_remaining);
	buf.writeBool(_hasTarget);
	if (_hasTarget) buf.writeUUID(_target);
}

void NeutralAnger::load(Buffer& buf) {
	_remaining = buf.readVarInt();
	_hasTarget = buf.readBool();
	if (_hasTarget) _target = buf.readUUID();
}
