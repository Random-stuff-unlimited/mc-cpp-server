#include "world/entity/mobs/TrialMobs.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	// AbstractWindCharge.EXPLOSION_DAMAGE_CALCULATOR: a burst that only pushes (1.22 times as hard), blocks untouched
	class WindChargeCalculator : public Explosions::Calculator {
	  public:
		bool  shouldBlockExplode(Level&, const BlockPos&, int, float) const override { return false; }
		bool  shouldDamageEntity(Actor&) const override { return false; }
		float knockbackMultiplier(Actor&) const override { return 1.22F; }
	};
	const WindChargeCalculator WIND_CHARGE_CALCULATOR;

	bool hurtActor(Level& level, Actor& target, const Combat::DamageSource& source, float amount) {
		if (Player* player = target.asPlayer()) return Combat::damage(level.server(), *player, amount, source);
		return target.hurtServer(source, amount);
	}

	// The breeze's fight (BreezeAi: Shoot, LongJump, Slide): it keeps its distance, shoots a wind charge every second
	// or so, and long-jumps to somewhere near its target when it is too close, unseen or now and then after a shot
	class BreezeAttackGoal : public Goal {
	  public:
		explicit BreezeAttackGoal(Breeze& breeze) : _breeze(breeze) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override {
			Actor* target = _breeze.getTarget();
			return target && target->isAlive() && _breeze.canAttack(*target);
		}
		bool canContinueToUse() override { return _phase != Phase::Idle || canUse(); }
		void start() override { _phase = Phase::Idle; }
		void stop() override {
			_phase = Phase::Idle;
			_breeze.setPose(LivingEntity::POSE_STANDING);
			_breeze.navigation().stop();
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _breeze.getTarget();
			if (_shootCooldown > 0) _shootCooldown--;
			if (_jumpCooldown > 0) _jumpCooldown--;
			_ticks++;
			switch (_phase) {
				case Phase::Idle: {
					if (!target) return;
					_breeze.lookControl().setLookAt(*target, 30.0F, 30.0F);
					bool   visible = _breeze.sensing().hasLineOfSight(*target);
					double dist	   = _breeze.distanceToSqr(*target);
					_unseen		   = visible ? 0 : _unseen + 1;
					if (_breeze.onGround() && _jumpCooldown <= 0 && (dist < 4.0 * 4.0 || _unseen > 40 || _jumpAfterShot)) {
						startPhase(Phase::Inhaling, Breeze::POSE_INHALING);
						_breeze.playSound("minecraft:entity.breeze.inhale", 1.0F, 1.0F);
						_breeze.navigation().stop();
						_jumpAfterShot = false;
						return;
					}
					if (visible && dist <= 24.0 * 24.0 && _shootCooldown <= 0) {
						startPhase(Phase::Shooting, Breeze::POSE_SHOOTING);
						_breeze.playSound("minecraft:entity.breeze.charge", 1.0F, 1.0F);
						_breeze.navigation().stop();
						return;
					}
					if (!visible || dist > 16.0 * 16.0) {
						if (_breeze.navigation().isDone()) _breeze.navigation().moveTo(*target, 1.0);
					}
					break;
				}
				case Phase::Shooting:
					if (target) _breeze.lookControl().setLookAt(*target, 30.0F, 30.0F);
					if (_ticks == 5 && target) shoot(*target); // Breeze.Shoot: SHOOT_INITIAL_DELAY
					if (_ticks >= 9) {
						startPhase(Phase::Idle, LivingEntity::POSE_STANDING);
						_shootCooldown = 20 + _breeze.random().nextInt(20);
						_jumpAfterShot = _breeze.random().nextInt(3) == 0;
					}
					break;
				case Phase::Inhaling:
					if (_ticks >= 20) {
						if (!target || !_breeze.onGround()) {
							startPhase(Phase::Idle, LivingEntity::POSE_STANDING);
							return;
						}
						jumpNear(*target);
						startPhase(Phase::Jumping, Breeze::POSE_LONG_JUMPING);
						_breeze.playSound("minecraft:entity.breeze.jump", 1.0F, 1.0F);
					}
					break;
				case Phase::Jumping:
					if (_ticks > 5 && _breeze.onGround()) {
						startPhase(Phase::Idle, LivingEntity::POSE_STANDING);
						_breeze.playSound("minecraft:entity.breeze.land", 1.0F, 1.0F);
						_jumpCooldown = 40 + _breeze.random().nextInt(60);
					}
					break;
			}
		}

	  private:
		enum class Phase { Idle, Shooting, Inhaling, Jumping };
		Breeze& _breeze;
		Phase	_phase = Phase::Idle;
		int		_ticks = 0, _shootCooldown = 0, _jumpCooldown = 0, _unseen = 0;
		bool	_jumpAfterShot = false;

		void startPhase(Phase phase, int pose) {
			_phase = phase;
			_ticks = 0;
			_breeze.setPose(pose);
		}

		void shoot(Actor& target) {
			// Breeze.Shoot: a wind charge from its snout toward the target's middle
			auto   charge = std::make_unique<BreezeWindCharge>(_breeze.level(), &_breeze);
			AABB   box	  = target.boundingBox();
			double dx	  = target.position().x - charge->position().x;
			double dy	  = target.position().y + (box.maxY - box.minY) * 0.5 - charge->position().y;
			double dz	  = target.position().z - charge->position().z;
			charge->shoot(dx, dy, dz, 0.7F, static_cast<float>(5 - _breeze.level().difficulty() * 4 > 0 ? 5 - _breeze.level().difficulty() * 4 : 0));
			_breeze.level().entities().add(std::move(charge));
			_breeze.playSound("minecraft:entity.breeze.shoot", 1.5F, 1.0F);
		}

		// Breeze.LongJump: to a spot 4 to 8 blocks from the target (on its side), in a high arc
		void jumpNear(Actor& target) {
			JavaRandom& random = _breeze.random();
			float		angle  = random.nextFloat() * (float)M_PI * 2.0F;
			double		radius = 4.0 + random.nextDouble() * 4.0;
			Vec3		spot{target.position().x + Mth::cos(angle) * radius, target.position().y, target.position().z + Mth::sin(angle) * radius};
			Vec3		to	 = spot - _breeze.position();
			double		dist = std::sqrt(to.horizontalDistanceSqr());
			Vec3		flat = Vec3{to.x, 0.0, to.z}.normalize().scale(std::min(1.2, dist * 0.12));
			_breeze.setDeltaMovement({flat.x, 0.8 + std::max(0.0, to.y) * 0.1, flat.z});
			_breeze.hasImpulse = true;
		}
	};

	// Creaking: frozen while looked at (every control taken, nothing moves)
	class CreakingFrozenGoal : public Goal {
	  public:
		explicit CreakingFrozenGoal(Creaking& creaking) : _creaking(creaking) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return !_creaking.canMove(); }
		bool isInterruptable() override { return false; }
		void start() override { _creaking.navigation().stop(); }

	  private:
		Creaking& _creaking;
	};
} // namespace

// ===================== Breeze =====================

Breeze::Breeze(Level& level, int typeId) : Monster(level, typeId) {
	setPathfindingMalus(PathType::DangerTrapdoor, -1.0F);
	setPathfindingMalus(PathType::DamageFire, -1.0F);
	_xpReward = 10;
}

void Breeze::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<BreezeAttackGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<RandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

bool Breeze::hurtServer(const Combat::DamageSource& source, float amount) {
	// Projectiles bounce off it (ProjectileDeflection), but for tridents and wind charges
	const GameData& data	   = _level.gameData();
	int				damageType = data.getSyncedId("minecraft:damage_type", source.type);
	if (data.isInTag("minecraft:damage_type", "minecraft:is_projectile", damageType)) {
		Actor*		direct = source.directEntity();
		std::string name   = direct ? data.getStaticName("minecraft:entity_type", direct->typeId()) : "";
		if (name != "minecraft:trident" && name != "minecraft:wind_charge" && name != "minecraft:breeze_wind_charge") return false;
	}
	return Monster::hurtServer(source, amount);
}

// ===================== BreezeWindCharge =====================

BreezeWindCharge::BreezeWindCharge(Level& level, Breeze* breeze) : HurtingProjectile(level, "minecraft:breeze_wind_charge", 0.3125F, breeze, Vec3{}) {
	accelerationPower = 0.0;
	if (breeze) {
		setPos({breeze->position().x, breeze->snoutY(), breeze->position().z});
		_oldPosition = _position;
	}
}

void BreezeWindCharge::onHit(const Hit& hit) {
	if (hit.actor && hit.actor != owner()) {
		Actor* by = owner();
		hurtActor(_level, *hit.actor, {"minecraft:wind_charge", by, this, std::nullopt}, 1.0F);
	}
	explode(hit.location);
	discard();
}

void BreezeWindCharge::explode(const Vec3& at) {
	Explosions::explode(_level, this, Combat::DamageSource{"minecraft:wind_charge", owner(), this, std::nullopt}, &WIND_CHARGE_CALCULATOR, at, 3.0F,
						false, Explosions::Interaction::None);
	_level.playSoundAt(nullptr, at.x, at.y, at.z, "minecraft:entity.breeze.wind_burst", Level::SoundSource::Hostile, 1.0F, 1.0F);
}

// ===================== Creaking =====================

Creaking::Creaking(Level& level, int typeId) : Monster(level, typeId) {
	setPathfindingMalus(PathType::DamageOther, 8.0F);
	setPathfindingMalus(PathType::PowderSnow, 8.0F);
	setPathfindingMalus(PathType::Lava, 8.0F);
	setPathfindingMalus(PathType::DamageFire, 0.0F);
	setPathfindingMalus(PathType::DangerFire, 0.0F);
	_xpReward = 0;
}

void Creaking::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<CreakingFrozenGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(5, std::make_unique<RandomStrollGoal>(*this, 0.3));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, false, false, nullptr));
}

void Creaking::setCanMove(bool can) {
	if (can == _canMove) return;
	_canMove = can;
	markData(DATA_CAN_MOVE);
	if (!can) {
		navigation().stop();
		playSound("minecraft:entity.creaking.freeze", 1.0F, 1.0F);
	} else {
		playSound("minecraft:entity.creaking.unfreeze", 1.0F, 1.0F);
	}
}

void Creaking::setActive(bool active) {
	if (active == _active) return;
	_active = active;
	markData(DATA_IS_ACTIVE);
	playSound(active ? "minecraft:entity.creaking.activate" : "minecraft:entity.creaking.deactivate", 1.0F, 1.0F);
}

bool Creaking::checkCanMove() {
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(24.0), this)) {
		Player* player = actor->asPlayer();
		if (!player || !notCreativeOrSpectator(*actor)) continue;
		// LivingEntity.isLookingAtMe(player, 0.5, false, true, its eyes, its middle, its feet)
		const float toRadians = static_cast<float>(M_PI / 180.0);
		float		yaw = player->getYaw() * toRadians, pitch = player->getPitch() * toRadians;
		Vec3		view = Vec3{-Mth::sin(yaw) * Mth::cos(pitch), -Mth::sin(pitch), Mth::cos(yaw) * Mth::cos(pitch)}.normalize();
		Vec3		eyes{player->getX(), player->eyeY(), player->getZ()};
		for (double y : {eyeY(), _position.y + _height * 0.5, _position.y}) {
			Vec3   point{_position.x, y, _position.z};
			Vec3   to		= point - eyes;
			double distance = to.length();
			if (distance < 1.0E-5) return false;
			to		   = to.normalize();
			double dot = view.x * to.x + view.y * to.y + view.z * to.z;
			if (dot > 1.0 - 0.5 / distance && !Clip::clip(_level, eyes, point, Clip::BlockMode::Collider, Clip::FluidMode::None).hit) return false;
		}
	}
	return true;
}

bool Creaking::doHurtTarget(Actor& target) {
	if (!_canMove) return false;
	_attackAnimationRemainingTicks = 15;
	broadcastEntityEvent(EVENT_ATTACK);
	playSound("minecraft:entity.creaking.attack", 1.0F, 1.0F);
	return Monster::doHurtTarget(target);
}

void Creaking::customServerAiStep() {
	if (_attackAnimationRemainingTicks > 0) _attackAnimationRemainingTicks--;
	setCanMove(checkCanMove());
	setActive(getTarget() != nullptr);
	Monster::customServerAiStep();
}

void Creaking::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_CAN_MOVE, onlyNonDefault, _canMove)) writeBoolData(buf, DATA_CAN_MOVE, _canMove);
	if (wantsData(mask, DATA_IS_ACTIVE, onlyNonDefault, !_active)) writeBoolData(buf, DATA_IS_ACTIVE, _active);
}
