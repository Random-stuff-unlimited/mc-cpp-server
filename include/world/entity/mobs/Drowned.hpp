#ifndef DROWNED_HPP
#define DROWNED_HPP

#include "world/entity/mobs/Zombie.hpp"

// Drowned: a zombie of the seas. It doesn't burn in the sun, can breathe underwater, and its victims are hit with
// the trident's splash... no, a simple melee like the zombie (trident throwing isn't there yet)
class Drowned : public Zombie {
  public:
	Drowned(Level& level, int typeId) : Zombie(level, typeId) { _canBreatheUnderwater = true; }

	bool isSunSensitive() const override { return false; }
	bool convertsInWater() const override { return false; }
};

#endif