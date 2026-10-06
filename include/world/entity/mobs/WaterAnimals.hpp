#ifndef WATER_ANIMALS_HPP
#define WATER_ANIMALS_HPP

#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/Animal.hpp"

// RandomSwimmingGoal: a stroll to a random position in something swimmable (BehaviorUtils.getRandomSwimmablePos)
class RandomSwimmingGoal : public RandomStrollGoal {
  public:
	RandomSwimmingGoal(Mob& mob, double speed, int interval) : RandomStrollGoal(mob, speed, interval) {}

  protected:
	std::optional<Vec3> getPosition() override;
};

// BreathAirGoal: under 7 seconds of air left, up to the nearest air above (nothing interrupts it)
class BreathAirGoal : public Goal {
  public:
	explicit BreathAirGoal(Mob& mob) : _mob(mob) { setFlags({Flag::Move, Flag::Look}); }
	bool canUse() override;
	bool canContinueToUse() override { return canUse(); }
	bool isInterruptable() override { return false; }
	void start() override { findAirPosition(); }
	void tick() override;

  private:
	Mob& _mob;
	void findAirPosition();
};

// TryFindWaterGoal: stranded on the ground, toward water within 2 blocks
class TryFindWaterGoal : public Goal {
  public:
	explicit TryFindWaterGoal(Mob& mob) : _mob(mob) {}
	bool canUse() override;
	void start() override;

  private:
	Mob& _mob;
};

// WaterAnimal: a mob of the water. It can't breathe out of it (its air runs out there, then it suffocates), isn't
// pushed by currents, and leaves 1 to 3 experience
class WaterAnimal : public Mob {
  public:
	WaterAnimal(Level& level, int typeId) : Mob(level, typeId) { setPathfindingMalus(PathType::Water, 0.0F); }

  protected:
	void livingBaseTick() override;
	int	 ambientSoundInterval() const override { return 120; }
	int	 baseExperienceReward() override { return 1 + _random.nextInt(3); }
	bool isPushedByFluid() const override { return false; }
};

// AgeableWaterCreature: the same, for the ones that have babies (squids, dolphins)
class AgeableWaterCreature : public AgeableMob {
  public:
	AgeableWaterCreature(Level& level, int typeId) : AgeableMob(level, typeId) { setPathfindingMalus(PathType::Water, 0.0F); }

  protected:
	void		 livingBaseTick() override;
	// handleAirSupply: dolphins hold their breath instead
	virtual bool suffocatesOnLand() const { return true; }
	int			 ambientSoundInterval() const override { return 120; }
	int			 baseExperienceReward() override { return 1 + _random.nextInt(3); }
	bool		 isPushedByFluid() const override { return false; }
};

// Squid: drifts along a movement vector it picks at random, in bursts as its tentacles close; flees who hurt it,
// leaving a cloud of ink. Out of the water it falls and suffocates
class Squid : public AgeableWaterCreature {
  public:
	static constexpr int EVENT_RESET_TENTACLES = 19;

	Squid(Level& level, int typeId);

	void		setMovementVector(const Vec3& v) { _movementVector = v; }
	bool		hasMovementVector() const { return _movementVector.lengthSqr() > 1.0E-5F; }
	bool		wasTouchingWater() const { return isInWater(); }
	void		registerGoals() override;
	bool		hurtServer(const Combat::DamageSource& source, float amount) override;

  protected:
	void			   aiStep() override;
	void			   travel(const Vec3&) override { move(_delta); }
	float			   soundVolume() const override { return 0.4F; }
	virtual const char* inkParticle() const { return "minecraft:squid_ink"; }
	virtual const char* squirtSound() const { return "minecraft:entity.squid.squirt"; }

  private:
	float _xBodyRot = 0, _xBodyRotO = 0, _zBodyRot = 0, _yBodyRotOld = 0;
	float _tentacleMovement = 0, _tentacleSpeed = 0, _rotateSpeed = 0;
	Vec3  _movementVector{};

	void spawnInk();
	Vec3 rotateVector(const Vec3& v) const;
};

// GlowSquid: of the dark depths; it stops glowing for 5 seconds when hurt
class GlowSquid : public Squid {
  public:
	static constexpr int DATA_DARK_TICKS_REMAINING = 17;

	using Squid::Squid;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void		aiStep() override;
	const char* inkParticle() const override { return "minecraft:glow_squid_ink"; }
	const char* squirtSound() const override { return "minecraft:entity.glow_squid.squirt"; }
	void		writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int	 _darkTicks = 0;
	void setDarkTicks(int ticks);
};

// AbstractFish: swims at random (water navigation), flees players, flops on land. A water bucket picks it up
class AbstractFish : public WaterAnimal {
  public:
	static constexpr int DATA_FROM_BUCKET = 16;

	AbstractFish(Level& level, int typeId);

	bool fromBucket() const { return _fromBucket; }
	void setFromBucket(bool from);
	virtual bool canRandomSwim() const { return true; }

	void registerGoals() override;
	bool removeWhenFarAway(double) const override { return !_fromBucket; }
	void checkDespawn() override;
	int	 getMaxSpawnClusterSize() const override { return 8; }
	bool mobInteract(Player& player, int hand) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							travel(const Vec3& input) override;
	void							aiStep() override;
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	// "minecraft:cod_bucket"...: the type's bucket
	std::string						bucketItem() const;

  private:
	bool _fromBucket = false;
};

// AbstractSchoolingFish: follows a leader of its kind, in a school of up to maxSchoolSize
class AbstractSchoolingFish : public AbstractFish {
  public:
	using AbstractFish::AbstractFish;

	virtual int maxSchoolSize() const { return AbstractFish::getMaxSpawnClusterSize(); }
	int			getMaxSpawnClusterSize() const override { return maxSchoolSize(); }
	bool		canRandomSwim() const override { return !const_cast<AbstractSchoolingFish*>(this)->isFollower(); }
	bool		isFollower();
	AbstractSchoolingFish* leader();
	void		startFollowing(AbstractSchoolingFish& leader);
	void		stopFollowing();
	bool		canBeFollowed() const { return hasFollowers() && _schoolSize < maxSchoolSize(); }
	bool		hasFollowers() const { return _schoolSize > 1; }
	bool		inRangeOfLeader();
	void		pathToLeader();
	// Up to the school's size, the ones given follow it
	void		addFollowers(const std::vector<AbstractSchoolingFish*>& fish);

	void							registerGoals() override;
	void							tick() override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;

  private:
	EntityRef _leader;
	int		  _schoolSize = 1;
};

class Cod : public AbstractSchoolingFish {
  public:
	using AbstractSchoolingFish::AbstractSchoolingFish;
};

// Salmon: small, medium or large; schools of 5
class Salmon : public AbstractSchoolingFish {
  public:
	static constexpr int DATA_TYPE = 17;

	Salmon(Level& level, int typeId) : AbstractSchoolingFish(level, typeId) {}
	int								maxSchoolSize() const override { return 5; }
	void							setVariant(int variant);
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int _variant = 1; // Medium
};

// TropicalFish: a pattern and two colors (a school shares one of the 22 common variants 90 % of the time), schools of 9
class TropicalFish : public AbstractSchoolingFish {
  public:
	static constexpr int DATA_VARIANT = 17;

	using AbstractSchoolingFish::AbstractSchoolingFish;
	int								maxSchoolSize() const override { return 9; }
	int								packedVariant() const { return _variant; }
	void							setPackedVariant(int variant);
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int _variant = 0;
};

// Pufferfish: puffs up when something scary comes within 2 blocks (players but creative ones, most mobs), then
// stings and poisons what touches it; deflates after a while
class Pufferfish : public AbstractFish {
  public:
	static constexpr int DATA_PUFF_STATE = 17;

	Pufferfish(Level& level, int typeId);
	int	 puffState() const { return _puffState; }
	void setPuffState(int state);
	void startPuffing() {
		_inflateCounter = 1;
		_deflateTimer	= 0;
	}
	void stopPuffing() { _inflateCounter = 0; }
	// Pufferfish.SCARY_MOB within `range` blocks of it
	bool scaryNearby(double range);

	void registerGoals() override;
	void tick() override;
	int	 touchedByPlayer(Player& player) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void aiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int _puffState = 0, _inflateCounter = 0, _deflateTimer = 0;
};

#endif
