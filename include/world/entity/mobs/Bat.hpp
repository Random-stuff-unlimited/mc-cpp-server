#ifndef BAT_HPP
#define BAT_HPP

#include "world/entity/Mob.hpp"

// Bat: hangs under a solid block until a player comes within 4 blocks (or the block goes), then flutters toward
// random spots nearby, and sometimes hangs again
class Bat : public Mob {
  public:
	static constexpr int DATA_FLAGS = 16; // 1: resting

	Bat(Level& level, int typeId);

	bool isResting() const { return _flags & 1; }
	void setResting(bool resting);
	bool isPushable() const override { return false; }
	void tick() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void  customServerAiStep() override;
	void  checkFallDamage(double, bool, const BlockPos&) override {}
	float soundVolume() const override { return 0.1F; }
	// Mostly silent while it hangs
	std::string ambientSoundName() override { return isResting() && _random.nextInt(4) != 0 ? "" : Mob::ambientSoundName(); }
	float voicePitch() override { return Mob::voicePitch() * 0.95F; }
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t	 _flags = 0;
	bool	 _hasTarget = false;
	BlockPos _targetPosition;
};

#endif
