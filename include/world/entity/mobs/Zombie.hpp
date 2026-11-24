#ifndef ZOMBIE_HPP
#define ZOMBIE_HPP

#include "world/entity/Trading.hpp"
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
	int	 baseExperienceReward() override {
		if (isBaby()) _xpReward = static_cast<int>(_xpReward * 2.5); // As vanilla: each time it is asked
		return Monster::baseExperienceReward();
	}
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

// ZombieVillager: a zombie with a villager's type (by biome) and profession. Given a golden apple while weakened, it
// cures into a villager after 3 to 5 minutes (faster near iron bars and beds)
// VillagerType.byBiome: the villager type of a biome ("minecraft:plains" by default)
const char* villagerTypeOfBiome(const GameData& data, int biome);

class ZombieVillager : public Zombie {
  public:
	static constexpr int DATA_CONVERTING = 19, DATA_VILLAGER_DATA = 20;
	static constexpr int SERIALIZER_VILLAGER_DATA = 18;

	ZombieVillager(Level& level, int typeId);

	bool isConverting() const { return _converting; }
	// VillagerData: minecraft:villager_type and minecraft:villager_profession ids, and the level
	int	 villagerType() const { return _type; }
	int	 profession() const { return _profession; }
	void setVillagerData(int type, int profession, int level);
	void setVillagerConversionTime(int ticks) { _conversionTime = ticks; }
	// What a villager killed by a zombie keeps for when it is cured: its trades (until the server stops: they aren't
	// saved with the zombie) and its experience
	void setTradeOffers(std::vector<MerchantOffer> offers) { _offers = std::move(offers); }
	void setVillagerXp(int xp) { _xp = xp; }

	void tick() override;
	bool mobInteract(Player& player, int hand) override;
	bool removeWhenFarAway(double distanceSqr) const override { return !_converting && _xp == 0; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	bool convertsInWater() const override { return false; }
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _converting	 = false;
	int	 _conversionTime = -1;
	UUID _conversionStarter{};
	bool _hasStarter = false;
	int	 _type = 0, _profession = 0, _villagerLevel = 1, _xp = 0;
	int	 _initialType = 0, _initialProfession = 0;
	std::vector<MerchantOffer> _offers;

	void startConverting(const UUID* starter, int ticks);
	int	 getConversionProgress();
	void finishConversion();
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
