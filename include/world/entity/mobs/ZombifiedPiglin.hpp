#ifndef ZOMBIFIED_PIGLIN_HPP
#define ZOMBIFIED_PIGLIN_HPP

#include "world/entity/mobs/Zombie.hpp"

// ZombifiedPiglin: a zombie of the Nether that is neutral until a player hurts it (or its kind around), then the
// group attacks for a while. It doesn't burn in the sun nor convert in water.
class ZombifiedPiglin : public Zombie {
  public:
	using Zombie::Zombie;

	bool isSunSensitive() const override { return false; }
	bool convertsInWater() const override { return false; }
	void addBehaviourGoals() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	void aiStep() override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  private:
	int _angerTime = 0; // NeutralMob.remainingPersistentAngerTime
};

#endif