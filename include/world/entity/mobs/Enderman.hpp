#ifndef ENDERMAN_HPP
#define ENDERMAN_HPP

#include "world/entity/mobs/Monster.hpp"

class Player;

// Enderman: neutral. It teleports away when hurt, in water (rain burns it) or at random, and a player looking at it
// (the ray from its eyes, within 64 blocks) provokes it into screaming and attacking
class Enderman : public Monster {
  public:
	using Monster::Monster;

	bool isScreaming() const { return _screaming; }
	void setScreaming(bool screaming);
	// Enderman.setTarget: screaming while it has a target
	void setTarget(Actor* target) override;
	// Enderman.teleportRandomly: to a random spot, down onto a solid block, with the puff sound
	bool teleportRandomly();

	void registerGoals() override;
	void tick() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  private:
	static constexpr int DATA_SCREAMING = 16;

	bool _screaming = false;
	int	 _teleportDelay = 0;
};

#endif