#ifndef SKELETON_HPP
#define SKELETON_HPP

#include "world/entity/mobs/Monster.hpp"

// Skeleton (and Stray): shoots arrows at its targets from a distance (RangedAttackGoal), keeps its distance, flees
// the sun (burning in daylight unless it wears a helmet, which wears out instead), and always spawns with a bow
class Skeleton : public Monster {
  public:
	using Monster::Monster;

	void registerGoals() override;
	void populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;

  protected:
	void aiStep() override;
};

// Stray: a skeleton of the frozen oceans (its slowness arrows aren't there yet, same behavior)
class Stray : public Skeleton {
  public:
	using Skeleton::Skeleton;
};

#endif