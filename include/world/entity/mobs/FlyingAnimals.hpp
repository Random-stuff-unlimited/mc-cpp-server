#ifndef FLYING_ANIMALS_HPP
#define FLYING_ANIMALS_HPP

#include "world/entity/ai/NeutralAnger.hpp"
#include "world/entity/mobs/TamableAnimal.hpp"

// The view direction of a mob (LivingEntity.getViewVector), for the random flying positions
Vec3 viewVectorOf(const Mob& mob);

// Parrot: flies around (it flutters down slowly), tamed with seeds (1 in 10), killed by cookies, imitates the hostile
// mobs around now and then, follows and sits for its owner, lands on its shoulder. Dancing to jukeboxes isn't ported
class Parrot : public TamableAnimal {
  public:
	static constexpr int DATA_VARIANT = 19, VARIANT_COUNT = 5;

	Parrot(Level& level, int typeId);

	int	 variant() const { return _variant; }
	void setVariant(int variant);
	bool isFlying() const { return !_onGround; }
	// rideCooldownCounter: it lands on a shoulder after 5 seconds in the level (a parrot back from a shoulder is new)
	int	 lastShoulderTick() const { return 0; }

	void							registerGoals() override;
	bool							mobInteract(Player& player, int hand) override;
	bool							isFood(const ItemStack&) const override { return false; } // Not bred
	bool							canMate(Animal&) override { return false; }
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	bool							canFlyToOwner() const override { return true; }
	void							aiStep() override;
	void							checkFallDamage(double, bool, const BlockPos&) override {}
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int _variant = 0;
};

// Bee: flies between flowers and its hive. It pollinates a flower (hovering over it for a while), then brings the
// nectar home (the hive's honey level rises) and grows the crops it flies over on the way. Hurt, it gets angry with
// the whole swarm around, stings (poison), loses its stinger and dies soon after. Tempted and bred with flowers.
// Going inside the hive (its block entity, nights and rain) isn't ported: bees stay out and rest near it
class Bee : public Animal {
  public:
	static constexpr int DATA_FLAGS = 17; // Its anger time (18) is left at its default: the client only colors its eyes with it
	static constexpr int FLAG_ROLL = 2, FLAG_HAS_STUNG = 4, FLAG_HAS_NECTAR = 8;

	Bee(Level& level, int typeId);

	bool hasNectar() const { return _beeFlags & FLAG_HAS_NECTAR; }
	void setHasNectar(bool nectar);
	bool hasStung() const { return _beeFlags & FLAG_HAS_STUNG; }
	bool hasHive() const { return _hasHive; }
	const BlockPos& hivePos() const { return _hivePos; }
	void setHive(const BlockPos& pos) { _hivePos = pos, _hasHive = true; }
	void clearHive() { _hasHive = false; }
	bool hasSavedFlower() const { return _hasSavedFlower; }
	const BlockPos& savedFlower() const { return _savedFlower; }
	void setSavedFlower(const BlockPos& pos) { _savedFlower = pos, _hasSavedFlower = true; }
	void clearSavedFlower() { _hasSavedFlower = false; }
	int	 cropsGrownSincePollination = 0, ticksWithoutNectarSinceExitingHive = 0, stayOutOfHiveCountdown = 0;
	// Bee.wantsToEnterHive: home with nectar, at night, in the rain, or tired of looking for flowers
	bool wantsToEnterHive();
	// Bee.isFlowerValid / isHiveValid
	bool isFlower(const BlockPos& pos);
	bool isHive(const BlockPos& pos);
	bool isAngry() const { return _anger.isAngry(); }
	NeutralAnger& anger() { return _anger; }
	// Bee.dropOffNectar: the nectar goes into its hive (one more honey level)
	void dropOffNectar();

	void							registerGoals() override;
	bool							doHurtTarget(Actor& target) override;
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	void							setTarget(Actor* target) override;
	bool							isFood(const ItemStack& stack) const override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return isAngry() ? "minecraft:entity.bee.loop_aggressive" : "minecraft:entity.bee.loop"; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;
	using Animal::playSound;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							customServerAiStep() override;
	void							checkFallDamage(double, bool, const BlockPos&) override {}
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t		 _beeFlags = 0;
	NeutralAnger _anger;
	BlockPos	 _hivePos, _savedFlower;
	bool		 _hasHive = false, _hasSavedFlower = false;
	int			 _timeSinceSting = 0, _underWaterTicks = 0;

	void setFlag(int flag, bool value);
};

#endif
