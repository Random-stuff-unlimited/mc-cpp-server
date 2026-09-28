#ifndef MOB_HPP
#define MOB_HPP

#include "world/entity/LivingEntity.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goal.hpp"

#include <memory>

// A mob (vanilla's Mob): a living entity with an AI. Every vanilla mob type is this class for now, configured by its
// type's data (size, attributes, loot table, sounds); subclasses only for behaviour its type needs.
//
// Where the AI plugs in, in vanilla's order (Mob.serverAiStep, run by LivingEntity.aiStep before travel):
//   sensing.tick, targetSelector then goalSelector (all goals on even ticks, running ones only on odd ticks),
//   navigation.tick, customServerAiStep (Brain-based mobs tick their Brain there), moveControl, lookControl,
//   jumpControl
// The goals of a type are registered in src/world/entity/ai/MobGoals.cpp (MobRegistry::registerGoals), called once
// when the mob is created (Mob.registerGoals). Brain-based mobs (villagers, piglins...) override customServerAiStep
class Mob : public LivingEntity {
  public:
	// Mob.DATA_MOB_FLAGS_ID: 1 no AI, 2 left handed, 4 aggressive
	static constexpr int DATA_MOB_FLAGS = 15;
	static constexpr int FLAG_NO_AI = 1, FLAG_LEFT_HANDED = 2, FLAG_AGGRESSIVE = 4;
	static constexpr int EVENT_SPAWN_ANIMATION = 20;

	Mob(Level& level, int typeId);

	GoalSelector&		 goalSelector() { return _goalSelector; }
	GoalSelector&		 targetSelector() { return _targetSelector; }
	MoveControl&		 moveControl() { return *_moveControl; }
	LookControl&		 lookControl() { return *_lookControl; }
	JumpControl&		 jumpControl() { return *_jumpControl; }
	PathNavigation&		 navigation() { return *_navigation; }
	const PathNavigation& navigation() const { return *_navigation; }
	Sensing&			 sensing() { return _sensing; }
	// Replace a control or the navigation with a type's own (FlyingMoveControl, WaterBoundPathNavigation...)
	void setMoveControl(std::unique_ptr<MoveControl> control) { _moveControl = std::move(control); }
	void setLookControl(std::unique_ptr<LookControl> control) { _lookControl = std::move(control); }
	void setJumpControl(std::unique_ptr<JumpControl> control) { _jumpControl = std::move(control); }
	void setNavigation(std::unique_ptr<PathNavigation> navigation) { _navigation = std::move(navigation); }

	// The entity it attacks (Mob.target), by entity id; -1 for none
	int	 target() const { return _target; }
	void setTarget(int entityId) { _target = entityId; }

	bool isNoAi() const { return _mobFlags & FLAG_NO_AI; }
	void setNoAi(bool noAi) { setMobFlag(FLAG_NO_AI, noAi); }
	bool isLeftHanded() const { return _mobFlags & FLAG_LEFT_HANDED; }
	void setLeftHanded(bool left) { setMobFlag(FLAG_LEFT_HANDED, left); }
	bool isAggressive() const { return _mobFlags & FLAG_AGGRESSIVE; }
	void setAggressive(bool aggressive) { setMobFlag(FLAG_AGGRESSIVE, aggressive); }
	bool isPersistenceRequired() const { return _persistenceRequired; }
	void setPersistenceRequired() { _persistenceRequired = true; }
	int	 noActionTime() const { return _noActionTime; }

	int	 maxHeadXRot() const { return 40; }
	int	 maxHeadYRot() const override { return 75; }
	int	 headRotSpeed() const { return 10; }
	// Mob.setSpeed: also the forward input
	void setSpeed(float speed) override {
		LivingEntity::setSpeed(speed);
		setZza(speed);
	}

	// Mob.finalizeSpawn: random follow range bonus, sometimes left handed
	virtual void finalizeSpawn();
	void		 playAmbientSound();
	void		 tick() override;
	void		 checkDespawn() override;
	// Mob.removeWhenFarAway: whether it despawns far from players (animals, golems, villagers... don't)
	virtual bool removeWhenFarAway(double distanceSqr) const;

	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	GoalSelector					_goalSelector, _targetSelector;
	std::unique_ptr<MoveControl>	_moveControl;
	std::unique_ptr<LookControl>	_lookControl;
	std::unique_ptr<JumpControl>	_jumpControl;
	BodyRotationControl				_bodyRotationControl;
	std::unique_ptr<PathNavigation> _navigation;
	Sensing							_sensing;
	int								_target				 = -1;
	uint8_t							_mobFlags			 = 0;
	bool							_persistenceRequired = false;
	int								_ambientSoundTime	 = 0;

	void		 serverAiStep() final;
	// Mob.customServerAiStep: the mob's own logic after its goals (Brain.tick for Brain-based mobs)
	virtual void customServerAiStep() {}
	bool		 isEffectiveAi() const override { return !isNoAi(); }
	void		 tickHeadTurn(float) override { _bodyRotationControl.clientTick(); }
	void		 livingBaseTick() override;
	void		 playHurtSound() override;
	virtual int	 ambientSoundInterval() const { return 80; }
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	void setMobFlag(int flag, bool set);
};

#endif
