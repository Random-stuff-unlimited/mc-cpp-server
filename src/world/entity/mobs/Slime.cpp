#include "world/entity/mobs/Slime.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	// SlimeMoveControl: turns toward the wanted yaw, then jumps every so often (nothing between the jumps)
	class SlimeMoveControl : public MoveControl {
	  public:
		explicit SlimeMoveControl(Slime& slime) : MoveControl(slime), _slime(slime) {}
		void tick() override {
			_slime.setYRotPublic(rotlerp(_slime.yRot(), _slime.targetYaw(), 90.0F));
			if (--_jumpTicks <= 0) {
				_jumpTicks = 20 + _slime.random().nextInt(20); // getRandomJumpTicks
				_slime.setJumping(true);
				_slime.level().playSoundAt(nullptr, _slime.position().x, _slime.position().y, _slime.position().z, "minecraft:entity.slime.jump",
										   Level::SoundSource::Hostile, 1.0F, 1.0F);
			} else {
				_slime.setJumping(false);
				_slime.setZza(0.0F);
			}
		}

	  private:
		Slime& _slime;
		int	   _jumpTicks = 20;
	};
	// RandomSlimeJumpGoal: idle slimes hop around in a random direction
	class RandomSlimeJumpGoal : public Goal {
	  public:
		explicit RandomSlimeJumpGoal(Slime& slime) : _slime(slime) {
			setFlags({Flag::Move, Flag::Look});
		}
		bool canUse() override { return _slime.getTarget() == nullptr; }
		void tick() override {
			if (--_ticks <= 0) {
				_ticks = 20 + _slime.random().nextInt(40);
				_slime.setTargetYaw(_slime.random().nextFloat() * 360.0F);
				_slime.setZza(0.6F);
			}
		}

	  private:
		Slime& _slime;
		int	   _ticks = 0;
	};
	// SlimeAttackGoal: hop toward the target and squash it on contact
	class SlimeAttackGoal : public Goal {
	  public:
		explicit SlimeAttackGoal(Slime& slime) : _slime(slime) {
			setFlags({Flag::Move, Flag::Look});
		}
		bool canUse() override { return _slime.getTarget() && _slime.getTarget()->isAlive(); }
		bool canContinueToUse() override { return canUse(); }
		void tick() override {
			Actor* target = _slime.getTarget();
			if (!target) return;
			_slime.lookControl().setLookAt(*target, 10.0F, 10.0F);
			// Face it and hop toward it
			double dx = target->position().x - _slime.position().x;
			double dz = target->position().z - _slime.position().z;
			_slime.setTargetYaw(static_cast<float>(std::atan2(dz, dx) * 180.0 / M_PI) - 90.0F);
			_slime.setZza(1.0F);
			if (_slime.distanceToSqr(*target) < 2.5 && --_attackTicks <= 0) {
				_attackTicks = 20;
				_slime.dealContactDamage(*target);
			}
		}

	  private:
		Slime& _slime;
		int	   _attackTicks = 0;
	};
} // namespace

Slime::Slime(Level& level, int typeId) : Mob(level, typeId) {
	setMoveControl(std::make_unique<SlimeMoveControl>(*this));
}

bool Slime::fireImmune() const {
	return _level.gameData().getStaticName("minecraft:entity_type", _typeId) == "minecraft:magma_cube";
}

void Slime::setSize(int size) {
	_size = size;
	markData(DATA_SIZE);
	_width = _height = 0.51F * size; // Entity.getDimensions
	if (AttributeInstance* health = attributes().getInstance(attributeIds().maxHealth)) {
		health->setBaseValue(size * size); // MAX_HEALTH = size^2
		setHealth(static_cast<float>(health->baseValue()));
	}
}

void Slime::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<SlimeAttackGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<RandomSlimeJumpGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(5, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

void Slime::tick() {
	Mob::tick();
	if (isRemoved()) return;
	// The squish: the squash sound when it lands (a jump onto the ground)
	if (_onGround && _horizontalCollision) {
		_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.slime.squish", Level::SoundSource::Hostile, 1.0F,
						   (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
	}
}

void Slime::dealContactDamage(Actor& target) {
	// Slime.dealDamage: the size as damage, from a mob attack
	float amount = static_cast<float>(_size);
	if (auto* player = target.asPlayer()) {
		Combat::damage(_level.server(), *player, amount, {"minecraft:mob_attack", this, nullptr, std::nullopt});
	} else if (auto* living = target.asLiving()) {
		living->hurtServer({"minecraft:mob_attack", this, nullptr, std::nullopt}, amount);
	}
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.slime.attack", Level::SoundSource::Hostile, 1.0F, 1.0F);
}

void Slime::die(const Combat::DamageSource& source) {
	// Slime.die: a bigger one splits into two of half its size
	int split = _size / 2;
	if (split >= 1) {
		for (int i = 0; i < 2; i++) {
			double ox = _random.nextInt(2) * 2 - 1; // -1 or 1
			double oz = _random.nextInt(2) * 2 - 1;
			std::unique_ptr<Mob> child = _level.mobs().create(_level, _typeId);
			if (auto* slime = dynamic_cast<Slime*>(child.get())) {
				slime->setSize(split);
				slime->snapTo({_position.x + ox * 0.5, _position.y, _position.z + oz * 0.5}, _yRot, _xRot);
				_level.entities().add(std::move(child));
			}
		}
	}
	LivingEntity::die(source);
}

void Slime::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SIZE, onlyNonDefault, _size == 1)) writeIntData(buf, DATA_SIZE, _size);
}

void Slime::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeVarInt(_size);
}

void Slime::load(Buffer& buf) {
	Mob::load(buf);
	setSize(buf.readVarInt());
}