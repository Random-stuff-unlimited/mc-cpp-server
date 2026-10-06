#ifndef FELINES_HPP
#define FELINES_HPP

#include "world/entity/mobs/TamableAnimal.hpp"

#include <memory>

// OcelotAttackGoal: stalks its target (crouching far, sprinting close) and hits it every second when on it
class OcelotAttackGoal : public Goal {
  public:
	explicit OcelotAttackGoal(Mob& mob) : _mob(mob) { setFlags({Flag::Move, Flag::Look}); }
	bool canUse() override;
	bool canContinueToUse() override;
	void stop() override;
	bool requiresUpdateEveryTick() override { return true; }
	void tick() override;

  private:
	Mob&	  _mob;
	EntityRef _target;
	int		  _attackTime = 0;
	Actor*	  target();
};

// Ocelot: flees the players (unless it trusts them: fed fish while it was tempted, one chance in three), crouches or
// sprints by the speed it moves at, hunts chickens and baby turtles on land
class Ocelot : public Animal {
  public:
	static constexpr int DATA_TRUSTING = 17;
	static constexpr int EVENT_TRUSTING_FAILED = 40, EVENT_TRUSTING_SUCCEEDED = 41;

	using Animal::Animal;

	bool isTrusting() const { return _trusting; }
	void registerGoals() override;
	bool mobInteract(Player& player, int hand) override;
	bool isFood(const ItemStack& stack) const override;
	bool removeWhenFarAway(double) const override { return !_trusting && _tickCount > 2400; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void  customServerAiStep() override;
	int	  ambientSoundInterval() const override { return 900; }
	float babySpawnChance() const override { return 1.0F; }
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool					 _trusting = false;
	std::shared_ptr<Goal>	 _avoidPlayersGoal;
	class TemptGoal*		 _temptGoal = nullptr;

	void setTrusting(bool trusting);
	void reassessTrustingGoals();
};

// Cat: a stray flees the players until one tames it with fish; tamed, it follows its owner, sits when told, sits on
// beds, chests and lit furnaces, sleeps next to its sleeping owner (and may bring a gift in the morning). Hunts
// rabbits and baby turtles while wild. Creepers and phantoms keep away from it
class Cat : public TamableAnimal {
  public:
	static constexpr int DATA_VARIANT = 19, DATA_LYING = 20, DATA_RELAX_STATE_ONE = 21, DATA_COLLAR_COLOR = 22;
	static constexpr int DEFAULT_COLLAR_COLOR = 14; // DyeColor.RED

	Cat(Level& level, int typeId);

	bool isLying() const { return _lying; }
	void setLying(bool lying);
	bool isRelaxStateOne() const { return _relaxStateOne; }
	void setRelaxStateOne(bool relax);
	int	 collarColor() const { return _collarColor; }
	void setCollarColor(int color);
	void setVariant(int variant);
	void hiss();
	// CatRelaxOnOwnerGoal.giveMorningGift
	void giveMorningGift();

	void							registerGoals() override;
	void							tick() override;
	bool							mobInteract(Player& player, int hand) override;
	bool							isFood(const ItemStack& stack) const override;
	bool							canMate(Animal& other) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							setTame(bool tame, bool applySideEffects) override;
	bool							removeWhenFarAway(double) const override { return !isTame() && _tickCount > 2400; }
	std::string						lootComponent(const std::string& component) const override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void		customServerAiStep() override;
	std::string ambientSoundName() override;
	void		playEatingSound() override { playSound("minecraft:entity.cat.eat", 1.0F, 1.0F); }
	void		writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int					  _variant = 0, _defaultVariant = 0, _collarColor = DEFAULT_COLLAR_COLOR;
	bool				  _lying = false, _relaxStateOne = false;
	std::shared_ptr<Goal> _avoidPlayersGoal;
	class TemptGoal*	  _temptGoal = nullptr;

	void reassessTameGoals();
};

#endif
