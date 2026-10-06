#ifndef ZOMBIFIED_PIGLIN_HPP
#define ZOMBIFIED_PIGLIN_HPP

#include "world/entity/ai/NeutralAnger.hpp"
#include "world/entity/mobs/Zombie.hpp"

// ZombifiedPiglin: a zombie of the Nether, neutral until one is hurt: then it is angry for 20 to 39 seconds, runs
// faster, and every 4 to 6 seconds calls the others around onto its target. It doesn't burn in the sun, convert in
// water nor call reinforcements, and always holds a golden sword
class ZombifiedPiglin : public Zombie {
  public:
	ZombifiedPiglin(Level& level, int typeId);

	NeutralAnger& anger() { return _anger; }
	bool		  isAngry() const { return _anger.isAngry(); }

	bool isSunSensitive() const override { return false; }
	bool convertsInWater() const override { return false; }
	void addBehaviourGoals() override;
	void populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	void setTarget(Actor* target) override;
	bool isPreventingPlayerRest(Player& player) const override;
	bool wantsToPickUp(const ItemStack& stack) override { return canHoldItem(stack); }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void		customServerAiStep() override;
	void		randomizeReinforcementsChance() override;
	std::string ambientSoundName() override;

  private:
	NeutralAnger _anger;
	int			 _playFirstAngerSoundIn = 0, _ticksUntilNextAlert = 0;

	void alertOthers();
};

#endif
