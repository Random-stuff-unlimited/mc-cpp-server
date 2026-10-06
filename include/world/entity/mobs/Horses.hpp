#ifndef HORSES_HPP
#define HORSES_HPP

#include "world/entity/mobs/Animal.hpp"

// AbstractHorse: wild, it throws its rider off until its temper lets it be tamed (food raises it); tamed and saddled,
// its rider's client moves it. Eats grass, rears, its speed, jump and health are drawn at spawn and bred from its
// parents'. The inventory screen (chests, taking the saddle back) isn't ported
class AbstractHorse : public Animal {
  public:
	static constexpr int DATA_FLAGS = 17;
	static constexpr int FLAG_TAME = 2, FLAG_BRED = 8, FLAG_EATING = 16, FLAG_STANDING = 32, FLAG_OPEN_MOUTH = 64;

	using Animal::Animal;

	bool isTamed() const { return _horseFlags & FLAG_TAME; }
	void setTamed(bool value) { setFlag(FLAG_TAME, value); }
	bool isEating() const { return _horseFlags & FLAG_EATING; }
	void setEating(bool value) { setFlag(FLAG_EATING, value); }
	bool isStanding() const { return _horseFlags & FLAG_STANDING; }
	bool isBred() const { return _horseFlags & FLAG_BRED; }
	void setBred(bool value) { setFlag(FLAG_BRED, value); }
	int	 temper() const { return _temper; }
	virtual int maxTemper() const { return 100; }
	int	 modifyTemper(int amount);
	void setStanding(int ticks);
	void clearStanding();
	void standIfPossible();
	void makeMad();
	void tameWithName(Player& player);
	// PlayerRideableJumping.handleStartJump
	void handleStartJump();
	bool canParent();
	int	 ambientStandInterval() const { return ambientSoundInterval(); }
	std::string ambientStandSound() { return ambientSoundName(); }
	void broadcastHorseEvent(int event) { broadcastEntityEvent(event); }
	using Animal::playSound;

	void							registerGoals() override;
	void							tick() override;
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							mobInteract(Player& player, int hand) override;
	bool							isFood(const ItemStack& stack) const override;
	bool							canMate(Animal&) override { return false; }
	std::unique_ptr<Mob>			getBreedOffspring(Animal&) override { return nullptr; }
	bool							isControlledByRider() override { return isSaddled() && rider() != nullptr; }
	bool							isImmobile() const override;
	bool							onClimbable() override { return false; }
	int								getMaxSpawnClusterSize() const override { return 6; }
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	int		  _temper = 0;
	EntityRef _owner;

	virtual void addBehaviourGoals();
	virtual bool canPerformRearing() const { return true; }
	virtual bool canEatGrass() const { return true; }
	virtual void randomizeAttributes(JavaRandom&) {}
	// AbstractHorse.handleEating: what each food heals, ages and tames
	virtual bool handleEating(Player& player, const ItemStack& stack);
	bool		 fedFood(Player& player, int hand);
	void		 doPlayerRide(Player& player);
	void		 setOffspringAttributes(Animal& partner, AbstractHorse& child);
	void		 aiStep() override;
	float		 soundVolume() const override { return 0.8F; }
	int			 ambientSoundInterval() const override { return 400; }
	float		 babySpawnChance() const override { return 0.2F; }
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	void		 setFlag(int flag, bool value);

	static float  generateMaxHealth(JavaRandom& random) { return 15.0F + random.nextInt(8) + random.nextInt(9); }
	static double generateJumpStrength(JavaRandom& random) { return 0.4F + random.nextDouble() * 0.2 + random.nextDouble() * 0.2 + random.nextDouble() * 0.2; }
	static double generateSpeed(JavaRandom& random) {
		return (0.45F + random.nextDouble() * 0.3 + random.nextDouble() * 0.3 + random.nextDouble() * 0.3) * 0.25;
	}

  private:
	uint8_t _horseFlags = 0;
	int		_eatingCounter = 0, _mouthCounter = 0, _standCounter = 0;
};

// Horse: 7 colors and 5 markings; breeds with horses (a foal) and donkeys (a mule)
class Horse : public AbstractHorse {
  public:
	static constexpr int DATA_TYPE_VARIANT = 18;
	using AbstractHorse::AbstractHorse;

	int	 typeVariant() const { return _typeVariant; }
	void setTypeVariant(int variant);

	bool							mobInteract(Player& player, int hand) override;
	bool							canMate(Animal& other) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void randomizeAttributes(JavaRandom& random) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int _typeVariant = 0;
};

// AbstractChestedHorse: donkeys and mules (their chest needs the inventory screen: not ported)
class AbstractChestedHorse : public AbstractHorse {
  public:
	static constexpr int DATA_CHEST = 18;
	using AbstractHorse::AbstractHorse;
	bool mobInteract(Player& player, int hand) override;

  protected:
	void randomizeAttributes(JavaRandom& random) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
};

class Donkey : public AbstractChestedHorse {
  public:
	using AbstractChestedHorse::AbstractChestedHorse;
	bool				 canMate(Animal& other) override;
	std::unique_ptr<Mob> getBreedOffspring(Animal& partner) override;
};

class Mule : public AbstractChestedHorse {
  public:
	using AbstractChestedHorse::AbstractChestedHorse;
};

// SkeletonHorse, ZombieHorse: tame from the start for who finds one, no breeding (the skeleton trap isn't ported)
class UndeadHorse : public AbstractHorse {
  public:
	using AbstractHorse::AbstractHorse;
	bool mobInteract(Player& player, int hand) override;

  protected:
	void addBehaviourGoals() override;
	bool canEatGrass() const override { return false; }
	void randomizeAttributes(JavaRandom& random) override;
};

// RunAroundLikeCrazyGoal: ridden while wild, it runs around and bucks, until it accepts its rider
class RunAroundLikeCrazyGoal : public Goal {
  public:
	RunAroundLikeCrazyGoal(AbstractHorse& horse, double speed) : _horse(horse), _speedModifier(speed) { setFlags({Flag::Move}); }
	bool canUse() override;
	void start() override;
	bool canContinueToUse() override;
	void tick() override;

  private:
	AbstractHorse& _horse;
	double		   _speedModifier, _posX = 0, _posY = 0, _posZ = 0;
};

#endif
