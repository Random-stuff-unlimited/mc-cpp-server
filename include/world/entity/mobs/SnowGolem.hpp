#ifndef SNOW_GOLEM_HPP
#define SNOW_GOLEM_HPP

#include "world/entity/Mob.hpp"

// SnowGolem: throws snowballs at the monsters around (RangedAttackGoal), leaves a trail of snow, is hurt by water and
// rain, and melts in the warm biomes. Shears take its pumpkin off
class SnowGolem : public Mob {
  public:
	static constexpr int DATA_PUMPKIN = 16; // 16: has its pumpkin

	using Mob::Mob;
	bool hasPumpkin() const { return _pumpkin; }
	void setPumpkin(bool pumpkin);
	bool readyForShearing() const { return isAlive() && _pumpkin; }
	void shear();

	void registerGoals() override;
	bool isSensitiveToWater() const override { return true; }
	void performRangedAttack(Actor& target, float power) override;
	bool mobInteract(Player& player, int hand) override;
	// AbstractGolem: never despawns
	bool removeWhenFarAway(double) const override { return false; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void aiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _pumpkin = true;
};

#endif
