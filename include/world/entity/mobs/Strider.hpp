#ifndef STRIDER_HPP
#define STRIDER_HPP

#include "world/entity/mobs/Animal.hpp"

class TemptGoal;

// Strider: walks on lava (it stands on its surface), looks for it when out of it, and shivers, slower, away from the
// warmth; water and rain hurt it. Riding one (saddle and warped fungus on a stick) and its jockeys need passengers,
// which aren't ported
class Strider : public Animal {
  public:
	static constexpr int DATA_BOOST_TIME = 17, DATA_SUFFOCATING = 18;

	Strider(Level& level, int typeId);

	bool isSuffocating() const { return _suffocating; }
	void setSuffocating(bool suffocating);

	void							registerGoals() override;
	void							tick() override;
	bool							mobInteract(Player& player, int hand) override;
	bool							isFood(const ItemStack& stack) const override;
	bool							isControlledByRider() override;
	bool							standsOnFluids() const override { return true; }
	bool							canStandOnFluid(const FluidState& fluid) const override;
	bool							isSensitiveToWater() const override { return true; }
	float							getWalkTargetValue(const BlockPos& pos) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							checkFallDamage(double dy, bool onGround, const BlockPos& onPos) override;
	std::string						ambientSoundName() override;
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool	   _suffocating = false;
	TemptGoal* _temptGoal	= nullptr;

	void floatStrider();
};

#endif
