#ifndef MONSTER_HPP
#define MONSTER_HPP

#include "world/entity/Mob.hpp"

// Monster: hostile mobs. They like the dark (their walk target value), and daylight makes them idle faster (they
// despawn sooner). Every entity type of the Monster class that has no class of its own is made one of these
class Monster : public Mob {
  public:
	using Mob::Mob;
	// Monster.getWalkTargetValue: minus how bright it is
	float getWalkTargetValue(const BlockPos& pos) override;

  protected:
	void aiStep() override;
	// Monster.updateNoActionTime: bright places count double
	virtual void updateNoActionTime();
};

#endif
