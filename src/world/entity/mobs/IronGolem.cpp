#include "world/entity/mobs/IronGolem.hpp"

#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

void IronGolem::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(3, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(5, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	// GolemLastTargetGoal: it attacks hostile mobs (their attacks on villagers are a village's; there are none)
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Monster", true));
}

bool IronGolem::doHurtTarget(Actor& target) {
	// IronGolem.doHurtTarget: a heavy swipe that knocks the victim up
	bool result = Mob::doHurtTarget(target);
	if (result) target.pushMotion({0.0, 0.4, 0.0}); // pushUp(0.4)
	return result;
}

void IronGolem::travel(const Vec3& input) {
	// IronGolem.travel: it can't swim, it drags through the water instead of floating
	if (isInWater()) {
		setDeltaMovement(_delta.scale(0.8));
		moveRelative(0.02F, input);
		move(_delta);
	} else {
		Mob::travel(input);
	}
}