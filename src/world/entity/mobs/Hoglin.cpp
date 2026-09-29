#include "world/entity/mobs/Hoglin.hpp"

#include "world/entity/ai/Goals.hpp"

void Hoglin::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(3, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.8));
	_goalSelector.addGoal(4, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(5, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}