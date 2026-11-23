#ifndef HOGLIN_HPP
#define HOGLIN_HPP

#include "world/entity/mobs/Animal.hpp"
#include "world/entity/mobs/Monster.hpp"

// HoglinBase.hurtAndThrowTarget: a headbutt (half to 1.5 times its attack damage, the full damage for babies) that
// throws the target up and away, by its attack knockback less the target's knockback resistance
bool hoglinHurtAndThrowTarget(Mob& attacker, Actor& target);

// Hoglin: a hostile pig of the Nether. It charges the players and piglins it sees (piglins hunt it back), flees warped
// fungi (and portals and respawn anchors in vanilla, not ported), breeds with crimson fungi, and outside the Nether
// (dimensions that aren't piglin safe) turns into a zoglin after 15 seconds. The Brain is approximated by goals
class Hoglin : public Animal {
  public:
	static constexpr int DATA_IMMUNE_TO_ZOMBIFICATION = 17;
	static constexpr int CONVERSION_TIME			  = 300;
	static constexpr int EVENT_ATTACK				  = 4;

	Hoglin(Level& level, int typeId);

	bool isImmuneToZombification() const { return _immuneToZombification; }
	void setImmuneToZombification(bool immune);
	bool isConverting() const;
	int	 timeInOverworld() const { return _timeInOverworld; }

	void		registerGoals() override;
	bool		doHurtTarget(Actor& target) override;
	bool		removeWhenFarAway(double) const override { return !isPersistenceRequired(); }
	std::string ambientSoundName() override { return getTarget() ? "minecraft:entity.hoglin.angry" : "minecraft:entity.hoglin.ambient"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;

  protected:
	void customServerAiStep() override;
	int	 baseExperienceReward() override { return 5; }
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _immuneToZombification = false;
	int	 _timeInOverworld		= 0;
};

// Zoglin: a zombified hoglin, hostile to every living thing but zoglins and creepers (they don't see invisible ones),
// with the hoglin's headbutt; it never despawns while angry... like any monster, it despawns far from players
class Zoglin : public Monster {
  public:
	static constexpr int DATA_BABY = 16;

	Zoglin(Level& level, int typeId) : Monster(level, typeId) { _xpReward = 5; }

	bool isBaby() const override { return _baby; }
	void setBaby(bool baby) override;

	void		registerGoals() override;
	bool		doHurtTarget(Actor& target) override;
	bool		canAttackType(int typeId) const override;
	std::string ambientSoundName() override { return getTarget() ? "minecraft:entity.zoglin.angry" : "minecraft:entity.zoglin.ambient"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;

  protected:
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _baby = false;
};

// Giant: a huge zombie without any AI (vanilla's has none: it stands where it is summoned)
class Giant : public Monster {
  public:
	using Monster::Monster;
	// Giant.getWalkTargetValue
	float getWalkTargetValue(const BlockPos& pos) override;
};

#endif
