#ifndef TURTLE_HPP
#define TURTLE_HPP

#include "world/entity/mobs/Animal.hpp"

#include <optional>

// Turtle: lives in the water (amphibious navigation, its own move control), travels far across the sea, and comes back
// to the beach it was born on to lay its eggs in the sand after breeding. Babies run for the water, and leave a scute
// when they grow up
class Turtle : public Animal {
  public:
	static constexpr int DATA_HAS_EGG = 17, DATA_LAYING_EGG = 18;

	Turtle(Level& level, int typeId);
	// Turtle.thunderHit: killed (it drops a bowl)
	void thunderHit(LightningBolt& bolt) override;

	bool hasEgg() const { return _hasEgg; }
	void setHasEgg(bool has);
	bool isLayingEgg() const { return _layingEgg; }
	void setLayingEgg(bool laying);
	void setHomePos(const BlockPos& pos) { homePos = pos; }
	bool homeCloserThan(double distance) const;
	bool onSand(const BlockPos& pos) const;

	// What its goals share
	BlockPos				homePos{0, 0, 0};
	std::optional<BlockPos> travelPos;
	bool					goingHome	  = false;
	int						layEggCounter = 0;

	void							registerGoals() override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	bool							canFallInLove() const override { return Animal::canFallInLove() && !_hasEgg; }
	bool							isFood(const ItemStack& stack) const override;
	// TurtleBreedGoal.breed: no baby, an egg to lay at home
	void							spawnChildFromBreeding(Animal& partner) override;
	float							getWalkTargetValue(const BlockPos& pos) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							aiStep() override;
	void							travel(const Vec3& input) override;
	void							ageBoundaryReached() override;
	bool							isPushedByFluid() const override { return false; }
	int								ambientSoundInterval() const override { return 200; }
	std::string						ambientSoundName() override;
	std::string						hurtSoundName() const override { return isBaby() ? "minecraft:entity.turtle.hurt_baby" : "minecraft:entity.turtle.hurt"; }
	std::string						deathSoundName() const override { return isBaby() ? "minecraft:entity.turtle.death_baby" : "minecraft:entity.turtle.death"; }
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _hasEgg = false, _layingEgg = false;
};

#endif
