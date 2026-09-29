#ifndef SNOW_GOLEM_HPP
#define SNOW_GOLEM_HPP

#include "world/entity/Mob.hpp"

// SnowGolem: throws snowballs (no damage, a knockback) at hostile mobs, and melts in the rain or in warm biomes
class SnowGolem : public Mob {
  public:
	using Mob::Mob;
	void registerGoals() override;
	void tick() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
};

#endif