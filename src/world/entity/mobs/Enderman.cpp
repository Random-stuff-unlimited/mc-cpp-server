#include "world/entity/mobs/Enderman.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

namespace {
	// The enderman attacks only once provoked (EndermanEndermanLookAtPlayerGoal / EndermanMeleeAttackGoal)
	class EndermanMeleeAttackGoal : public MeleeAttackGoal {
	  public:
		EndermanMeleeAttackGoal(Enderman& enderman) : MeleeAttackGoal(enderman, 1.0, false), _enderman(enderman) {}
		bool canUse() override { return _enderman.isScreaming() && MeleeAttackGoal::canUse(); }
		bool canContinueToUse() override { return _enderman.isScreaming() && MeleeAttackGoal::canContinueToUse(); }

	  private:
		Enderman& _enderman;
	};
	// A player looking at the enderman provokes it (EndermanLookAtPlayerGoal): the ray from its eyes to its look,
	// within 64 blocks, hits the enderman
	class EndermanLookAtPlayerGoal : public Goal {
	  public:
		explicit EndermanLookAtPlayerGoal(Enderman& enderman) : _enderman(enderman) {
			setFlags({Flag::Look, Flag::Move});
		}
		bool canUse() override {
			// Level.getNearestPlayer(64): the closest player within 64 blocks
			Player* nearest = nullptr;
			double	best	= 64.0 * 64.0;
			for (const auto& other : _enderman.level().players()) {
				if (other->isSpectator() || other->combat().dead) continue;
				double d = _enderman.distanceToSqr(*other);
				if (d < best) {
					best	= d;
					nearest = other.get();
				}
			}
			_owner = nearest;
			return _owner != nullptr;
		}
		bool canContinueToUse() override { return _owner != nullptr && _enderman.distanceToSqr(*_owner) < 64.0 * 64.0; }
		void stop() override { _owner = nullptr; }
		void tick() override {
			if (!_owner) return;
			_enderman.lookControl().setLookAt(*_owner, 30.0F, 30.0F);
			if (_enderman.isScreaming()) return;
			// The player's look direction, and the ray to the enderman
			float yaw	 = _owner->getYaw() * static_cast<float>(M_PI) / 180.0F;
			float pitch	 = _owner->getPitch() * static_cast<float>(M_PI) / 180.0F;
			Vec3  look{-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
			Vec3  eye	 = _owner->eyePosition();
			if (Clip::clipBox(_enderman.boundingBox(), eye, eye + look.scale(64.0))) {
				_enderman.setScreaming(true);
				_enderman.setTarget(_owner);
			}
		}

	  private:
		Enderman& _enderman;
		Player*	  _owner = nullptr;
	};
} // namespace

void Enderman::setScreaming(bool screaming) {
	if (_screaming == screaming) return;
	_screaming = screaming;
	markData(DATA_SCREAMING);
}

void Enderman::setTarget(Actor* target) {
	Monster::setTarget(target);
	setScreaming(target != nullptr);
}

bool Enderman::teleportRandomly() {
	// Enderman.teleportRandomly: a random spot up to 32 blocks away, scanning down onto a solid block
	double x = _position.x + (_random.nextDouble() - 0.5) * 64.0;
	double y = _position.y + (_random.nextInt(64) - 32);
	double z = _position.z + (_random.nextDouble() - 0.5) * 64.0;
	BlockPos pos{Mth::floor(x), Mth::floor(y), Mth::floor(z)};
	while (pos.y > _level.minY() && !_level.gameData().getStateProperties(_level.getBlockState(pos)).blocksMotion) pos.y--;
	if (!_level.gameData().getStateProperties(_level.getBlockState(pos)).blocksMotion) return false;
	snapTo({pos.x + 0.5, pos.y + 1.0, pos.z + 0.5}, _yRot, _xRot);
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.enderman.teleport", Level::SoundSource::Hostile, 1.0F, 1.0F);
	_level.playSoundAt(nullptr, pos.x + 0.5, pos.y + 1.0, pos.z + 0.5, "minecraft:entity.enderman.teleport", Level::SoundSource::Hostile, 1.0F, 1.0F);
	return true;
}

void Enderman::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<EndermanMeleeAttackGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<EndermanLookAtPlayerGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
}

void Enderman::tick() {
	// Enderman.tick: rain or water hurts it and it teleports away; calms down when the target is gone
	Monster::tick();
	if (isRemoved()) return;
	if ((_level.isRaining() && _level.canSeeSky(blockPosition())) || isInWater()) {
		hurtServer({"minecraft:drown", nullptr, nullptr, std::nullopt}, 1.0F);
		if (!isRemoved() && _random.nextInt(3) == 0) teleportRandomly();
	}
	if (_screaming && !getTarget()) setScreaming(false);
}

bool Enderman::hurtServer(const Combat::DamageSource& source, float amount) {
	bool result = Monster::hurtServer(source, amount);
	// Enderman.hurt: teleport away, and scream
	if (result && !isRemoved() && _teleportDelay-- <= 0) {
		_teleportDelay = 20;
		teleportRandomly();
	}
	return result;
}

void Enderman::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SCREAMING, onlyNonDefault, !_screaming)) writeBoolData(buf, DATA_SCREAMING, _screaming);
}

void Enderman::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeBool(_screaming);
}

void Enderman::load(Buffer& buf) {
	Monster::load(buf);
	setScreaming(buf.readBool());
}