#ifndef IRON_GOLEM_HPP
#define IRON_GOLEM_HPP

#include "world/entity/Mob.hpp"

// IronGolem: a sturdy golem that attacks hostile mobs (it can't swim and sinks), with a swipe that knocks its
// victim up. It never despawns.
class IronGolem : public Mob {
  public:
	using Mob::Mob;

	void registerGoals() override;
	bool removeWhenFarAway(double distanceSqr) const override { return false; }
	bool doHurtTarget(Actor& target) override;
	void travel(const Vec3& input) override; // It can't swim: it sinks
};

#endif