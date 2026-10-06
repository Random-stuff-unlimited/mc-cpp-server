#ifndef WOLF_HPP
#define WOLF_HPP

#include "world/entity/ai/NeutralAnger.hpp"
#include "world/entity/mobs/TamableAnimal.hpp"

// Wolf: wild, it hunts sheep, rabbits, foxes, baby turtles and skeletons, and turns on who hurts it (the pack joins
// in). A bone tames it one time in three: then it follows its owner, sits when told, fights what hurts its owner or
// what its owner hits, has 40 health (meat heals it), a collar that can be dyed, an armor, and can be bred. Its coat
// comes from the biome (a pack shares it), its voice is drawn at random. It shakes itself dry out of the water
class Wolf : public TamableAnimal {
  public:
	static constexpr int DATA_INTERESTED = 19, DATA_COLLAR_COLOR = 20, DATA_REMAINING_ANGER_TIME = 21, DATA_VARIANT = 22, DATA_SOUND_VARIANT = 23;
	static constexpr int EVENT_START_SHAKING = 8, EVENT_CANCEL_SHAKING = 56;
	static constexpr int DEFAULT_COLLAR_COLOR = 14; // DyeColor.RED

	Wolf(Level& level, int typeId);

	int	 collarColor() const { return _collarColor; }
	void setCollarColor(int color);
	int	 variant() const { return _variant; }
	void setVariant(int variant);
	void setSoundVariant(int variant);
	bool isInterested() const { return _interested; }
	void setIsInterested(bool interested);
	NeutralAnger& anger() { return _anger; }
	bool isAngry() const { return _anger.isAngry(); }

	void							registerGoals() override;
	bool							isFood(const ItemStack& stack) const override;
	bool							mobInteract(Player& player, int hand) override;
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							wantsToAttack(Actor& target, Actor& owner) override;
	bool							canMate(Animal& other) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	int								getMaxSpawnClusterSize() const override { return 8; }
	int								maxHeadXRot() const override { return isInSittingPose() ? 20 : TamableAnimal::maxHeadXRot(); }
	void							tick() override;
	void							die(const Combat::DamageSource& source) override;
	std::string						lootComponent(const std::string& component) const override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void		aiStep() override;
	void		applyTamingSideEffects() override;
	void		actuallyHurt(const Combat::DamageSource& source, float amount, int damageType) override;
	float		soundVolume() const override { return 0.4F; }
	std::string ambientSoundName() override;
	std::string hurtSoundName() const override;
	std::string deathSoundName() const override { return variantSound("death"); }
	void		writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool		 _interested = false;
	int			 _collarColor = DEFAULT_COLLAR_COLOR;
	int			 _variant, _defaultVariant, _soundVariant, _defaultSoundVariant;
	int			 _syncedAngerTime = 0;
	bool		 _isWet = false, _isShaking = false;
	float		 _shakeAnim = 0, _shakeAnimO = 0;
	NeutralAnger _anger;

	void		tryToTame(Player& player);
	bool		hasWolfArmor() const;
	std::string variantSound(const char* name) const;
};

#endif
