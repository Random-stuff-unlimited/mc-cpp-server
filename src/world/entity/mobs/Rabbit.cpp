#include "world/entity/mobs/Rabbit.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>
#include <functional>

namespace {
	constexpr int		  SAVE_VERSION = 1;
	constexpr int		  EVENT_JUMP   = 1;
	constexpr const char* EVIL_ATTACK_POWER_MODIFIER = "minecraft:evil";
	constexpr int		  LEVEL_EVENT_BLOCK_BREAK	 = 2001;

	// Rabbit.RabbitJumpControl: a wanted jump starts a hop; it may only ask again once enabled after landing
	class RabbitJumpControl : public JumpControl {
	  public:
		explicit RabbitJumpControl(Rabbit& rabbit) : JumpControl(rabbit), _rabbit(rabbit) {}
		bool canJump() const { return _canJump; }
		void setCanJump(bool can) { _canJump = can; }
		void tick() override {
			if (_jump) {
				_rabbit.startJumping();
				_jump = false;
			}
		}

	  private:
		Rabbit& _rabbit;
		bool	_canJump = false;
	};

	// Rabbit.RabbitMoveControl: no speed on the ground between two hops
	class RabbitMoveControl : public MoveControl {
	  public:
		explicit RabbitMoveControl(Rabbit& rabbit) : MoveControl(rabbit), _rabbit(rabbit) {}
		void tick() override {
			if (_rabbit.onGround() && !_rabbit.isJumping() && !_rabbit.jumpControl().wantJump()) {
				_rabbit.setSpeedModifier(0.0);
			} else if (hasWanted() || _operation == Operation::Jumping) {
				_rabbit.setSpeedModifier(_nextJumpSpeed);
			}
			MoveControl::tick();
		}
		void setWantedPosition(double x, double y, double z, double speed) override {
			if (_rabbit.isInWaterNow()) speed = 1.5;
			MoveControl::setWantedPosition(x, y, z, speed);
			if (speed > 0.0) _nextJumpSpeed = speed;
		}

	  private:
		Rabbit& _rabbit;
		double	_nextJumpSpeed = 0;
	};

	class RabbitPanicGoal : public PanicGoal {
	  public:
		RabbitPanicGoal(Rabbit& rabbit, double speed) : PanicGoal(rabbit, speed), _rabbit(rabbit) {}
		void tick() override { _rabbit.setSpeedModifier(_speedModifier); }

	  private:
		Rabbit& _rabbit;
	};

	// The killer bunny doesn't flee
	class RabbitAvoidEntityGoal : public AvoidEntityGoal {
	  public:
		RabbitAvoidEntityGoal(Rabbit& rabbit, std::string avoidClass, float maxDist, double walk, double sprint)
			: AvoidEntityGoal(rabbit, std::move(avoidClass), maxDist, walk, sprint), _rabbit(rabbit) {}
		bool canUse() override { return _rabbit.variant() != Rabbit::Evil && AvoidEntityGoal::canUse(); }

	  private:
		Rabbit& _rabbit;
	};

	// Rabbit.RaidGardenGoal: goes to the farmland under ripe carrots and eats them, a stage at a time
	class RaidGardenGoal : public MoveToBlockGoal {
	  public:
		explicit RaidGardenGoal(Rabbit& rabbit) : MoveToBlockGoal(rabbit, 0.7F, 16), _rabbit(rabbit) {}
		bool canUse() override {
			if (_nextStartTick <= 0) {
				_canRaid	 = false;
				_wantsToRaid = _rabbit.wantsMoreFood();
			}
			return MoveToBlockGoal::canUse();
		}
		bool canContinueToUse() override { return _canRaid && MoveToBlockGoal::canContinueToUse(); }
		void tick() override {
			MoveToBlockGoal::tick();
			_rabbit.lookControl().setLookAt(_blockPos.x + 0.5, _blockPos.y + 1, _blockPos.z + 0.5, 10.0F, static_cast<float>(_rabbit.maxHeadXRot()));
			if (!isReachedTarget()) return;
			Level&				 level	= _rabbit.level();
			const BlockRegistry& blocks = level.blocks();
			BlockPos			 above	= _blockPos.above();
			int					 state	= level.getBlockState(above);
			if (_canRaid && blocks.blockOf(state) == level.gameData().getStaticId("minecraft:block", "minecraft:carrots")) {
				int age = blocks.getInt(state, blocks.property("age"));
				if (age == 0) {
					level.setBlock(above, level.gameData().getDefaultBlockState("minecraft:air"), Level::UPDATE_CLIENTS);
					level.destroyBlock(above, true);
				} else {
					level.setBlock(above, blocks.withInt(state, blocks.property("age"), age - 1), Level::UPDATE_CLIENTS);
					level.levelEvent(nullptr, LEVEL_EVENT_BLOCK_BREAK, above, state);
				}
				_rabbit.setMoreCarrotTicks(40);
			}
			_canRaid	   = false;
			_nextStartTick = 10;
		}

	  protected:
		bool isValidTarget(Level& level, const BlockPos& pos) override {
			const BlockRegistry& blocks = level.blocks();
			const GameData&		 data	= level.gameData();
			if (blocks.blockOf(level.getBlockState(pos)) != data.getStaticId("minecraft:block", "minecraft:farmland") || !_wantsToRaid || _canRaid) return false;
			int above = level.getBlockState(pos.above());
			if (blocks.blockOf(above) == data.getStaticId("minecraft:block", "minecraft:carrots") && blocks.getInt(above, blocks.property("age")) >= 7) {
				_canRaid = true;
				return true;
			}
			return false;
		}

	  private:
		Rabbit& _rabbit;
		bool	_wantsToRaid = false, _canRaid = false;
	};

	struct RabbitGroupData : SpawnGroupData {
		int variant;
		explicit RabbitGroupData(int v) : variant(v) {}
	};

	std::function<bool(const ItemStack&)> itemTag(Level& level, const std::string& tag) {
		return [&level, tag](const ItemStack& stack) { return !stack.isEmpty() && level.gameData().isInTag("minecraft:item", tag, stack.item); };
	}
} // namespace

Rabbit::Rabbit(Level& level, int typeId) : Animal(level, typeId) {
	setJumpControl(std::make_unique<RabbitJumpControl>(*this));
	setMoveControl(std::make_unique<RabbitMoveControl>(*this));
	setSpeedModifier(0.0);
}

void Rabbit::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	// ClimbOnTopOfPowderSnowGoal: powder snow isn't walked on yet
	_goalSelector.addGoal(1, std::make_unique<RabbitPanicGoal>(*this, 2.2));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 0.8));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.0, itemTag(_level, "minecraft:rabbit_food"), false));
	_goalSelector.addGoal(4, std::make_unique<RabbitAvoidEntityGoal>(*this, "Player", 8.0F, 2.2, 2.2));
	_goalSelector.addGoal(4, std::make_unique<RabbitAvoidEntityGoal>(*this, "Wolf", 10.0F, 2.2, 2.2));
	_goalSelector.addGoal(4, std::make_unique<RabbitAvoidEntityGoal>(*this, "Monster", 4.0F, 2.2, 2.2));
	_goalSelector.addGoal(5, std::make_unique<RaidGardenGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(11, std::make_unique<LookAtPlayerGoal>(*this, "Player", 10.0F));
}

void Rabbit::setSpeedModifier(double speed) {
	navigation().setSpeedModifier(speed);
	_moveControl->setWantedPosition(_moveControl->wantedX(), _moveControl->wantedY(), _moveControl->wantedZ(), speed);
}

float Rabbit::jumpPowerFactor() {
	float power = _moveControl->speedModifier() <= 0.6 ? 0.2F : 0.3F;
	Path* path	= navigation().getPath();
	if (path && !path->isDone() && path->getNextEntityPos(_width).y > _position.y + 0.5) power = 0.5F;
	if (_horizontalCollision || (_jumping && _moveControl->wantedY() > _position.y + 0.5)) power = 0.5F;
	return power / 0.42F;
}

void Rabbit::jumpFromGround() {
	Animal::jumpFromGround();
	if (_moveControl->speedModifier() > 0.0 && _delta.horizontalDistanceSqr() < 0.01) moveRelative(0.1F, {0.0, 0.0, 1.0});
	broadcastEntityEvent(EVENT_JUMP);
}

void Rabbit::setJumping(bool jumping) {
	Animal::setJumping(jumping);
	if (jumping) playSound("minecraft:entity.rabbit.jump", soundVolume(), ((_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F) * 0.8F);
}

void Rabbit::startJumping() {
	setJumping(true);
	_jumpDuration = 10;
	_jumpTicks	  = 0;
}

void Rabbit::facePoint(double x, double z) { _yRot = static_cast<float>(Mth::atan2(z - _position.z, x - _position.x) * 180.0F / (float)M_PI) - 90.0F; }

void Rabbit::customServerAiStep() {
	if (_jumpDelayTicks > 0) _jumpDelayTicks--;
	if (_moreCarrotTicks > 0) _moreCarrotTicks = std::max(0, _moreCarrotTicks - _random.nextInt(3));
	auto& jump = static_cast<RabbitJumpControl&>(*_jumpControl);
	if (_onGround) {
		if (!_wasOnGround) {
			// Landed: a pause before the next hop (short when it flees)
			setJumping(false);
			_jumpDelayTicks = _moveControl->speedModifier() < 2.2 ? 10 : 1;
			jump.setCanJump(false);
		}
		if (_variant == Evil && _jumpDelayTicks == 0) {
			Actor* target = getTarget();
			if (target && distanceToSqr(*target) < 16.0) {
				facePoint(target->position().x, target->position().z);
				_moveControl->setWantedPosition(target->position().x, target->position().y, target->position().z, _moveControl->speedModifier());
				startJumping();
				_wasOnGround = true;
			}
		}
		if (!jump.wantJump()) {
			if (_moveControl->hasWanted() && _jumpDelayTicks == 0) {
				Vec3  wanted{_moveControl->wantedX(), _moveControl->wantedY(), _moveControl->wantedZ()};
				Path* path = navigation().getPath();
				if (path && !path->isDone()) wanted = path->getNextEntityPos(_width);
				facePoint(wanted.x, wanted.z);
				startJumping();
			}
		} else if (!jump.canJump()) {
			jump.setCanJump(true);
		}
	}
	_wasOnGround = _onGround;
}

void Rabbit::aiStep() {
	Animal::aiStep();
	if (_jumpTicks != _jumpDuration) {
		_jumpTicks++;
	} else if (_jumpDuration != 0) {
		_jumpTicks	  = 0;
		_jumpDuration = 0;
		setJumping(false);
	}
}

void Rabbit::playAttackSound() {
	if (_variant == Evil) playSound("minecraft:entity.rabbit.attack", 1.0F, (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
}

void Rabbit::setVariant(int variant) {
	if (variant == Evil) {
		if (AttributeInstance* armor = _attributes.getInstance(_ids.armor)) armor->setBaseValue(8.0);
		_goalSelector.addGoal(4, std::make_unique<MeleeAttackGoal>(*this, 1.4, true));
		auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
		hurtBy->setAlertOthers();
		_targetSelector.addGoal(1, std::move(hurtBy));
		_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
		_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Wolf", true));
		if (AttributeInstance* damage = _attributes.getInstance(_ids.attackDamage)) {
			damage->removeModifier(EVIL_ATTACK_POWER_MODIFIER);
			damage->addModifier({EVIL_ATTACK_POWER_MODIFIER, 5.0, AttributeModifier::Operation::AddValue}, false);
		}
	} else if (AttributeInstance* damage = _attributes.getInstance(_ids.attackDamage)) {
		damage->removeModifier(EVIL_ATTACK_POWER_MODIFIER);
	}
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_TYPE);
}

int Rabbit::randomVariant(const BlockPos& pos) {
	const GameData& data  = _level.gameData();
	int				biome = _level.getBiomeId(pos);
	int				roll  = _level.random().nextInt(100);
	if (data.isInTag("minecraft:worldgen/biome", "minecraft:spawns_white_rabbits", biome)) return roll < 80 ? White : WhiteSplotched;
	if (data.isInTag("minecraft:worldgen/biome", "minecraft:spawns_gold_rabbits", biome)) return Gold;
	return roll < 50 ? Brown : roll < 90 ? Salt : Black;
}

std::shared_ptr<SpawnGroupData> Rabbit::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	int variant = randomVariant(blockPosition());
	if (auto* data = dynamic_cast<RabbitGroupData*>(group.get())) {
		variant = data->variant;
	} else {
		group = std::make_shared<RabbitGroupData>(variant);
	}
	setVariant(variant);
	return Animal::finalizeSpawn(difficulty, reason, group);
}

std::unique_ptr<Mob> Rabbit::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child = Animal::getBreedOffspring(partner);
	auto*				 kit   = dynamic_cast<Rabbit*>(child.get());
	if (!kit) return child;
	int variant = randomVariant(blockPosition());
	if (_random.nextInt(20) != 0) {
		auto* other = dynamic_cast<Rabbit*>(&partner);
		variant		= other && _random.nextBoolean() ? other->_variant : _variant;
	}
	kit->setVariant(variant);
	return child;
}

std::string Rabbit::lootComponent(const std::string& component) const {
	if (component == "minecraft:rabbit/variant") {
		switch (_variant) {
		case White: return "white";
		case Black: return "black";
		case WhiteSplotched: return "white_splotched";
		case Gold: return "gold";
		case Salt: return "salt";
		case Evil: return "evil";
		default: return "brown";
		}
	}
	return Animal::lootComponent(component);
}

void Rabbit::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_TYPE, onlyNonDefault, _variant == Brown)) writeIntData(buf, DATA_TYPE, _variant);
}

void Rabbit::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeVarInt(_variant);
	buf.writeVarInt(_moreCarrotTicks);
}

void Rabbit::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown rabbit data version");
	setVariant(buf.readVarInt());
	_moreCarrotTicks = buf.readVarInt();
	_dirtyData		 = 0;
}
