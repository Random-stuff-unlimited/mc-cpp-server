#ifndef SILVERFISH_HPP
#define SILVERFISH_HPP

#include "world/entity/mobs/Monster.hpp"

// Silverfish: a small, fast hostile that scuttles around and, when one is hurt, alerts every other silverfish
// nearby to join the fight
class Silverfish : public Monster {
  public:
	using Monster::Monster;
	void registerGoals() override;
};

#endif