#ifndef CAMELIDS_HPP
#define CAMELIDS_HPP

#include "world/entity/Throwables.hpp"
#include "world/entity/mobs/Horses.hpp"

// Llama: a chested horse that spits (1 damage) at what hurt it and at wild wolves, once per provocation. Its strength
// (1 to 5) is drawn at spawn and bred; four colors. Wild, it is tamed by its temper (30) like a horse but never saddled.
// Caravans (leads) aren't ported
class Llama : public AbstractChestedHorse {
  public:
	static constexpr int DATA_STRENGTH = 19, DATA_VARIANT = 20;

	Llama(Level& level, int typeId) : AbstractChestedHorse(level, typeId) {}

	int	 strength() const { return _strength; }
	void setStrength(int strength);
	int	 variant() const { return _variant; }
	void setVariant(int variant);
	bool didSpit() const { return _didSpit; }
	void setDidSpit(bool spit) { _didSpit = spit; }

	void							registerGoals() override;
	void							performRangedAttack(Actor& target, float power) override;
	bool							isFood(const ItemStack& stack) const override;
	int								maxTemper() const override { return 30; }
	bool							isControlledByRider() override { return false; }
	bool							canMate(Animal& other) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	int								getMaxSpawnClusterSize() const override { return 4; }
	std::string						ambientSoundName() override { return "minecraft:entity.llama.ambient"; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	bool handleEating(Player& player, const ItemStack& stack) override;
	bool canPerformRearing() const override { return false; }
	void randomizeAttributes(JavaRandom& random) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int	 _strength = 1, _variant = 0;
	bool _didSpit = false;
};

// TraderLlama: a wandering trader's llama; it despawns after 40 minutes unless a player tamed it, and defends its
// trader (attacks what hurts a wandering trader nearby)
class TraderLlama : public Llama {
  public:
	TraderLlama(Level& level, int typeId) : Llama(level, typeId) {}

	void registerGoals() override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void aiStep() override;

  private:
	int _despawnDelay = 47999;
};

// LlamaSpit: falls a little (0.06), 1 damage to what it hits
class LlamaSpit : public ThrowableProjectile {
  public:
	LlamaSpit(Level& level, Llama& spitter);
	explicit LlamaSpit(Level& level);
	void writeEntityData(Buffer&) const override {} // No item: llama spit has no entity data of its own

  protected:
	double gravity() const override { return 0.06; }
	void   onHit(const Hit& hit) override;
};

// Camel: a big desert mount for two (a saddle is enough, no taming). It sits down now and then (and stands up when it
// must move), eats cacti, panics when hurt. Dashing (its rider's jump) isn't ported
class Camel : public AbstractHorse {
  public:
	static constexpr int DATA_DASH = 18, DATA_LAST_POSE_CHANGE_TICK = 19;
	static constexpr int POSE_SITTING = 10;

	Camel(Level& level, int typeId);

	bool	isCamelSitting() const { return _lastPoseChangeTick < 0; }
	int64_t poseTime() const;
	void	sitDown();
	void	standUp();

	void							registerGoals() override;
	bool							mobInteract(Player& player, int hand) override;
	bool							isFood(const ItemStack& stack) const override;
	bool							isImmobile() const override { return isCamelSitting() || AbstractHorse::isImmobile(); }
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							canMate(Animal& other) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return "minecraft:entity.camel.ambient"; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	bool handleEating(Player& player, const ItemStack& stack) override;
	bool canPerformRearing() const override { return false; }
	bool canEatGrass() const override { return false; }
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int64_t _lastPoseChangeTick = 0;

	void resetLastPoseChangeTick(int64_t tick);
};

#endif
