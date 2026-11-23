#ifndef ILLAGERS_HPP
#define ILLAGERS_HPP

#include "world/entity/Entity.hpp"

#include <optional>
#include "world/entity/ai/CrossbowAttack.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/Monster.hpp"

// Raider (PatrollingMonster and Raider): the mobs of patrols and raids, which don't fight each other
// (AbstractIllager.isAlliedTo: the #raiders). A patrol follows its leader (the captain, with a banner on its head) far
// across the land; killing a captain gives the player Bad Omen. In a raid (VillageEvents) they march on the village
class Raider : public Monster {
  public:
	static constexpr int DATA_IS_CELEBRATING = 16;

	using Monster::Monster;
	using Monster::playSound;
	// Raider.isAlliedTo: another raider
	bool isAlliedTo(Actor& other) const;
	bool canAttack(Actor& target) override { return !isAlliedTo(target) && Monster::canAttack(target); }

	// ----- Patrols -----
	bool						   isPatrolLeader() const { return _patrolLeader; }
	void						   setPatrolLeader(bool leader);
	bool						   isPatrolling() const { return _patrolling; }
	void						   setPatrolling(bool patrolling) { _patrolling = patrolling; }
	const std::optional<BlockPos>& patrolTarget() const { return _patrolTarget; }
	void						   setPatrolTarget(const BlockPos& pos) { _patrolTarget = pos, _patrolling = true; }
	// PatrollingMonster.findPatrolTarget: somewhere up to 500 blocks away
	void						   findPatrolTarget();
	// Raider.isCaptain: the leader of its patrol or raid wave
	bool						   isCaptain() const { return _patrolLeader; }

	// ----- Raids -----
	int	 raidId() const { return _raidId; }
	void setRaidId(int id) { _raidId = id; }
	bool isCelebrating() const { return _celebrating; }
	void setCelebrating(bool celebrating);

	void die(const Combat::DamageSource& source) override;
	bool removeWhenFarAway(double distanceSqr) const override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	// The raiders' common targets: what hurt them (the others join in), players, villagers and iron golems; and their
	// patrol and raid goals
	void addRaiderTargets(bool villagersMustSee);
	// The patrol and raid goals (PatrolGoal, RaiderMoveThroughVillageGoal)
	void addRaiderGoals();
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool					_patrolLeader = false, _patrolling = false, _celebrating = false;
	std::optional<BlockPos> _patrolTarget;
	int						_raidId = -1;
};

// Vindicator: an illager with an iron axe that charges its targets (named "Johnny", it attacks everything)
class Vindicator : public Raider {
  public:
	Vindicator(Level& level, int typeId);
	void							registerGoals() override;
	void							populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return "minecraft:entity.vindicator.ambient"; }

  protected:
	void customServerAiStep() override;
};

// Pillager: shoots its crossbow (load for 25 ticks, wait one or two seconds, fire) from up to 8 blocks
class Pillager : public Raider, public CrossbowAttackMob {
  public:
	static constexpr int DATA_IS_CHARGING_CROSSBOW = 17;

	Pillager(Level& level, int typeId);
	bool isChargingCrossbow() const { return _chargingCrossbow; }
	void setChargingCrossbow(bool charging) override;
	bool isHoldingCrossbow() override;

	void							registerGoals() override;
	void							performRangedAttack(Actor& target, float power) override;
	void							populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return "minecraft:entity.pillager.ambient"; }
	int								getMaxSpawnClusterSize() const override { return 1; }

  protected:
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _chargingCrossbow = false;
};

// SpellcasterIllager: casts a spell (its arms raised, particles for the viewers): the spell goals wait for their warmup,
// then perform it; the casting goal keeps it still and looking at its target meanwhile
class SpellcasterIllager : public Raider {
  public:
	static constexpr int DATA_SPELL_CASTING = 17;
	// SpellcasterIllager.IllagerSpell
	enum class Spell : uint8_t { None = 0, SummonVex = 1, Fangs = 2, Wololo = 3, Disappear = 4, Blindness = 5 };

	using Raider::Raider;
	bool  isCastingSpell() const { return _spell != Spell::None; }
	Spell currentSpell() const { return _spell; }
	void  setIsCastingSpell(Spell spell);
	int	  spellCastingTickCount = 0;

	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void customServerAiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	Spell _spell = Spell::None;
};

// SpellcasterIllager.SpellcasterCastingSpellGoal
class SpellcasterCastingSpellGoal : public Goal {
  public:
	explicit SpellcasterCastingSpellGoal(SpellcasterIllager& caster) : _caster(caster) { setFlags({Flag::Move, Flag::Look}); }
	bool canUse() override { return _caster.spellCastingTickCount > 0; }
	void start() override;
	void stop() override { _caster.setIsCastingSpell(SpellcasterIllager::Spell::None); }
	void tick() override;

  private:
	SpellcasterIllager& _caster;
};

// SpellcasterIllager.SpellcasterUseSpellGoal: at a target, every castingInterval ticks
class SpellcasterUseSpellGoal : public Goal {
  public:
	explicit SpellcasterUseSpellGoal(SpellcasterIllager& caster) : _caster(caster) {}
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void tick() override;

  protected:
	SpellcasterIllager& _caster;
	int					_attackWarmupDelay = 0, _nextAttackTickCount = 0;

	virtual void						performSpellCasting() = 0;
	virtual int							castWarmupTime() const { return 20; }
	virtual int							castingTime() const	  = 0;
	virtual int							castingInterval() const = 0;
	virtual const char*					spellPrepareSound() const = 0;
	virtual SpellcasterIllager::Spell	spell() const			  = 0;
};

// Evoker: keeps away from players and casts: evoker fangs (a line toward its target, or two rings around itself when
// it is close), three vexes (while fewer than one to eight are around), and turns blue sheep red ("wololo")
class Evoker : public SpellcasterIllager {
  public:
	Evoker(Level& level, int typeId);
	EntityRef wololoTarget;

	void		registerGoals() override;
	std::string ambientSoundName() override { return "minecraft:entity.evoker.ambient"; }
};

// Illusioner: a bow, and two spells: invisibility (with mirror images on the client), and blindness on a new target
class Illusioner : public SpellcasterIllager {
  public:
	Illusioner(Level& level, int typeId);

	void							registerGoals() override;
	void							performRangedAttack(Actor& target, float power) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return "minecraft:entity.illusioner.ambient"; }
};

// Vex: a small flying spirit that goes through walls (no physics, no gravity), charges its target, copies its
// evoker's target, and starts dying (1 damage a tick) when its limited life runs out
class Vex : public Monster {
  public:
	static constexpr int DATA_FLAGS = 16, FLAG_IS_CHARGING = 1;

	Vex(Level& level, int typeId);
	using Monster::playSound;

	bool	  isCharging() const { return _vexFlags & FLAG_IS_CHARGING; }
	void	  setIsCharging(bool charging);
	void	  setOwner(Mob* owner);
	Mob*	  ownerMob();
	Actor*	  getOwnerEntity() override { return ownerMob(); }
	void	  setBoundOrigin(const BlockPos& pos) { _boundOrigin = pos; _hasBoundOrigin = true; }
	bool	  hasBoundOrigin() const { return _hasBoundOrigin; }
	const BlockPos& boundOrigin() const { return _boundOrigin; }
	void	  setLimitedLife(int ticks) { _hasLimitedLife = true; _limitedLifeTicks = ticks; }

	void							registerGoals() override;
	void							tick() override;
	bool							onClimbable() override { return false; }
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return "minecraft:entity.vex.ambient"; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void travel(const Vec3& input) override { travelFlying(input, 0.02F); }
	void checkFallDamage(double, bool, const BlockPos&) override {}
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t	  _vexFlags = 0;
	EntityRef _owner;
	BlockPos  _boundOrigin;
	bool	  _hasBoundOrigin = false, _hasLimitedLife = false;
	int		  _limitedLifeTicks = 0;
};

// Ravager: a big beast of the raids. Bites (a 10 tick animation), breaks the leaves it runs into, and roars (pushing
// everything around away) after being stunned by a shield
class Ravager : public Raider {
  public:
	static constexpr int EVENT_ATTACK = 4, EVENT_STUNNED = 39;
	static constexpr int STUN_DURATION = 40, ROAR_DURATION = 20;

	Ravager(Level& level, int typeId);
	int attackTick() const { return _attackTick; }
	int stunnedTick() const { return _stunnedTick; }
	int roarTick() const { return _roarTick; }
	// Ravager.blockedByItem: its bite was blocked (by a shield): stunned, then it roars
	void blockedByShield();

	void		registerGoals() override;
	bool		doHurtTarget(Actor& target) override;
	bool		isImmobile() const override;
	int			maxHeadYRot() const override { return 45; }
	std::string ambientSoundName() override { return "minecraft:entity.ravager.ambient"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;

  protected:
	void aiStep() override;

  private:
	int _attackTick = 0, _stunnedTick = 0, _roarTick = 0;

	void roar();
};

// EvokerFangs: rise from the ground after a warmup, bite (6 magic damage) what stands in them, and sink back
class EvokerFangs : public Entity {
  public:
	EvokerFangs(Level& level, double x, double y, double z, float yRot, int warmupDelay, Actor* owner);
	explicit EvokerFangs(Level& level) : EvokerFangs(level, 0, 0, 0, 0, 0, nullptr) {}

	Actor* owner() const override;
	void   tick() override;

  private:
	int		  _warmupDelayTicks, _lifeTicks = 22;
	bool	  _sentSpikeEvent = false;
	EntityRef _owner;

	void dealDamageTo(Actor& target);
};

#endif
