#ifndef BAT_HPP
#define BAT_HPP

#include "world/entity/Mob.hpp"

// Bat: an ambient mob that hovers through the air, drifting toward random points nearby (BatMoveControl + a hover
// goal). Its gravity is off: it never falls.
class Bat : public Mob {
  public:
	Bat(Level& level, int typeId);

	bool isPushable() const override { return false; }
	bool removeWhenFarAway(double distanceSqr) const override { return true; }
	void registerGoals() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	static constexpr int DATA_ID_FLAGS = 16;
};

#endif