#ifndef CREEPER_HPP
#define CREEPER_HPP

#include "world/entity/mobs/Monster.hpp"

// Creeper: swells when its target is within 3 blocks (seen), explodes after 30 ticks of swelling (radius 3, twice as
// big when charged by lightning), shrinks back when the target gets away. Flint and steel lights it
class Creeper : public Monster {
  public:
	static constexpr int DATA_SWELL_DIR = 16, DATA_IS_POWERED = 17, DATA_IS_IGNITED = 18;

	Creeper(Level& level, int typeId) : Monster(level, typeId) {}

	int	 getSwellDir() const { return _swellDir; }
	void setSwellDir(int dir);
	bool isPowered() const { return _powered; }
	void setPowered(bool powered);
	bool isIgnited() const { return _ignited; }
	void ignite();
	int	 swell() const { return _swell; }

	void registerGoals() override;
	void tick() override;
	int	 getMaxFallDistance() override;
	void setTarget(Actor* target) override;
	// Creeper.doHurtTarget: it doesn't hit, it explodes
	bool doHurtTarget(Actor&) override { return true; }
	bool mobInteract(Player& player, int hand) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	bool causeFallDamage(double distance, float multiplier) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int	 _swell = 0, _maxSwell = 30, _explosionRadius = 3;
	int	 _swellDir = -1;
	bool _powered = false, _ignited = false;

	void explodeCreeper();
};

// SwellGoal: the creeper stops and swells while its target is close and seen
class SwellGoal : public Goal {
  public:
	explicit SwellGoal(Creeper& creeper);
	bool canUse() override;
	void start() override;
	void stop() override { _target.clear(); }
	bool requiresUpdateEveryTick() override { return true; }
	void tick() override;

  private:
	Creeper&  _creeper;
	EntityRef _target;
};

#endif
