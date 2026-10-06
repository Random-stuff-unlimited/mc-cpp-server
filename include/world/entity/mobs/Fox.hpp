#ifndef FOX_HPP
#define FOX_HPP

#include "world/entity/mobs/Animal.hpp"

#include <memory>

// Fox: sleeps in the shade by day, hunts at night (red ones chickens, rabbits and baby turtles; snow ones fish):
// stalks its prey, crouches, then pounces (and plants its face in snow when it misses). Picks up items and carries one
// in its mouth, eating it if it is food; eats sweet and glow berries; flees players, wolves and polar bears unless it
// trusts the player (bred by one), which it then defends
class Fox : public Animal {
  public:
	static constexpr int DATA_TYPE = 17, DATA_FLAGS = 18, DATA_TRUSTED_0 = 19, DATA_TRUSTED_1 = 20;
	static constexpr int FLAG_SITTING = 1, FLAG_CROUCHING = 4, FLAG_INTERESTED = 8, FLAG_POUNCING = 16, FLAG_SLEEPING = 32, FLAG_FACEPLANTED = 64,
						 FLAG_DEFENDING = 128;
	static constexpr int VARIANT_RED = 0, VARIANT_SNOW = 1;
	static constexpr int EVENT_EATING = 45;

	Fox(Level& level, int typeId);

	bool isSitting() const { return _foxFlags & FLAG_SITTING; }
	void setSitting(bool value) { setFlag(FLAG_SITTING, value); }
	bool isFaceplanted() const { return _foxFlags & FLAG_FACEPLANTED; }
	void setFaceplanted(bool value) { setFlag(FLAG_FACEPLANTED, value); }
	bool isDefending() const { return _foxFlags & FLAG_DEFENDING; }
	void setDefending(bool value) { setFlag(FLAG_DEFENDING, value); }
	bool isSleeping() const { return _foxFlags & FLAG_SLEEPING; }
	void setSleeping(bool value) { setFlag(FLAG_SLEEPING, value); }
	bool isPouncing() const { return _foxFlags & FLAG_POUNCING; }
	void setIsPouncing(bool value) { setFlag(FLAG_POUNCING, value); }
	bool isCrouching() const { return _foxFlags & FLAG_CROUCHING; }
	void setIsCrouching(bool value) { setFlag(FLAG_CROUCHING, value); }
	bool isInterested() const { return _foxFlags & FLAG_INTERESTED; }
	void setIsInterested(bool value) { setFlag(FLAG_INTERESTED, value); }
	bool isFullyCrouched() const { return _crouchAmount == 3.0F; }
	void resetCrouchAmount() { _crouchAmount = 0.0F; }
	void wakeUp() { setSleeping(false); }
	void clearStates();
	bool canMove() const { return !isSleeping() && !isSitting() && !isFaceplanted(); }
	int	 variant() const { return _variant; }
	void setVariant(int variant);
	bool trusts(const Actor& actor) const;
	void addTrusted(const EntityRef& ref);
	const EntityRef& trusted(int index) const { return _trusted[index]; }
	// Fox.isPathClear: nothing solid in the 3 blocks above the way to its target
	bool isPathClear(Actor& target);
	bool hasInput() const { return _xxa != 0.0F || _yya != 0.0F || _zza != 0.0F; }
	using Animal::playSound;

	void							registerGoals() override;
	void							tick() override;
	void							setTarget(Actor* target) override;
	bool							isFood(const ItemStack& stack) const override;
	bool							canHoldItem(const ItemStack& stack) override;
	void							spawnChildFromBreeding(Animal& partner) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	void							populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						lootComponent(const std::string& component) const override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void		aiStep() override;
	void		pickUpItem(ItemEntity& item) override;
	void		dropAllDeathLoot(const Combat::DamageSource& source) override;
	float		babyEyeHeight() const override { return 0.2975F; }
	std::string ambientSoundName() override;
	void		playEatingSound() override { playSound("minecraft:entity.fox.eat", 1.0F, 1.0F); }
	void		writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int					  _variant	= VARIANT_RED;
	uint8_t				  _foxFlags = 0;
	EntityRef			  _trusted[2];
	float				  _crouchAmount		= 0;
	int					  _ticksSinceEaten	= 0;
	bool				  _targetGoalsSet	= false;
	std::shared_ptr<Goal> _landTargetGoal, _turtleEggTargetGoal, _fishTargetGoal;

	void setFlag(int flag, bool value);
	void setTargetGoals();
	bool isFoodItem(const ItemStack& stack) const;
	void spitOutItem(const ItemStack& stack);
};

#endif
