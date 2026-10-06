#ifndef RABBIT_HPP
#define RABBIT_HPP

#include "world/entity/mobs/Animal.hpp"

// Rabbit: moves by hops (its own jump and move controls: it only has speed while in the air), flees players, wolves
// and monsters, eats the ripe carrots of the farms around. Its coat comes from the biome (a pack shares it). The
// killer bunny (variant 99) attacks players and wolves
class Rabbit : public Animal {
  public:
	static constexpr int DATA_TYPE = 17;
	enum Variant { Brown = 0, White = 1, Black = 2, WhiteSplotched = 3, Gold = 4, Salt = 5, Evil = 99 };

	Rabbit(Level& level, int typeId);

	int	 variant() const { return _variant; }
	void setVariant(int variant);
	void setSpeedModifier(double speed);
	void startJumping();
	bool wantsMoreFood() const { return _moreCarrotTicks <= 0; }
	void setMoreCarrotTicks(int ticks) { _moreCarrotTicks = ticks; }

	void							registerGoals() override;
	void							setJumping(bool jumping) override;
	void							jumpFromGround() override;
	void							playAttackSound() override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::string						lootComponent(const std::string& component) const override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void  customServerAiStep() override;
	void  aiStep() override;
	float jumpPowerFactor() override;
	float babySpawnChance() const override { return 1.0F; } // RabbitGroupData: AgeableMobGroupData(1.0)
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int	 _variant = Brown;
	int	 _jumpTicks = 0, _jumpDuration = 0, _jumpDelayTicks = 0, _moreCarrotTicks = 0;
	bool _wasOnGround = false;

	void facePoint(double x, double z);
	int	 randomVariant(const BlockPos& pos);
};

#endif
