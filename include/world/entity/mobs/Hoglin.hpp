#ifndef HOGLIN_HPP
#define HOGLIN_HPP

#include "world/entity/Mob.hpp"

// Hoglin: a hostile pig of the Nether that charges players and hunts hoglins... no, just players, with a powerful
// headbutt
class Hoglin : public Mob {
  public:
	using Mob::Mob;
	void registerGoals() override;
};

#endif