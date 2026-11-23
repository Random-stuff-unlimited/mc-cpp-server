#ifndef BOSSES_HPP
#define BOSSES_HPP

#include "lib/UUID.hpp"
#include "world/BlockBehavior.hpp"
#include "world/entity/Fireballs.hpp"
#include "world/entity/mobs/Monster.hpp"

#include <array>
#include <set>
#include <string>

// ServerBossEvent: a boss bar (BOSS_EVENT) shown to the players near its boss
class BossBar {
  public:
	// BossEvent.BossBarColor / BossBarOverlay, and its flags (darken screen 1, boss music 2, world fog 4)
	enum Color { Pink = 0, Blue = 1, Red = 2, Green = 3, Yellow = 4, Purple = 5, White = 6 };

	BossBar(Level& level, std::string titleKey, Color color, uint8_t flags);

	void  setProgress(float progress) { _progress = std::max(0.0F, std::min(1.0F, progress)); }
	float progress() const { return _progress; }
	// The players within range see it (they join and leave), and its progress if it changed
	void  update(const Vec3& center, double range);
	// Gone for everyone (the boss died)
	void  removeAll();

  private:
	Level&		  _level;
	UUID		  _uuid;
	std::string	  _titleKey;
	Color		  _color;
	uint8_t		  _flags;
	float		  _progress = 1.0F, _sentProgress = 1.0F;
	std::set<int> _players; // Ids of the players that see it

	void sendAdd(Player& player);
	void sendRemove(Player& player);
};

// WitherBoss: summoned with a T of soul sand and three wither skeleton skulls, it charges up for 11 seconds (it
// can't be hurt, its health grows) and explodes, then flies above its targets — anything living but the undead —
// and shoots wither skulls from its three heads (now and then a slow blue one that breaks blocks). Hurt, it breaks the
// blocks around it; under half its health its armor stops arrows. A bar shows its health
class WitherBoss : public Monster {
  public:
	static constexpr int DATA_TARGET_A = 16, DATA_TARGET_B = 17, DATA_TARGET_C = 18, DATA_ID_INV = 19;
	static constexpr int INVULNERABLE_TICKS = 220;

	WitherBoss(Level& level, int typeId);
	~WitherBoss() override;

	int	 invulnerableTicks() const { return _invulnerableTicks; }
	void setInvulnerableTicks(int ticks);
	// WitherBoss.makeInvulnerable: the charge up of a freshly summoned wither
	void makeInvulnerable();
	bool isPowered() const { return _health <= maxHealth() / 2.0F; }
	int	 alternativeTarget(int head) const { return _targets[head]; }
	void setAlternativeTarget(int head, int id);
	// WitherBoss.canAttack's LIVING_ENTITY_SELECTOR: alive, not one of the #wither_friends (the undead)
	bool isValidTarget(Actor& actor);
	Vec3 headPosition(int head) const;
	void performRangedAttack(Actor& target, float power) override;
	void performHeadAttack(int head, const Vec3& target, bool dangerous);

	void		registerGoals() override;
	bool		hurtServer(const Combat::DamageSource& source, float amount) override;
	bool		removeWhenFarAway(double) const override { return false; }
	bool		onClimbable() override { return false; }
	void		checkDespawn() override;
	std::string ambientSoundName() override { return "minecraft:entity.wither.ambient"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;
	using Monster::playSound;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							aiStep() override;
	void							customServerAiStep() override;
	void							travel(const Vec3& input) override { travelFlying(input, 0.02F); }
	void							checkFallDamage(double, bool, const BlockPos&) override {}
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	std::array<int, 3>		_targets{};
	std::array<int, 2>		_nextHeadUpdate{}, _idleHeadUpdates{};
	int						_invulnerableTicks = 0, _destroyBlocksTick = 0;
	BossBar					_bossBar;
};

// WitherSkull: 8 damage and withering (10 seconds, 40 in hard) to what it hits, then a small explosion. The blue ones
// fly slower
class WitherSkull : public HurtingProjectile {
  public:
	static constexpr int DATA_DANGEROUS = 8;

	WitherSkull(Level& level, LivingEntity* shooter, const Vec3& direction, bool dangerous);
	explicit WitherSkull(Level& level) : WitherSkull(level, nullptr, Vec3{0, 0, 1}, false) {}
	bool isDangerous() const { return _dangerous; }
	void writeEntityData(Buffer& buf) const override;

  protected:
	float inertia() const override { return _dangerous ? 0.73F : HurtingProjectile::inertia(); }
	bool  shouldBurn() const override { return false; }
	void  onHit(const Hit& hit) override;

  private:
	bool _dangerous;
};

// DragonFireball: what the dragon spits while strafing: it bursts into dragon's breath (6 magic damage around, here
// at once instead of a lingering cloud)
class DragonFireball : public HurtingProjectile {
  public:
	DragonFireball(Level& level, LivingEntity* shooter, const Vec3& direction);
	explicit DragonFireball(Level& level) : DragonFireball(level, nullptr, Vec3{0, 0, 1}) {}
	void writeEntityData(Buffer&) const override {}

  protected:
	bool shouldBurn() const override { return false; }
	void onHit(const Hit& hit) override;
};

// EnderDragon: flies circles around its fight's center (where it was summoned), charges at a player or strafes one
// with fireballs, lands on the center now and then and takes off again; its head hurts and its wings throw what they
// touch. Dying, it rises for 10 seconds, giving its experience. Its 8 parts are client side (their ids follow its own,
// attacks on them hit the dragon). End crystals, the exit portal, the egg and the End's own fight aren't ported
class EnderDragon : public Mob {
  public:
	static constexpr int DATA_PHASE = 16, PARTS = 8;
	// EnderDragonPhase ids
	enum class Phase { HoldingPattern = 0, StrafePlayer = 1, LandingApproach = 2, Landing = 3, Takeoff = 4, SittingFlaming = 5, SittingScanning = 6,
					   SittingAttacking = 7, ChargingPlayer = 8, Dying = 9, Hovering = 10 };

	EnderDragon(Level& level, int typeId);
	~EnderDragon() override;

	Phase phase() const { return _phase; }
	void  setPhase(Phase phase);
	// The end crystal healing it (EnderDragon.nearestCrystal), its id, -1 if none
	int	  healingCrystalId() const { return _healingCrystal; }
	const Vec3& fightCenter() const { return _center; }

	int								partCount() const override { return PARTS; }
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							removeWhenFarAway(double) const override { return false; }
	bool							isPushable() const override { return false; }
	bool							onClimbable() override { return false; }
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return "minecraft:entity.ender_dragon.ambient"; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;
	using Mob::playSound;

  protected:
	void aiStep() override;
	void checkFallDamage(double, bool, const BlockPos&) override {}
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	float soundVolume() const override { return 5.0F; }

  private:
	Phase	  _phase = Phase::HoldingPattern;
	Vec3	  _center, _target;
	double	  _podiumY = 0.0;
	float	  _yRotA   = 0.0F;
	int		  _phaseTicks = 0, _circleStep = 0, _fireballCooldown = 0, _dyingTicks = 0, _healingCrystal = -1;
	EntityRef _attackTarget;
	BossBar	  _bossBar;

	void tickPhase();
	void flyToTarget(float flySpeed, float turnSpeed);
	void hurtAndKnockBack();
	void chooseNextPhase();
	Vec3 headPosition() const;
	void findPodium();
	// EnderDragon.checkCrystals: the nearest crystal within 32 blocks heals it a point every half second
	void checkCrystals();
};

// WitherSkullBlock.checkSpawn: the third wither skeleton skull on a T of soul sand or soul soil summons a wither
class WitherSkullBehavior : public BlockBehavior {
  public:
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
};

#endif
