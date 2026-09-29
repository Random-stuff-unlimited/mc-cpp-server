#ifndef BLAZE_HPP
#define BLAZE_HPP

#include "world/entity/mobs/Monster.hpp"

// Blaze: a hostile of the Nether that hovers (no gravity), immune to fire and lava, and shoots volleys of three
// fireballs at its target
class Blaze : public Monster {
  public:
	Blaze(Level& level, int typeId);

	bool fireImmune() const override { return true; }
	void registerGoals() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	static constexpr int DATA_ID_FLAGS = 16;
};

#endif