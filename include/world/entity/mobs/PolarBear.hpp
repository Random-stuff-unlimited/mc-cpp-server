#ifndef POLAR_BEAR_HPP
#define POLAR_BEAR_HPP

#include "world/entity/ai/NeutralAnger.hpp"
#include "world/entity/mobs/Animal.hpp"

// PolarBear: neutral. It attacks who hurt it (20 to 39 seconds of anger), foxes, and the players that come near its
// cubs; a hurt cub calls the adults around. It stands up (a warning) before it strikes. Swims fast, can't be bred
class PolarBear : public Animal {
  public:
	static constexpr int DATA_STANDING = 17;

	PolarBear(Level& level, int typeId) : Animal(level, typeId), _anger(*this, 20, 39) {}

	bool isStanding() const { return _standing; }
	void setStanding(bool standing);
	void playWarningSound();
	NeutralAnger& anger() { return _anger; }

	void registerGoals() override;
	bool isFood(const ItemStack&) const override { return false; }
	void tick() override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	float waterSlowDown() const override { return 0.98F; }
	// AgeableMobGroupData(1.0): a pack's bears after the first are all cubs
	float babySpawnChance() const override { return 1.0F; }
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool		 _standing = false;
	int			 _warningSoundTicks = 0;
	NeutralAnger _anger;
};

#endif
