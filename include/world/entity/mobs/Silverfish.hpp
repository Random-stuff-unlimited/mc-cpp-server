#ifndef SILVERFISH_HPP
#define SILVERFISH_HPP

#include "world/entity/mobs/Monster.hpp"

#include <memory>

// Silverfish: hides into the stone next to it when idle (an infested block), and when hurt by an entity calls out the
// ones hidden in the infested blocks around (they break out)
class Silverfish : public Monster {
  public:
	using Monster::Monster;
	void  registerGoals() override;
	bool  hurtServer(const Combat::DamageSource& source, float amount) override;
	void  tick() override;
	float getWalkTargetValue(const BlockPos& pos) override;
	// InfestedBlock.isCompatibleHostBlock and infestedStateByHost: the infested state of a host block state, -1 if none
	static int infestedStateOf(Level& level, int hostState);

  private:
	std::shared_ptr<Goal> _friendsGoal;
};

#endif
