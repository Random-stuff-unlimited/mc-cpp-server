#ifndef GUARDIAN_HPP
#define GUARDIAN_HPP

#include "world/entity/mobs/Monster.hpp"

#include <memory>

class RandomStrollGoal;

// Guardian: swims around the ocean monuments, charges a beam for 4 seconds on players, squids and axolotls further
// than 3 blocks it sees (magic damage, then its attack), and its spikes hurt who hits it while it is still. On land
// it flops around
class Guardian : public Monster {
  public:
	static constexpr int DATA_MOVING = 16, DATA_ATTACK_TARGET = 17;
	static constexpr int EVENT_ATTACK_SOUND = 21;

	Guardian(Level& level, int typeId);

	bool		isMoving() const { return _moving; }
	void		setMoving(bool moving);
	void		setActiveAttackTarget(int entityId);
	bool		hasActiveAttackTarget() const { return _attackTarget != 0; }
	virtual int attackDuration() const { return 80; }
	virtual bool isElder() const { return false; }
	void		triggerStroll();
	using Monster::broadcastEntityEvent; // For its goals

	void  registerGoals() override;
	bool  hurtServer(const Combat::DamageSource& source, float amount) override;
	float getWalkTargetValue(const BlockPos& pos) override;
	int	  maxHeadXRot() const override { return 180; }

  protected:
	std::shared_ptr<RandomStrollGoal> _randomStrollGoal;

	std::unique_ptr<PathNavigation> createNavigation() override;
	void							aiStep() override;
	void							travel(const Vec3& input) override;
	int								ambientSoundInterval() const override { return 160; }
	std::string						ambientSoundName() override { return typeSound(isInWater() ? "ambient" : "ambient_land", ""); }
	std::string						hurtSoundName() const override { return typeSound(isInWater() ? "hurt" : "hurt_land", ""); }
	std::string						deathSoundName() const override { return typeSound(isInWater() ? "death" : "death_land", ""); }
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _moving	   = false;
	int	 _attackTarget = 0;
};

// ElderGuardian: the three of each monument. Stronger, a faster beam, and every minute gives mining fatigue III to
// the players within 50 blocks (with its apparition on their screen)
class ElderGuardian : public Guardian {
  public:
	ElderGuardian(Level& level, int typeId) : Guardian(level, typeId) { setPersistenceRequired(); }
	void registerGoals() override;
	int	 attackDuration() const override { return 60; }
	bool isElder() const override { return true; }

  protected:
	void customServerAiStep() override;
};

#endif
