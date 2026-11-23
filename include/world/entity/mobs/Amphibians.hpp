#ifndef AMPHIBIANS_HPP
#define AMPHIBIANS_HPP

#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/entity/mobs/WaterAnimals.hpp"

// Frog: walks and swims (amphibious), catches the small slimes and magma cubes it sees with its tongue (a magma cube
// gives a froglight of its variant's color), is tempted and bred with slime balls, and after breeding lays frogspawn on
// the water next to land. Its variant (temperate, warm, cold) comes from the biome. Long jumps aren't ported
class Frog : public VariantAnimal {
  public:
	static constexpr int POSE_CROAKING = 8, POSE_USING_TONGUE = 9;

	Frog(Level& level, int typeId);

	bool hasEgg() const { return _hasEgg; }
	void setHasEgg(bool egg) { _hasEgg = egg; }
	// Frog.canEat: a tiny slime or magma cube (the #frog_food)
	bool canEat(Actor& actor);
	// The tongue: what it eats is gone (a magma cube leaves a froglight)
	void eat(Actor& prey);

	void		registerGoals() override;
	void		spawnChildFromBreeding(Animal& partner) override;
	bool		canAttack(Actor& target) override { return canEat(target) && VariantAnimal::canAttack(target); }
	std::string ambientSoundName() override { return "minecraft:entity.frog.ambient"; }
	int			maxHeadYRot() const override { return 35; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;
	using VariantAnimal::playSound;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							travel(const Vec3& input) override;

  private:
	bool _hasEgg = false;
};

// Tadpole: a little fish that grows into a frog in 20 minutes (slime balls speed it up), caught with a water bucket
class Tadpole : public AbstractFish {
  public:
	static constexpr int MAX_AGE = 24000;

	Tadpole(Level& level, int typeId) : AbstractFish(level, typeId) {}

	int	 age() const { return _age; }
	void setAge(int age) { _age = age; }

	void registerGoals() override;
	bool mobInteract(Player& player, int hand) override;
	bool removeWhenFarAway(double) const override { return false; }
	int	 getMaxSpawnClusterSize() const override { return 4; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void aiStep() override;

  private:
	int _age = 0;

	void ageUp(int ticks);
};

// Axolotl: an amphibian of the lush caves. It hunts fish, squids, drowned and guardians, plays dead when badly hurt in
// the water (healing meanwhile), and dries out after 5 minutes out of the water. Tempted and bred with buckets of
// tropical fish; caught with a water bucket. Five colors (blue is rare: 1 in 1200, for babies only)
class Axolotl : public Animal {
  public:
	static constexpr int DATA_VARIANT = 17, DATA_PLAYING_DEAD = 18, DATA_FROM_BUCKET = 19;
	static constexpr int VARIANT_BLUE = 4, VARIANT_COUNT = 5;
	static constexpr int AXOLOTL_TOTAL_AIR_SUPPLY = 6000, PLAY_DEAD_TICKS = 200;

	Axolotl(Level& level, int typeId);

	int	 variant() const { return _variant; }
	void setVariant(int variant);
	bool isPlayingDead() const { return _playDeadTicks > 0; }
	void setPlayingDead(int ticks);
	bool fromBucket() const { return _fromBucket; }
	void setFromBucket(bool from);

	void							registerGoals() override;
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							mobInteract(Player& player, int hand) override;
	bool							isFood(const ItemStack& stack) const override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	bool							removeWhenFarAway(double) const override { return !_fromBucket && !isPersistenceRequired(); }
	int								maxAirSupply() const override { return AXOLOTL_TOTAL_AIR_SUPPLY; }
	int								getMaxSpawnClusterSize() const override { return 4; }
	std::string						ambientSoundName() override { return isInWaterNow() ? "minecraft:entity.axolotl.idle_water" : "minecraft:entity.axolotl.idle_air"; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							travel(const Vec3& input) override;
	void							livingBaseTick() override;
	void							customServerAiStep() override;
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int	 _variant = 0, _playDeadTicks = 0;
	bool _fromBucket = false;
};

#endif
