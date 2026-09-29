#ifndef ZOMBIE_HPP
#define ZOMBIE_HPP

#include "world/entity/mobs/Monster.hpp"

// Zombie (and Husk): burns in daylight unless it wears a helmet (which wears out instead), calls reinforcements in
// hard difficulty, sets its victims on fire while burning, turns into a drowned after 30 seconds under water (a husk
// into a zombie), may spawn as a baby (faster, smaller) or with a weapon and armor
class Zombie : public Monster {
  public:
	// Zombie's entity data
	static constexpr int DATA_BABY = 16, DATA_SPECIAL_TYPE = 17, DATA_DROWNED_CONVERSION = 18;

	Zombie(Level& level, int typeId) : Monster(level, typeId) {}

	bool isBaby() const override { return _baby; }
	void setBaby(bool baby) override;
	bool isUnderWaterConverting() const { return _drownedConversion; }
	bool canBreakDoors() const { return _canBreakDoors; }
	void setCanBreakDoors(bool can);

	void registerGoals() override;
	void tick() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	bool doHurtTarget(Actor& target) override;
	void populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	bool wantsToPickUp(const ItemStack& stack) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	// Zombie.addBehaviourGoals: what differs between zombies, husks, drowned and zombified piglins
	virtual void addBehaviourGoals();
	virtual bool isSunSensitive() const { return true; }
	virtual bool convertsInWater() const { return true; }
	// Zombie.doUnderWaterConversion: into a drowned (a husk: into a zombie)
	virtual void doUnderWaterConversion();
	void		 convertToZombieType(const std::string& type);
	// Zombie.handleAttributes: random knockback resistance, follow range, and sometimes a leader
	void		 handleAttributes(float specialMultiplier);
	virtual void randomizeReinforcementsChance();
	void		 aiStep() override;
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _baby = false, _drownedConversion = false, _canBreakDoors = false;
	int	 _inWaterTime = 0, _conversionTime = 0;

	void startUnderWaterConversion(int time);
};

class Husk : public Zombie {
  public:
	using Zombie::Zombie;
	bool doHurtTarget(Actor& target) override;

  protected:
	bool isSunSensitive() const override { return false; }
	void doUnderWaterConversion() override;
};

#endif
