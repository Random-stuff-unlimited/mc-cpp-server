#include "world/entity/mobs/PolarBear.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/entity/ai/Goals.hpp"

#include <algorithm>

void PolarBear::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.25, true));
	_goalSelector.addGoal(3, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(5, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	// Neutral: players are only targeted once provoked
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																		   [this](Actor&, Level&) { return _angry; }));
}

bool PolarBear::hurtServer(const Combat::DamageSource& source, float amount) {
	bool result = Mob::hurtServer(source, amount);
	if (result && !isRemoved() && source.attackerPlayer() && !_angry) {
		_angry = true;
		setTarget(source.attackerPlayer());
	}
	return result;
}

bool PolarBear::doHurtTarget(Actor& target) {
	// PolarBear.doHurtTarget: a swipe that also knocks the victim up a little
	bool result = Mob::doHurtTarget(target);
	if (result) target.pushMotion({0.0, 0.6, 0.0}); // pushUp
	return result;
}

void PolarBear::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeBool(_angry);
}

void PolarBear::load(Buffer& buf) {
	Mob::load(buf);
	_angry = buf.readBool();
}