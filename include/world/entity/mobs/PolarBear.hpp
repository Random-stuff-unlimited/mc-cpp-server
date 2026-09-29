#ifndef POLAR_BEAR_HPP
#define POLAR_BEAR_HPP

#include "world/entity/Mob.hpp"

// PolarBear: neutral, it attacks a player who hurts it (or its cubs: a hurt cub's attacker), with a strong swipe
class PolarBear : public Mob {
  public:
	using Mob::Mob;

	void registerGoals() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	bool doHurtTarget(Actor& target) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  private:
	bool _angry = false;
};

#endif