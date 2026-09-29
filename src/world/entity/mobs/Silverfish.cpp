#include "world/entity/mobs/Silverfish.hpp"

#include "world/entity/ai/Goals.hpp"

void Silverfish::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(3, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(5, std::make_unique<RandomLookAroundGoal>(*this));

	// SilverfishWakeUpFriendsGoal: a hurt silverfish wakes every other one around
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers({"Silverfish"});
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}