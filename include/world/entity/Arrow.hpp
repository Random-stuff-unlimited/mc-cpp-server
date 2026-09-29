#ifndef ARROW_HPP
#define ARROW_HPP

#include "world/entity/Entity.hpp"

#include <memory>

class Actor;

// A flying arrow (vanilla's AbstractArrow): fired with a velocity, it falls with gravity, breaks on a block and hurts
// the first player or mob it touches (its shooter's doing). Not saved with the chunks; gone when it hits.
class Arrow : public Entity {
  public:
	static constexpr int LIFETIME = 1200;

	Arrow(Level& level, const Vec3& position, const Vec3& delta, Actor* owner);

	void tick() override;
	Actor* owner() const override;

  private:
	static constexpr float BASE_DAMAGE = 2.0F;

	EntityRef _owner;
	int		  _age = 0;

	void hitEntity(Actor& target);
	void hitBlock();
};

#endif