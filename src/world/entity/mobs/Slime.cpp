#include "world/entity/mobs/Slime.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 2;

	// Slime.SlimeMoveControl: turned toward a direction; when a goal wants it to move, it jumps every jumpDelay ticks
	// (three times as often when attacking), with speed only while in the air
	class SlimeMoveControl : public MoveControl {
	  public:
		explicit SlimeMoveControl(Slime& slime) : MoveControl(slime), _slime(slime), _yRot(180.0F * slime.yRot() / static_cast<float>(M_PI)) {}
		void setDirection(float yRot, bool aggressive) {
			_yRot		= yRot;
			_aggressive = aggressive;
		}
		void setWantedMovement(double speed) {
			_speedModifier = speed;
			_operation	   = Operation::MoveTo;
		}
		void tick() override {
			_mob.setYRotPublic(rotlerp(_mob.yRot(), _yRot, 90.0F));
			_mob.setYHeadRot(_mob.yRot());
			_mob.setYBodyRot(_mob.yRot());
			if (_operation != Operation::MoveTo) {
				_mob.setZza(0.0F);
				return;
			}
			_operation	= Operation::Wait;
			float speed = static_cast<float>(_speedModifier * _mob.getAttributeValue(_mob.attributeIds().movementSpeed));
			if (!_mob.onGround()) {
				_mob.setSpeed(speed);
				return;
			}
			_mob.setSpeed(speed);
			if (_jumpDelay-- <= 0) {
				_jumpDelay = _slime.jumpDelay();
				if (_aggressive) _jumpDelay /= 3;
				_slime.jumpControl().jump();
				_slime.playJumpSound();
			} else {
				_slime.setXxa(0.0F);
				_slime.setZza(0.0F);
				_mob.setSpeed(0.0F);
			}
		}

	  private:
		Slime& _slime;
		float  _yRot;
		int	   _jumpDelay  = 0;
		bool   _aggressive = false;
	};

	SlimeMoveControl& control(Slime& slime) { return static_cast<SlimeMoveControl&>(slime.moveControl()); }

	class SlimeFloatGoal : public Goal {
	  public:
		explicit SlimeFloatGoal(Slime& slime) : _slime(slime) {
			setFlags({Flag::Jump, Flag::Move});
			slime.navigation().setCanFloat(true);
		}
		bool canUse() override { return _slime.isInWaterNow() || _slime.isInLavaNow(); }
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			if (_slime.random().nextFloat() < 0.8F) _slime.jumpControl().jump();
			control(_slime).setWantedMovement(1.2);
		}

	  private:
		Slime& _slime;
	};

	// Faces its target for 15 seconds, jumping faster if it deals damage
	class SlimeAttackGoal : public Goal {
	  public:
		explicit SlimeAttackGoal(Slime& slime) : _slime(slime) { setFlags({Flag::Look}); }
		bool canUse() override {
			Actor* target = _slime.getTarget();
			return target && _slime.canAttack(*target);
		}
		void start() override { _growTiredTimer = reducedTickDelay(300); }
		bool canContinueToUse() override {
			Actor* target = _slime.getTarget();
			return target && _slime.canAttack(*target) && --_growTiredTimer > 0;
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			if (Actor* target = _slime.getTarget()) _slime.lookAt(*target, 10.0F, 10.0F);
			control(_slime).setDirection(_slime.yRot(), _slime.isDealsDamage());
		}

	  private:
		Slime& _slime;
		int	   _growTiredTimer = 0;
	};

	// A new random direction every 2 to 5 seconds
	class SlimeRandomDirectionGoal : public Goal {
	  public:
		explicit SlimeRandomDirectionGoal(Slime& slime) : _slime(slime) { setFlags({Flag::Look}); }
		bool canUse() override {
			return !_slime.getTarget() && (_slime.onGround() || _slime.isInWaterNow() || _slime.isInLavaNow() || _slime.hasEffect(MobEffects::Levitation));
		}
		void tick() override {
			if (--_nextRandomizeTime <= 0) {
				_nextRandomizeTime = adjustedTickDelay(40 + _slime.random().nextInt(60));
				_chosenDegrees	   = static_cast<float>(_slime.random().nextInt(360));
			}
			control(_slime).setDirection(_chosenDegrees, false);
		}

	  private:
		Slime& _slime;
		float  _chosenDegrees	  = 0;
		int	   _nextRandomizeTime = 0;
	};

	class SlimeKeepOnJumpingGoal : public Goal {
	  public:
		explicit SlimeKeepOnJumpingGoal(Slime& slime) : _slime(slime) { setFlags({Flag::Jump, Flag::Move}); }
		bool canUse() override { return true; }
		void tick() override { control(_slime).setWantedMovement(1.0); }

	  private:
		Slime& _slime;
	};
} // namespace

Slime::Slime(Level& level, int typeId) : Mob(level, typeId) { setMoveControl(std::make_unique<SlimeMoveControl>(*this)); }

void Slime::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<SlimeFloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<SlimeAttackGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<SlimeRandomDirectionGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<SlimeKeepOnJumpingGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false, [this](Actor& target, Level&) {
		return std::abs(target.position().y - _position.y) <= 4.0;
	}));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", true));
}

void Slime::setSize(int size, bool resetHealth) {
	int clamped = std::clamp(size, 1, 127);
	if (clamped != _size) {
		_size = clamped;
		markData(DATA_SIZE);
	}
	// refreshDimensions: its type's box, scaled
	setDimensions(_type.width * _size, _type.height * _size, _type.eyeHeight * _size);
	if (AttributeInstance* health = _attributes.getInstance(_ids.maxHealth)) health->setBaseValue(_size * _size);
	if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) speed->setBaseValue(0.2F + 0.1F * _size);
	if (AttributeInstance* damage = _attributes.getInstance(_ids.attackDamage)) damage->setBaseValue(_size);
	if (resetHealth) setHealth(maxHealth());
	_xpReward = _size;
}

void Slime::playJumpSound() {
	float pitch = ((_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F) * (isTiny() ? 1.4F : 0.8F); // getSoundPitch
	playSound(jumpSound(), soundVolume(), pitch);
}

void Slime::tick() {
	Mob::tick();
	if (isRemoved()) return;
	if (_onGround && !_wasOnGround) {
		// Landed: the particles are the clients' (their random draws are made), the squish sound
		int particles = static_cast<int>(std::ceil(_width * 2.0F * 16.0F));
		for (int i = 0; i < particles; i++) {
			_random.nextFloat();
			_random.nextFloat();
		}
		playSound(sizedSound("squish"), soundVolume(), ((_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F) / 0.8F);
	}
	_wasOnGround = _onGround;
}

void Slime::jumpFromGround() {
	_delta.y   = jumpPower();
	hasImpulse = true;
}

void Slime::dealDamage(Actor& target) {
	if (!isAlive() || !isWithinMeleeAttackRange(target) || !hasLineOfSight(target)) return;
	Combat::DamageSource source{"minecraft:mob_attack", this, nullptr, std::nullopt};
	bool hurt = target.asPlayer() ? Combat::damage(_level.server(), *target.asPlayer(), attackDamage(), source) : target.hurtServer(source, attackDamage());
	if (hurt) playSound(std::string(soundPrefix()) + "attack", 1.0F, (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
}

int Slime::touchedByPlayer(Player& player) {
	if (isDealsDamage()) dealDamage(player);
	return 0;
}

void Slime::pushAgainst(Entity& other) {
	Mob::pushAgainst(other);
	const GameData::EntityTypeInfo* type = _level.gameData().getEntityType(other.typeId());
	if (type && type->is("IronGolem") && isDealsDamage()) dealDamage(other);
}

void Slime::tickDeath() {
	Mob::tickDeath();
	if (!isRemoved() || _size <= 1) return;
	// Slime.remove: gone, it leaves 2 to 4 slimes of half its size
	float half	   = _width / 2.0F;
	int	  size	   = _size / 2;
	int	  children = 2 + _random.nextInt(3);
	for (int i = 0; i < children; i++) {
		float				 dx	   = (i % 2 - 0.5F) * half;
		float				 dz	   = (i / 2 - 0.5F) * half;
		std::unique_ptr<Mob> child = _level.mobs().create(_level, _typeId);
		auto*				 slime = dynamic_cast<Slime*>(child.get());
		if (!slime) break;
		slime->setSize(size, true);
		slime->snapTo({_position.x + dx, _position.y + 0.5, _position.z + dz}, _random.nextFloat() * 360.0F, 0.0F);
		slime->setNoAi(isNoAi());
		_level.addFreshEntity(std::move(child));
	}
}

std::shared_ptr<SpawnGroupData> Slime::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	JavaRandom& random = _level.random();
	int			power  = random.nextInt(3);
	if (power < 2 && random.nextFloat() < 0.5F * difficulty.getSpecialMultiplier()) power++;
	setSize(1 << power, true);
	return Mob::finalizeSpawn(difficulty, reason, group);
}

void Slime::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SIZE, onlyNonDefault, _size == 1)) writeIntData(buf, DATA_SIZE, _size);
}

void Slime::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeVarInt(_size);
	buf.writeBool(_wasOnGround);
}

void Slime::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown slime data version");
	float health = _health;
	setSize(buf.readVarInt(), false);
	setHealth(health);
	_wasOnGround = buf.readBool();
	_dirtyData	 = 0;
}

// ===================== MagmaCube =====================

void MagmaCube::setSize(int size, bool resetHealth) {
	Slime::setSize(size, resetHealth);
	if (AttributeInstance* armor = _attributes.getInstance(_ids.armor)) armor->setBaseValue(_size * 3);
}

void MagmaCube::jumpFromGround() {
	_delta.y   = jumpPower() + _size * 0.1F;
	hasImpulse = true;
}

void MagmaCube::jumpInLiquid(bool lava) {
	if (!lava) {
		Slime::jumpInLiquid(lava);
		return;
	}
	_delta.y   = 0.22F + _size * 0.05F;
	hasImpulse = true;
}
