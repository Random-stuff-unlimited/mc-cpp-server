#ifndef MOB_HPP
#define MOB_HPP

#include "world/DifficultyInstance.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goal.hpp"
#include "world/entity/ai/Navigation.hpp"

#include <array>
#include <functional>
#include <memory>

class ItemEntity;

// SpawnGroupData: what the mobs of a pack share (a zombie pack's babyness, a spider's effect...)
struct SpawnGroupData {
	virtual ~SpawnGroupData() = default;
};

// A mob (vanilla's Mob): a living entity with an AI. Each vanilla mob type is this class configured by its type's
// data (size, attributes, loot table, sounds), or a subclass for the behaviour its type needs (src/world/entity/mobs).
//
// Where the AI plugs in, in vanilla's order (Mob.serverAiStep, run by LivingEntity.aiStep before travel):
//   sensing.tick, targetSelector then goalSelector (all goals on even ticks, running ones only on odd ticks),
//   navigation.tick, customServerAiStep (Brain-based mobs tick their Brain there), moveControl, lookControl,
//   jumpControl
// The goals of a type are registered in src/world/entity/ai/MobGoals.cpp (MobRegistry), or by its class
class Mob : public LivingEntity {
  public:
	// Mob.DATA_MOB_FLAGS_ID: 1 no AI, 2 left handed, 4 aggressive
	static constexpr int DATA_MOB_FLAGS = 15;
	static constexpr int FLAG_NO_AI = 1, FLAG_LEFT_HANDED = 2, FLAG_AGGRESSIVE = 4;
	static constexpr int EVENT_SPAWN_ANIMATION = 20;
	// DropChances: the default chance of each equipment slot, and a guaranteed (preserved) drop
	static constexpr float DEFAULT_DROP_CHANCE = 0.085F, PRESERVED_DROP_CHANCE = 2.0F;

	Mob(Level& level, int typeId);

	GoalSelector&		 goalSelector() { return _goalSelector; }
	GoalSelector&		 targetSelector() { return _targetSelector; }
	MoveControl&		 moveControl() { return *_moveControl; }
	LookControl&		 lookControl() { return *_lookControl; }
	JumpControl&		 jumpControl() { return *_jumpControl; }
	// The navigation (made by createNavigation the first time)
	PathNavigation&		 navigation();
	Sensing&			 sensing() { return _sensing; }
	// Replace a control or the navigation with a type's own (FlyingMoveControl, WaterBoundPathNavigation...)
	void setMoveControl(std::unique_ptr<MoveControl> control) { _moveControl = std::move(control); }
	void setLookControl(std::unique_ptr<LookControl> control) { _lookControl = std::move(control); }
	void setJumpControl(std::unique_ptr<JumpControl> control) { _jumpControl = std::move(control); }
	void setNavigation(std::unique_ptr<PathNavigation> navigation) { _navigation = std::move(navigation); }

	// Mob.getTarget: what it attacks, null if none (or gone, dead, elsewhere)
	Actor*		 getTarget();
	virtual void setTarget(Actor* target);
	// Mob.canAttackType
	virtual bool canAttackType(int typeId) const;
	// LivingEntity.canAttack: a target it may attack (not a creative or spectator player, not itself)
	virtual bool canAttack(Actor& target);

	bool isNoAi() const { return _mobFlags & FLAG_NO_AI; }
	void setNoAi(bool noAi) { setMobFlag(FLAG_NO_AI, noAi); }
	bool isLeftHanded() const { return _mobFlags & FLAG_LEFT_HANDED; }
	void setLeftHanded(bool left) { setMobFlag(FLAG_LEFT_HANDED, left); }
	bool isAggressive() const { return _mobFlags & FLAG_AGGRESSIVE; }
	void setAggressive(bool aggressive) { setMobFlag(FLAG_AGGRESSIVE, aggressive); }
	bool isPersistenceRequired() const { return _persistenceRequired; }
	void setPersistenceRequired() { _persistenceRequired = true; }
	int	 noActionTime() const { return _noActionTime; }
	bool canPickUpLoot() const { return _canPickUpLoot; }
	void setCanPickUpLoot(bool can) { _canPickUpLoot = can; }

	virtual int maxHeadXRot() const { return 40; }
	int			maxHeadYRot() const override { return 75; }
	virtual int headRotSpeed() const { return 10; }
	// Mob.setSpeed: also the forward input
	void setSpeed(float speed) override {
		LivingEntity::setSpeed(speed);
		setZza(speed);
	}
	void setYRotPublic(float yRot) { _yRot = yRot; }

	// ----- Pathfinding -----

	float getPathfindingMalus(PathType type) const;
	void  setPathfindingMalus(PathType type, float malus) { _pathfindingMalus[static_cast<int>(type)] = malus; }
	virtual void onPathfindingStart() {}
	virtual void onPathfindingDone() {}
	int			 getMaxFallDistance() override;
	// PathfinderMob's home: a position it stays within radius of (leashes, golems' villages); radius -1 for none
	bool			hasHome() const { return _homeRadius != -1; }
	const BlockPos& getHomePosition() const { return _homePos; }
	int				getHomeRadius() const { return _homeRadius; }
	void			setHomeTo(const BlockPos& pos, int radius) {
		   _homePos	   = pos;
		   _homeRadius = radius;
	}
	void clearHome() { _homeRadius = -1; }
	bool isWithinHome(const BlockPos& pos) const {
		if (_homeRadius == -1) return true;
		int64_t dx = pos.x - _homePos.x, dy = pos.y - _homePos.y, dz = pos.z - _homePos.z;
		return dx * dx + dy * dy + dz * dz < static_cast<int64_t>(_homeRadius) * _homeRadius;
	}
	// PathfinderMob.getWalkTargetValue: how much it likes a position (0 by default; animals want grass, monsters
	// darkness)
	virtual float getWalkTargetValue(const BlockPos&) { return 0.0F; }

	// ----- Combat -----

	// Mob.doHurtTarget: a melee hit with its attack damage and weapon (enchantments), knockback, fire aspect
	virtual bool doHurtTarget(Actor& target);
	// Mob.isWithinMeleeAttackRange: its box widened by the reach touches the target's
	bool		 isWithinMeleeAttackRange(Actor& target);
	// Mob.lookAt: turns its body toward the target, by at most these degrees
	void		 lookAt(Actor& target, float maxYRot, float maxXRot);
	// Mob.isSunBurnTick: in daylight, under the open sky, not wet
	bool		 isSunBurnTick();
	// Entity.getLightLevelDependentMagicValue: the light at its eyes, 0 to 1 (0 in unloaded chunks)
	float		 lightLevelDependentMagicValue();
	virtual void playAttackSound() {}

	// ----- Equipment -----

	void		 setDropChance(EquipmentSlot slot, float chance) { _dropChances[static_cast<int>(slot)] = chance; }
	void		 setGuaranteedDrop(EquipmentSlot slot) { setDropChance(slot, PRESERVED_DROP_CHANCE); }
	// Mob.populateDefaultEquipmentSlots: armor pieces at random, better with the difficulty
	virtual void populateDefaultEquipmentSlots(DifficultyInstance& difficulty);
	// Mob.populateDefaultEquipmentEnchantments
	virtual void populateDefaultEquipmentEnchantments(DifficultyInstance& difficulty);
	// Mob.getEquipmentForSlot: the armor of a slot for a material tier (0 leather .. 5 diamond)
	int			 equipmentForSlot(EquipmentSlot slot, int tier) const;
	// Mob.equipItemIfPossible, from an item it picks up
	ItemStack	 equipItemIfPossible(const ItemStack& stack);
	virtual bool wantsToPickUp(const ItemStack& stack) { return canHoldItem(stack); }
	virtual bool canHoldItem(const ItemStack&) { return true; }

	// Mob.ate: it ate grass (sheep regrow their wool)
	virtual void ate() {}
	// PathfinderMob.isPanicking: its PanicGoal runs
	bool		 isPanicking() const;
	// Mob.setBaby (zombies, ageable mobs)
	virtual void setBaby(bool) {}
	// Mob.interact / mobInteract: a player right-clicks it with the item in that hand (0 main, 1 off); true if
	// something happened (the hand swings)
	virtual bool mobInteract(Player& player, int hand);
	// Mob.convertTo (ConversionType.SINGLE): a mob of another type takes its place, with its position, motion, health
	// state, equipment (keepEquipment) and flags; `finalize` runs before it joins the level. This one is discarded.
	// Null if it couldn't be made
	Mob* convertTo(int typeId, bool keepEquipment, bool preserveCanPickUpLoot, const std::function<void(Mob&)>& finalize = nullptr);

	// ----- Natural spawning -----

	// Mob.checkSpawnRules: PathfinderMobs want a walk target value of at least 0 where they spawn
	virtual bool checkSpawnRules(int reason);
	// Mob.checkSpawnObstruction: no liquid, no entity in the way (water mobs: no entity only...)
	virtual bool checkSpawnObstruction();
	// Mob.getMaxSpawnClusterSize: how many of a natural spawn attempt at most (4; wolves 8, ghasts 1...)
	virtual int	 getMaxSpawnClusterSize() const;
	virtual bool isMaxGroupSizeReached(int) const { return false; }

	// Mob.registerGoals: the goals of its class (called once, right after it is made)
	virtual void registerGoals() {}
	// Mob.finalizeSpawn: random follow range bonus, sometimes left handed (subclasses: equipment, babies...). `group`:
	// what the previous mob of the same pack returned (natural spawning), shared by the pack (all babies or none...)
	virtual std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group = nullptr);
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
	EntityRef						_target;
	uint8_t							_mobFlags			 = 0;
	bool							_persistenceRequired = false;
	bool							_canPickUpLoot		 = false;
	int								_ambientSoundTime	 = 0;
	BlockPos						_homePos;
	int								_homeRadius = -1;
	std::array<float, static_cast<int>(PathType::Count)> _pathfindingMalus;
	std::array<float, EQUIPMENT_SLOT_COUNT>				 _dropChances;

	// Mob.createNavigation: walking by default
	virtual std::unique_ptr<PathNavigation> createNavigation();
	void		 serverAiStep() final;
	// Mob.customServerAiStep: the mob's own logic after its goals (Brain.tick for Brain-based mobs)
	virtual void customServerAiStep() {}
	bool		 isEffectiveAi() const override { return !isNoAi(); }
	void		 tickHeadTurn(float) override { _bodyRotationControl.clientTick(); }
	void		 livingBaseTick() override;
	void		 aiStep() override;
	void		 playHurtSound() override;
	virtual int	 ambientSoundInterval() const { return 80; }
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	// Mob.dropCustomDeathLoot: the equipment, by its drop chances
	void		 dropAllDeathLoot(const Combat::DamageSource& source) override;
	// Entity.spawnAtLocation: an item at its feet
	ItemEntity*	 spawnAtLocation(ItemStack stack, float yOffset = 0.0F);
	// Mob.pickUpItem
	virtual void pickUpItem(ItemEntity& item);
	bool		 canReplaceCurrentItem(const ItemStack& candidate, const ItemStack& current, EquipmentSlot slot);
	double		 approximateAttributeWith(const ItemStack& stack, int attribute, EquipmentSlot slot);
	void		 enchantSpawnedEquipment(EquipmentSlot slot, float chance, DifficultyInstance& difficulty);

  private:
	void setMobFlag(int flag, bool set);
};

#endif
