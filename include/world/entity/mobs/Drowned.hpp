#ifndef DROWNED_HPP
#define DROWNED_HPP

#include "world/entity/mobs/Zombie.hpp"

// Drowned: a zombie of the waters (amphibious navigation). By day it stays in the water and only attacks what is in
// it; at night it swims up and walks ashore. One in ten holds a trident (or a fishing rod) and throws it from afar
class Drowned : public Zombie {
  public:
	Drowned(Level& level, int typeId);

	bool isSunSensitive() const override { return false; }
	bool convertsInWater() const override { return false; }
	void addBehaviourGoals() override;
	void populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void performRangedAttack(Actor& target, float power) override;
	// Drowned.okTarget: at night anything, by day only what is in the water
	bool okTarget(Actor* target);
	bool wantsToSwim();
	bool searchingForLand() const { return _searchingForLand; }
	void setSearchingForLand(bool searching) { _searchingForLand = searching; }
	// Close to the end of its path
	bool closeToNextPos();
	bool isUnderWater() const { return _eyeInWater && isInWater(); }

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							travel(const Vec3& input) override;
	bool							isPushedByFluid() const override { return !_swimming; }
	bool							canReplaceCurrentItem(const ItemStack& candidate, const ItemStack& current, EquipmentSlot slot) override;
	void							aiStep() override;
	std::string						ambientSoundName() override { return isInWater() ? "minecraft:entity.drowned.ambient_water" : "minecraft:entity.drowned.ambient"; }
	std::string						hurtSoundName() const override { return isInWater() ? "minecraft:entity.drowned.hurt_water" : "minecraft:entity.drowned.hurt"; }
	std::string						deathSoundName() const override { return isInWater() ? "minecraft:entity.drowned.death_water" : "minecraft:entity.drowned.death"; }

  private:
	bool _searchingForLand = false, _swimming = false;
};

#endif
