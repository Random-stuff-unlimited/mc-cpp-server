#ifndef THROWN_EXPERIENCE_BOTTLE_HPP
#define THROWN_EXPERIENCE_BOTTLE_HPP

#include "world/entity/Entity.hpp"

#include <memory>

class Player;

// A thrown experience bottle (vanilla's ThrownExperienceBottle): flies with gravity, breaks on a block, dropping XP
// orbs worth 3-11, and disappears after 600 ticks
class ThrownExperienceBottle : public Entity {
  public:
	// From the player's eye, thrown along its look (Projectile.shoot: 0.7 speed, 1.0 divergence)
	ThrownExperienceBottle(Level& level, Player& player);

	void tick() override;

  private:
	int _age = 0;
	void breakBottle();
};

#endif