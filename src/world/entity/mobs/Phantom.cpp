#include "world/entity/mobs/Phantom.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/Felines.hpp"

#include <algorithm>
#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int LEVEL_EVENT_PHANTOM_BITE = 1039;

	// Mth.approach, approachDegrees, degreesDifferenceAbs
	float approach(float value, float target, float step) {
		step = std::abs(step);
		return value < target ? Mth::clamp(value + step, value, target) : Mth::clamp(value - step, target, value);
	}
	float approachDegrees(float angle, float target, float step) { return approach(angle, angle + Mth::degreesDifference(angle, target), step); }

	bool canAttackDefault(Phantom& phantom, Actor& target) { return TargetingConditions::forCombat().test(phantom.level(), &phantom, target); }

	// Phantom.PhantomMoveControl: glides toward its move target, turning at most 4 degrees a tick, faster when it
	// flies straight
	class PhantomMoveControl : public MoveControl {
	  public:
		explicit PhantomMoveControl(Phantom& phantom) : MoveControl(phantom), _phantom(phantom) {}
		void tick() override {
			if (_phantom.horizontalCollision()) {
				_phantom.setYRotPublic(_phantom.yRot() + 180.0F);
				_speed = 0.1F;
			}
			double dx = _phantom.moveTargetPoint.x - _phantom.position().x;
			double dy = _phantom.moveTargetPoint.y - _phantom.position().y;
			double dz = _phantom.moveTargetPoint.z - _phantom.position().z;
			double horizontal = std::sqrt(dx * dx + dz * dz);
			if (!(std::abs(horizontal) > 1.0E-5F)) return;
			double scale = 1.0 - std::abs(dy * 0.7F) / horizontal;
			dx *= scale;
			dz *= scale;
			horizontal		= std::sqrt(dx * dx + dz * dz);
			double distance = std::sqrt(dx * dx + dz * dz + dy * dy);
			float  before	= _phantom.yRot();
			float  wanted	= static_cast<float>(Mth::atan2(dz, dx));
			float  current	= Mth::wrapDegrees(_phantom.yRot() + 90.0F);
			float  target	= Mth::wrapDegrees(wanted * (180.0F / (float)M_PI));
			_phantom.setYRotPublic(approachDegrees(current, target, 4.0F) - 90.0F);
			_phantom.setYBodyRot(_phantom.yRot());
			if (std::abs(Mth::degreesDifference(before, _phantom.yRot())) < 3.0F) {
				_speed = approach(_speed, 1.8F, 0.005F * (1.8F / _speed));
			} else {
				_speed = approach(_speed, 0.2F, 0.025F);
			}
			float pitch = static_cast<float>(-(Mth::atan2(-dy, horizontal) * 180.0F / (float)M_PI));
			_phantom.setXRot(pitch);
			const float toRadians = static_cast<float>(M_PI / 180.0);
			float		yaw		  = _phantom.yRot() + 90.0F;
			double		x = _speed * Mth::cos(yaw * toRadians) * std::abs(dx / distance);
			double		z = _speed * Mth::sin(yaw * toRadians) * std::abs(dz / distance);
			double		y = _speed * Mth::sin(pitch * toRadians) * std::abs(dy / distance);
			Vec3		delta = _phantom.deltaMovement();
			_phantom.setDeltaMovement(delta + (Vec3{x, y, z} - delta).scale(0.2));
		}

	  private:
		Phantom& _phantom;
		float	 _speed = 0.1F;
	};

	class PhantomLookControl : public LookControl {
	  public:
		using LookControl::LookControl;
		void tick() override {}
	};

	// Every 3 seconds: the highest player within 16 blocks (64 up and down) it can attack
	class PhantomAttackPlayerTargetGoal : public Goal {
	  public:
		explicit PhantomAttackPlayerTargetGoal(Phantom& phantom) : _phantom(phantom) {}
		bool canUse() override {
			if (_nextScanTick > 0) {
				_nextScanTick--;
				return false;
			}
			_nextScanTick = reducedTickDelay(60);
			Level&				level = _phantom.level();
			TargetingConditions range = TargetingConditions::forCombat();
			range.withRange(64.0);
			AABB				 area = _phantom.boundingBox().inflate(16.0, 64.0, 16.0);
			std::vector<Player*> players;
			for (const auto& player : level.players()) {
				if (player->boundingBox().intersects(area) && range.test(level, &_phantom, *player)) players.push_back(player.get());
			}
			std::stable_sort(players.begin(), players.end(), [](Player* a, Player* b) { return a->getY() > b->getY(); });
			for (Player* player : players) {
				if (canAttackDefault(_phantom, *player)) {
					_phantom.setTarget(player);
					return true;
				}
			}
			return false;
		}
		bool canContinueToUse() override {
			Actor* target = _phantom.getTarget();
			return target && canAttackDefault(_phantom, *target);
		}

	  private:
		Phantom& _phantom;
		int		 _nextScanTick = reducedTickDelay(20);
	};

	// Circles for 8 to 12 seconds, then swoops
	class PhantomAttackStrategyGoal : public Goal {
	  public:
		explicit PhantomAttackStrategyGoal(Phantom& phantom) : _phantom(phantom) {}
		bool canUse() override {
			Actor* target = _phantom.getTarget();
			return target && canAttackDefault(_phantom, *target);
		}
		void start() override {
			_nextSweepTick		 = adjustedTickDelay(10);
			_phantom.attackPhase = Phantom::AttackPhase::Circle;
			setAnchorAboveTarget();
		}
		void stop() override {
			if (!_phantom.hasAnchor) return;
			Level& level = _phantom.level();
			int	   top	 = level.getHeight(Level::Heightmap::MotionBlocking, _phantom.anchorPoint.x, _phantom.anchorPoint.z);
			_phantom.anchorPoint = {_phantom.anchorPoint.x, top + 10 + _phantom.random().nextInt(20), _phantom.anchorPoint.z};
		}
		void tick() override {
			if (_phantom.attackPhase != Phantom::AttackPhase::Circle) return;
			if (--_nextSweepTick > 0) return;
			_phantom.attackPhase = Phantom::AttackPhase::Swoop;
			setAnchorAboveTarget();
			_nextSweepTick = adjustedTickDelay((8 + _phantom.random().nextInt(4)) * 20);
			_phantom.level().playSoundAt(nullptr, _phantom.position().x, _phantom.position().y, _phantom.position().z, "minecraft:entity.phantom.swoop",
										 Level::SoundSource::Hostile, 10.0F, 0.95F + _phantom.random().nextFloat() * 0.1F);
		}

	  private:
		Phantom& _phantom;
		int		 _nextSweepTick = 0;

		void setAnchorAboveTarget() {
			Actor* target = _phantom.getTarget();
			if (!_phantom.hasAnchor || !target) return;
			const Vec3& p = target->position();
			BlockPos	anchor{Mth::floor(p.x), Mth::floor(p.y) + 20 + _phantom.random().nextInt(20), Mth::floor(p.z)};
			int			sea = _phantom.level().seaLevel();
			if (anchor.y < sea) anchor.y = sea + 1;
			_phantom.anchorPoint = anchor;
		}
	};

	bool touchingTarget(Phantom& phantom) { return (phantom.moveTargetPoint - phantom.position()).lengthSqr() < 4.0; }

	// Around its anchor, at a distance and a height that change now and then
	class PhantomCircleAroundAnchorGoal : public Goal {
	  public:
		explicit PhantomCircleAroundAnchorGoal(Phantom& phantom) : _phantom(phantom) { setFlags({Flag::Move}); }
		bool canUse() override { return !_phantom.getTarget() || _phantom.attackPhase == Phantom::AttackPhase::Circle; }
		void start() override {
			JavaRandom& random = _phantom.random();
			_distance		   = 5.0F + random.nextFloat() * 10.0F;
			_height			   = -4.0F + random.nextFloat() * 9.0F;
			_clockwise		   = random.nextBoolean() ? 1.0F : -1.0F;
			selectNext();
		}
		void tick() override {
			JavaRandom& random = _phantom.random();
			Level&		level  = _phantom.level();
			if (random.nextInt(adjustedTickDelay(350)) == 0) _height = -4.0F + random.nextFloat() * 9.0F;
			if (random.nextInt(adjustedTickDelay(250)) == 0) {
				_distance++;
				if (_distance > 15.0F) {
					_distance  = 5.0F;
					_clockwise = -_clockwise;
				}
			}
			if (random.nextInt(adjustedTickDelay(450)) == 0) {
				_angle = random.nextFloat() * 2.0F * static_cast<float>(M_PI);
				selectNext();
			}
			if (touchingTarget(_phantom)) selectNext();
			BlockPos pos = _phantom.blockPosition();
			if (_phantom.moveTargetPoint.y < _phantom.position().y && !level.blocks().isAir(level.getBlockState(pos.below()))) {
				_height = std::max(1.0F, _height);
				selectNext();
			}
			if (_phantom.moveTargetPoint.y > _phantom.position().y && !level.blocks().isAir(level.getBlockState(pos.above()))) {
				_height = std::min(-1.0F, _height);
				selectNext();
			}
		}

	  private:
		Phantom& _phantom;
		float	 _angle = 0, _distance = 0, _height = 0, _clockwise = 1;

		void selectNext() {
			if (!_phantom.hasAnchor) {
				_phantom.anchorPoint = _phantom.blockPosition();
				_phantom.hasAnchor	 = true;
			}
			_angle += _clockwise * 15.0F * static_cast<float>(M_PI / 180.0);
			const BlockPos& anchor	 = _phantom.anchorPoint;
			_phantom.moveTargetPoint = {anchor.x + _distance * Mth::cos(_angle), anchor.y + -4.0F + _height, anchor.z + _distance * Mth::sin(_angle)};
		}
	};

	// Dives onto its target and bites; gives up if a cat is within 16 blocks
	class PhantomSweepAttackGoal : public Goal {
	  public:
		explicit PhantomSweepAttackGoal(Phantom& phantom) : _phantom(phantom) { setFlags({Flag::Move}); }
		bool canUse() override { return _phantom.getTarget() && _phantom.attackPhase == Phantom::AttackPhase::Swoop; }
		bool canContinueToUse() override {
			Actor* target = _phantom.getTarget();
			if (!target || !target->isAlive()) return false;
			if (Player* player = target->asPlayer(); player && (player->isSpectator() || player->isCreative())) return false;
			if (!canUse()) return false;
			if (_phantom.tickCount() > _catSearchTick) {
				_catSearchTick = _phantom.tickCount() + 20;
				bool cats	   = false;
				_phantom.level().entities().forEachIn(_phantom.boundingBox().inflate(16.0), [&](Entity& entity) {
					auto* cat = dynamic_cast<Cat*>(&entity);
					if (!cat || cat->isRemoved() || !cat->isAlive()) return;
					cat->hiss();
					cats = true;
				});
				_scaredOfCat = cats;
			}
			return !_scaredOfCat;
		}
		void stop() override {
			_phantom.setTarget(nullptr);
			_phantom.attackPhase = Phantom::AttackPhase::Circle;
		}
		void tick() override {
			Actor* target = _phantom.getTarget();
			if (!target) return;
			AABB box				 = target->boundingBox();
			_phantom.moveTargetPoint = {target->position().x, target->position().y + (box.maxY - box.minY) * 0.5, target->position().z};
			if (_phantom.boundingBox().inflate(0.2F).intersects(box)) {
				_phantom.doHurtTarget(*target);
				_phantom.attackPhase = Phantom::AttackPhase::Circle;
				_phantom.level().levelEvent(nullptr, LEVEL_EVENT_PHANTOM_BITE, _phantom.blockPosition(), 0);
			} else if (_phantom.horizontalCollision() || _phantom.hurtTime() > 0) {
				_phantom.attackPhase = Phantom::AttackPhase::Circle;
			}
		}

	  private:
		Phantom& _phantom;
		bool	 _scaredOfCat	= false;
		int		 _catSearchTick = 0;
	};
} // namespace

// ===================== Endermite =====================

void Endermite::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	// ClimbOnTopOfPowderSnowGoal: powder snow isn't walked on yet
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(3, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

void Endermite::tick() {
	_yBodyRot = _yRot;
	Monster::tick();
}

void Endermite::aiStep() {
	Monster::aiStep();
	if (!isPersistenceRequired()) _life++;
	if (_life >= 2400) discard();
}

void Endermite::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeVarInt(_life);
}

void Endermite::load(Buffer& buf) {
	Monster::load(buf);
	_life = buf.readVarInt();
}

// ===================== Phantom =====================

Phantom::Phantom(Level& level, int typeId) : Mob(level, typeId) {
	_xpReward = 5;
	setMoveControl(std::make_unique<PhantomMoveControl>(*this));
	setLookControl(std::make_unique<PhantomLookControl>(*this));
}

void Phantom::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<PhantomAttackStrategyGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<PhantomSweepAttackGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<PhantomCircleAroundAnchorGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<PhantomAttackPlayerTargetGoal>(*this));
}

void Phantom::setPhantomSize(int size) {
	size = std::clamp(size, 0, 64);
	// updatePhantomSizeInfo: its box and its bite grow with it
	float scale = 1.0F + 0.15F * size;
	setDimensions(_type.width * scale, _type.height * scale, _type.eyeHeight * scale);
	if (AttributeInstance* damage = _attributes.getInstance(_ids.attackDamage)) damage->setBaseValue(6 + size);
	if (size == _size) return;
	_size = size;
	markData(DATA_SIZE);
}

void Phantom::aiStep() {
	if (isAlive() && isSunBurnTick()) igniteForTicks(160);
	Mob::aiStep();
}

std::shared_ptr<SpawnGroupData> Phantom::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	anchorPoint = blockPosition().above(5);
	hasAnchor	= true;
	setPhantomSize(0);
	return Mob::finalizeSpawn(difficulty, reason, group);
}

void Phantom::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SIZE, onlyNonDefault, _size == 0)) writeIntData(buf, DATA_SIZE, _size);
}

void Phantom::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(hasAnchor);
	buf.writeInt(anchorPoint.x);
	buf.writeInt(anchorPoint.y);
	buf.writeInt(anchorPoint.z);
	buf.writeVarInt(_size);
}

void Phantom::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown phantom data version");
	hasAnchor	  = buf.readBool();
	anchorPoint.x = buf.readInt();
	anchorPoint.y = buf.readInt();
	anchorPoint.z = buf.readInt();
	setPhantomSize(buf.readVarInt());
	_dirtyData = 0;
}
