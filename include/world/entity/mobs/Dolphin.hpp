#ifndef DOLPHIN_HPP
#define DOLPHIN_HPP

#include "world/entity/mobs/WaterAnimals.hpp"

// Dolphin: swims fast and smoothly, comes up to breathe (4 minutes of air), leaps out of the water, plays with the
// items that float around, swims along the players that swim (dolphin's grace), dries out after 2 minutes on land.
// Fights back in groups. Fed fish it would lead to a treasure: structures can't be located yet, so it only eats
class Dolphin : public AgeableWaterCreature {
  public:
	static constexpr int DATA_GOT_FISH = 17, DATA_MOISTNESS_LEVEL = 18;
	static constexpr int TOTAL_AIR_SUPPLY = 4800, TOTAL_MOISTNESS_LEVEL = 2400;

	Dolphin(Level& level, int typeId);

	bool gotFish() const { return _gotFish; }
	void setGotFish(bool got);
	int	 moistnessLevel() const { return _moistness; }
	void setMoistnessLevel(int level);
	// Entity.getMotionDirection: the way it faces, as steps along x and z
	void motionStep(int& x, int& z) const;

	void							registerGoals() override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							tick() override;
	bool							mobInteract(Player& player, int hand) override;
	bool							canAttack(Actor& target) override { return !isBaby() && AgeableWaterCreature::canAttack(target); }
	void							playAttackSound() override { playSound("minecraft:entity.dolphin.attack", 1.0F, 1.0F); }
	using AgeableWaterCreature::playSound; // For its goals
	int								maxAirSupply() const override { return TOTAL_AIR_SUPPLY; }
	int								increaseAirSupply(int) override { return TOTAL_AIR_SUPPLY; }
	int								maxHeadXRot() const override { return 1; }
	int								maxHeadYRot() const override { return 1; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							travel(const Vec3& input) override;
	void							pickUpItem(ItemEntity& item) override;
	bool							suffocatesOnLand() const override { return false; }
	float							babySpawnChance() const override { return 0.1F; }
	std::string						ambientSoundName() override;
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _gotFish	= false;
	int	 _moistness = TOTAL_MOISTNESS_LEVEL;
};

#endif
