#ifndef EXPERIENCE_ORB_HPP
#define EXPERIENCE_ORB_HPP

#include "world/entity/Entity.hpp"

#include <memory>

class Player;

// An experience orb floating in the world (vanilla's ExperienceOrb): falls, bounces, merges with its neighbors and
// is drawn to the nearest player within 6 blocks, who picks it up (its value added to its XP)
class ExperienceOrb : public Entity {
  public:
	static constexpr int LIFETIME  = 5900;
	static constexpr int MAX_VALUE = 100; // More than that splits into several orbs (ExperienceOrb.award)

	ExperienceOrb(Level& level, const Vec3& position, int value);
	// With vanilla's random initial push
	static std::unique_ptr<ExperienceOrb> create(Level& level, const Vec3& position, int value);

	int value() const { return _value; }
	int age() const { return _age; }

	void tick() override;
	// The value sent in the Spawn Entity packet's data field (Entity.getData)
	int	 entityData() const override { return _value; }

  private:
	int _value = 0;
	int _age	 = 0;
	bool isMergable() const;
	void mergeWithNeighbours();
	void playerTouch(Player& player);
};

#endif