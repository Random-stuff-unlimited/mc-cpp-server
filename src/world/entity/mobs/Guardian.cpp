#include "world/entity/mobs/Guardian.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	constexpr int GAME_EVENT_GUARDIAN_ELDER_EFFECT = 10; // ClientboundGameEventPacket.GUARDIAN_ELDER_EFFECT

	// Guardian.GuardianMoveControl: swims toward the wanted position, wobbling, looking ahead
	class GuardianMoveControl : public MoveControl {
	  public:
		explicit GuardianMoveControl(Guardian& guardian) : MoveControl(guardian), _guardian(guardian) {}
		void tick() override {
			if (_operation != Operation::MoveTo || _guardian.navigation().isDone()) {
				_guardian.setSpeed(0.0F);
				_guardian.setMoving(false);
				return;
			}
			const Vec3& p = _guardian.position();
			Vec3		to{_wantedX - p.x, _wantedY - p.y, _wantedZ - p.z};
			double		length = to.length();
			double		nx = to.x / length, ny = to.y / length, nz = to.z / length;
			float		yaw = static_cast<float>(Mth::atan2(to.z, to.x) * 180.0F / (float)M_PI) - 90.0F;
			_guardian.setYRotPublic(rotlerp(_guardian.yRot(), yaw, 90.0F));
			_guardian.setYBodyRot(_guardian.yRot());
			float wanted = static_cast<float>(_speedModifier * _guardian.getAttributeValue(_guardian.attributeIds().movementSpeed));
			float speed	 = Mth::lerp(0.125F, _guardian.speed(), wanted);
			_guardian.setSpeed(speed);
			int	   phase = _guardian.tickCount() + _guardian.id();
			double sway	 = std::sin(phase * 0.5) * 0.05;
			double cos	 = std::cos(_guardian.yRot() * static_cast<float>(M_PI / 180.0));
			double sin	 = std::sin(_guardian.yRot() * static_cast<float>(M_PI / 180.0));
			double bob	 = std::sin(phase * 0.75) * 0.05;
			_guardian.setDeltaMovement(_guardian.deltaMovement() + Vec3{sway * cos, bob * (sin + cos) * 0.25 + speed * ny * 0.1, sway * sin});
			LookControl& look = _guardian.lookControl();
			double		 lx = p.x + nx * 2.0, ly = _guardian.eyeY() + ny / length, lz = p.z + nz * 2.0;
			double		 wx = look.getWantedX(), wy = look.getWantedY(), wz = look.getWantedZ();
			if (!look.isLookingAtTarget()) {
				wx = lx;
				wy = ly;
				wz = lz;
			}
			look.setLookAt(Mth::lerp(0.125, wx, lx), Mth::lerp(0.125, wy, ly), Mth::lerp(0.125, wz, lz), 10.0F, 40.0F);
			_guardian.setMoving(true);
		}

	  private:
		Guardian& _guardian;
	};

	// Guardian.GuardianAttackGoal: stops, locks its beam on its target, and hits when it is charged
	class GuardianAttackGoal : public Goal {
	  public:
		explicit GuardianAttackGoal(Guardian& guardian) : _guardian(guardian), _elder(guardian.isElder()) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			Actor* target = _guardian.getTarget();
			return target && target->isAlive();
		}
		bool canContinueToUse() override {
			return canUse() && (_elder || (_guardian.getTarget() && _guardian.distanceToSqr(*_guardian.getTarget()) > 9.0));
		}
		void start() override {
			_attackTime = -10;
			_guardian.navigation().stop();
			if (Actor* target = _guardian.getTarget()) _guardian.lookControl().setLookAt(*target, 90.0F, 90.0F);
			_guardian.hasImpulse = true;
		}
		void stop() override {
			_guardian.setActiveAttackTarget(0);
			_guardian.setTarget(nullptr);
			_guardian.triggerStroll();
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _guardian.getTarget();
			if (!target) return;
			_guardian.navigation().stop();
			_guardian.lookControl().setLookAt(*target, 90.0F, 90.0F);
			if (!_guardian.hasLineOfSight(*target)) {
				_guardian.setTarget(nullptr);
				return;
			}
			_attackTime++;
			if (_attackTime == 0) {
				_guardian.setActiveAttackTarget(target->id());
				_guardian.broadcastEntityEvent(Guardian::EVENT_ATTACK_SOUND);
			} else if (_attackTime >= _guardian.attackDuration()) {
				float damage = 1.0F;
				if (_guardian.level().difficulty() == 3) damage += 2.0F;
				if (_elder) damage += 2.0F;
				target->hurtServer({"minecraft:indirect_magic", &_guardian, &_guardian, std::nullopt}, damage);
				_guardian.doHurtTarget(*target);
				_guardian.setTarget(nullptr);
			}
		}

	  private:
		Guardian& _guardian;
		int		  _attackTime = 0;
		bool	  _elder;
	};
} // namespace

Guardian::Guardian(Level& level, int typeId) : Monster(level, typeId) {
	_xpReward = 10;
	setPathfindingMalus(PathType::Water, 0.0F);
	setMoveControl(std::make_unique<GuardianMoveControl>(*this));
	_random.nextFloat(); // clientSideTailAnimation
}

std::unique_ptr<PathNavigation> Guardian::createNavigation() { return std::make_unique<WaterBoundPathNavigation>(*this, _level); }

void Guardian::registerGoals() {
	auto restriction  = std::make_unique<MoveTowardsRestrictionGoal>(*this, 1.0);
	_randomStrollGoal = std::make_shared<RandomStrollGoal>(*this, 1.0, 80);
	_randomStrollGoal->setFlags({Goal::Flag::Move, Goal::Flag::Look});
	restriction->setFlags({Goal::Flag::Move, Goal::Flag::Look});
	_goalSelector.addGoal(4, std::make_unique<GuardianAttackGoal>(*this));
	_goalSelector.addGoal(5, std::move(restriction));
	_goalSelector.addSharedGoal(7, _randomStrollGoal);
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Guardian", 12.0F, 0.01F));
	_goalSelector.addGoal(9, std::make_unique<RandomLookAroundGoal>(*this));
	// GuardianAttackSelector: players, squids and axolotls further than 3 blocks
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "LivingEntity", 10, true, false, [this](Actor& target, Level&) {
		return (target.asPlayer() || isOfClass(target, "Squid") || isOfClass(target, "Axolotl")) && target.distanceToSqr(*this) > 9.0;
	}));
}

void Guardian::triggerStroll() {
	if (_randomStrollGoal) _randomStrollGoal->trigger();
}

void Guardian::setMoving(bool moving) {
	if (moving == _moving) return;
	_moving = moving;
	markData(DATA_MOVING);
}

void Guardian::setActiveAttackTarget(int entityId) {
	if (entityId == _attackTarget) return;
	_attackTarget = entityId;
	markData(DATA_ATTACK_TARGET);
}

float Guardian::getWalkTargetValue(const BlockPos& pos) {
	if (_level.fluids().isWater(_level.fluids().stateOf(_level.getBlockState(pos)).type)) return 10.0F + _level.getPathfindingCostFromLightLevels(pos);
	return Monster::getWalkTargetValue(pos);
}

void Guardian::aiStep() {
	if (isAlive()) {
		if (isInWater()) {
			setAirSupply(300);
		} else if (_onGround) {
			_delta	   = _delta + Vec3{(_random.nextFloat() * 2.0F - 1.0F) * 0.4F, 0.5, (_random.nextFloat() * 2.0F - 1.0F) * 0.4F};
			_yRot	   = _random.nextFloat() * 360.0F;
			_onGround  = false;
			hasImpulse = true;
		}
		if (hasActiveAttackTarget()) _yRot = _yHeadRot;
	}
	Monster::aiStep();
}

bool Guardian::hurtServer(const Combat::DamageSource& source, float amount) {
	// Its spikes, when it isn't swimming
	const GameData& data = _level.gameData();
	int				type = data.getSyncedId("minecraft:damage_type", source.type);
	Actor*			direct = source.directEntity();
	if (!_moving && !data.isInTag("minecraft:damage_type", "minecraft:avoids_guardian_thorns", type) && source.type != "minecraft:thorns" && direct &&
		(direct->asLiving() || direct->asPlayer())) {
		direct->hurtServer({"minecraft:thorns", this, nullptr, std::nullopt}, 2.0F);
	}
	triggerStroll();
	return Monster::hurtServer(source, amount);
}

void Guardian::travel(const Vec3& input) {
	if (!isInWater()) {
		Monster::travel(input);
		return;
	}
	moveRelative(0.1F, input);
	move(_delta);
	_delta = _delta.scale(0.9);
	if (!_moving && !getTarget()) _delta.y -= 0.005;
}

void Guardian::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_MOVING, onlyNonDefault, !_moving)) writeBoolData(buf, DATA_MOVING, _moving);
	if (wantsData(mask, DATA_ATTACK_TARGET, onlyNonDefault, _attackTarget == 0)) writeIntData(buf, DATA_ATTACK_TARGET, _attackTarget);
}

// ===================== ElderGuardian =====================

void ElderGuardian::registerGoals() {
	Guardian::registerGoals();
	_randomStrollGoal->setInterval(400);
}

void ElderGuardian::customServerAiStep() {
	Guardian::customServerAiStep();
	if ((_tickCount + _id) % 1200 == 0) {
		// MobEffectUtil.addEffectToPlayersAround: the survival players within 50 blocks that don't have it as strong
		// for another minute
		for (const auto& player : _level.players()) {
			if (player->isDisconnected() || player->isSpectator() || player->isCreative() || player->combat().dead) continue;
			Vec3 delta = player->position() - _position;
			if (delta.lengthSqr() > 50.0 * 50.0) continue;
			const MobEffects::Instance* current = player->effects().get(MobEffects::MiningFatigue);
			if (current && current->amplifier >= 2 && current->duration >= 1200) continue;
			player->addEffect(MobEffects::Instance(MobEffects::MiningFatigue, 6000, 2), this);
			Buffer event;
			event.writeUByte(GAME_EVENT_GUARDIAN_ELDER_EFFECT);
			event.writeFloat(1.0F);
			Packet::send(player, PacketId::Play::Clientbound::GAME_EVENT, event, _level.server());
		}
	}
	if (!hasHome()) setHomeTo(blockPosition(), 16);
}
