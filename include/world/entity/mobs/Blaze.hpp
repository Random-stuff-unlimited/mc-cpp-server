#ifndef BLAZE_HPP
#define BLAZE_HPP

#include "world/entity/mobs/Monster.hpp"

// Blaze: falls slowly, rises above its target, hits it up close and from afar charges up (on fire for 3 seconds) then
// shoots three small fireballs, every 5 seconds. Water and rain hurt it
class Blaze : public Monster {
  public:
	static constexpr int DATA_FLAGS = 16; // 1: charged (seen on fire)

	Blaze(Level& level, int typeId);

	void registerGoals() override;
	bool isSensitiveToWater() const override { return true; }
	bool isCharged() const { return _flags & 1; }
	void setCharged(bool charged);

  protected:
	void aiStep() override;
	void customServerAiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t _flags				  = 0;
	float	_allowedHeightOffset  = 0.5F;
	int		_nextHeightOffsetChangeTick = 0;
};

#endif
