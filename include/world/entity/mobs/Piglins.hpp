#ifndef PIGLINS_HPP
#define PIGLINS_HPP

#include "world/entity/ai/CrossbowAttack.hpp"
#include "world/entity/mobs/Monster.hpp"

#include <vector>

// AbstractPiglin: the piglins of the Nether, which turn into zombified piglins after 15 seconds anywhere else
class AbstractPiglin : public Monster {
  public:
	static constexpr int DATA_IMMUNE_TO_ZOMBIFICATION = 16;
	static constexpr int CONVERSION_TIME			  = 300;

	AbstractPiglin(Level& level, int typeId);

	bool isImmuneToZombification() const { return _immuneToZombification; }
	void setImmuneToZombification(bool immune);
	bool isConverting() const;
	// PiglinAi.isWearingSafeArmor: a piece of gold armor (#piglin_safe_armor)
	static bool isWearingSafeArmor(Level& level, Actor& actor);
	using Monster::playSound;

	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void		 customServerAiStep() override;
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	virtual void playConvertedSound() = 0;

  private:
	bool _immuneToZombification = false;
	int	 _timeInOverworld		= 0;
};

// Piglin: neutral to the players wearing gold, hostile to the others (and to wither skeletons). It loves gold: it
// picks up gold ingots and the #piglin_loved items it sees, admires them for 6 seconds and, for an ingot, gives
// something back (the bartering table); it gets angry at who hurt it (with the piglins around). Crossbow or golden
// sword. Babies don't fight. Now and then the adults hunt a hoglin (the ones around then wait 30 to 120 seconds before
// another hunt) and may dance after the kill; mining gold or opening a chest near them makes them angry
class Piglin : public AbstractPiglin, public CrossbowAttackMob {
  public:
	static constexpr int DATA_BABY = 17, DATA_IS_CHARGING_CROSSBOW = 18, DATA_IS_DANCING = 19;
	static constexpr int ADMIRE_DURATION = 119;

	Piglin(Level& level, int typeId);

	bool isBaby() const override { return _baby; }
	void setBaby(bool baby) override;
	void setChargingCrossbow(bool charging) override;
	bool isHoldingCrossbow() override;
	bool isAdmiring() const { return _admireTicks > 0; }
	// PiglinAi.wantsToPickup: gold and loved items, when not fighting nor admiring already
	bool wantsToPickUpItem(const ItemStack& stack);
	// PiglinAi.pickUpItem: the item goes into its off hand to be admired
	void startAdmiring(ItemStack stack);
	// PiglinAi.isPlayerHoldingLovedItem: a player it wants to stare at
	bool isLovedItem(const ItemStack& stack);
	const std::vector<ItemStack>& inventory() const { return _inventory; }
	bool						  isDancing() const { return _dancingTicks > 0; }
	// PiglinAi.angerNearbyPiglins: a player broke or opened a block piglins guard (#guarded_by_piglins); with
	// onlyIfSeen, only the ones that see it
	static void					  angerNearby(Level& level, Player& player, bool onlyIfSeen);

	void							registerGoals() override;
	bool							canAttack(Actor& target) override;
	void							performRangedAttack(Actor& target, float power) override;
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							mobInteract(Player& player, int hand) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void customServerAiStep() override;
	void dropAllDeathLoot(const Combat::DamageSource& source) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	void playConvertedSound() override { playSound("minecraft:entity.piglin.converted_to_zombified", 1.0F, 1.0F); }

  private:
	bool				   _baby = false, _chargingCrossbow = false;
	int					   _admireTicks = 0, _dancingTicks = 0, _huntedRecently = 0;
	EntityRef			   _hunted; // The hoglin it hunts

	void tickHunting();
	std::vector<ItemStack> _inventory; // What it keeps (8 slots in vanilla)

	void finishAdmiring();
	void throwItem(ItemStack stack);
};

// PiglinBrute: a guard of the bastions: always hostile to players (gold or not) and wither skeletons, a golden axe,
// never despawns, never barters
class PiglinBrute : public AbstractPiglin {
  public:
	PiglinBrute(Level& level, int typeId);

	void							registerGoals() override;
	bool							removeWhenFarAway(double) const override { return false; }
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return getTarget() ? "minecraft:entity.piglin_brute.angry" : "minecraft:entity.piglin_brute.ambient"; }

  protected:
	void playConvertedSound() override { playSound("minecraft:entity.piglin_brute.converted_to_zombified", 1.0F, 1.0F); }
};

#endif
