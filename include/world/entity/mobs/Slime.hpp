#ifndef SLIME_HPP
#define SLIME_HPP

#include "world/entity/Mob.hpp"

// Slime: moves by jumps (its own move control: it only has speed in the air), turns toward its target or a random
// direction, hurts the players and iron golems it touches unless tiny, and splits into 2 to 4 of half its size when it
// dies. Its size (1, 2 or 4 naturally) sets its box, health, speed and damage
class Slime : public Mob {
  public:
	static constexpr int DATA_SIZE = 16;

	Slime(Level& level, int typeId);

	int			 size() const { return _size; }
	// Slime.setSize: resetHealth for a new one
	virtual void setSize(int size, bool resetHealth);
	bool		 isTiny() const { return _size <= 1; }
	virtual bool isDealsDamage() const { return !isTiny() && isEffectiveAi(); }
	// Slime.getJumpDelay: the ticks between two jumps
	virtual int	 jumpDelay() { return _random.nextInt(20) + 10; }
	void		 playJumpSound();

	void							registerGoals() override;
	void							tick() override;
	void							jumpFromGround() override;
	int								maxHeadXRot() const override { return 0; }
	int								touchedByPlayer(Player& player) override;
	void							pushAgainst(Entity& other) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	// "minecraft:entity.slime." or "minecraft:entity.magma_cube."
	virtual const char* soundPrefix() const { return "minecraft:entity.slime."; }
	virtual float		attackDamage() const { return static_cast<float>(getAttributeValue(_ids.attackDamage)); }
	virtual std::string jumpSound() const { return sizedSound("jump"); }
	std::string			sizedSound(const char* name) const { return std::string(soundPrefix()) + name + (isTiny() ? "_small" : ""); }
	void				dealDamage(Actor& target);
	void				tickDeath() override;
	float				soundVolume() const override { return 0.4F * _size; }
	std::string			hurtSoundName() const override { return sizedSound("hurt"); }
	std::string			deathSoundName() const override { return sizedSound("death"); }
	void				writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

	int	 _size		  = 1;
	bool _wasOnGround = false;
};

// MagmaCube: the slime of the nether. Armored, jumps higher with its size and four times less often, leaps out of
// lava, and even the tiny ones hurt
class MagmaCube : public Slime {
  public:
	using Slime::Slime;
	void setSize(int size, bool resetHealth) override;
	bool isDealsDamage() const override { return isEffectiveAi(); }
	int	 jumpDelay() override { return Slime::jumpDelay() * 4; }
	void jumpFromGround() override;

  protected:
	const char* soundPrefix() const override { return "minecraft:entity.magma_cube."; }
	float		attackDamage() const override { return Slime::attackDamage() + 2.0F; }
	std::string jumpSound() const override { return "minecraft:entity.magma_cube.jump"; }
	void		jumpInLiquid(bool lava) override;
};

#endif
