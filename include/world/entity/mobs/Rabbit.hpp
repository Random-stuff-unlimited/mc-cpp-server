#ifndef RABBIT_HPP
#define RABBIT_HPP

#include "world/entity/mobs/Animal.hpp"

// Rabbit: a small prey animal that hops (a jump as soon as it lands), panics when hurt, flees players and wolves,
// and breeds with carrots
class Rabbit : public Animal {
  public:
	Rabbit(Level& level, int typeId);
	void registerGoals() override;
};

#endif