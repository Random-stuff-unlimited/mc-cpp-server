#ifndef SMALL_FIREBALL_HPP
#define SMALL_FIREBALL_HPP

#include "world/entity/Entity.hpp"

#include <memory>

class Actor;

// A blaze's fireball (vanilla's SmallFireballEntity): flies straight (no gravity), hurts for 5 and sets its target
// on fire for 5 seconds
class SmallFireball : public Entity {
  public:
	SmallFireball(Level& level, const Vec3& position, const Vec3& delta, Actor* owner);

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