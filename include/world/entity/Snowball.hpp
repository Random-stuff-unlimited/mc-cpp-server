#ifndef SNOWBALL_HPP
#define SNOWBALL_HPP

#include "world/entity/Entity.hpp"

#include <memory>

class Actor;

// A thrown snowball (vanilla's SnowballEntity): flies with gravity and, on a player or mob, knocks it back (and
// hurts a blaze for 3). It does no damage otherwise.
class Snowball : public Entity {
  public:
	Snowball(Level& level, const Vec3& position, const Vec3& delta, Actor* owner);

	void tick() override;
	Actor* owner() const override;

  private:
	static constexpr int LIFETIME = 1200;

	EntityRef _owner;
	int		  _age = 0;

	void hitEntity(Actor& target);
	void hitBlock();
};

#endif