#include "world/entity/mobs/WaterAnimals.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/blockentity/StorageEntities.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"
#include "world/item/ItemUse.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int GAME_EVENT_PUFFER_FISH_STING = 9; // ClientboundGameEventPacket.PUFFER_FISH_STING

	// WaterAnimal.handleAirSupply: out of the water its air runs out, then it suffocates
	template <typename T> void handleAirSupply(T& mob, int airBefore) {
		if (mob.isAlive() && !mob.isInWaterNow()) {
			mob.setAirSupply(airBefore - 1);
			if (mob.airSupply() <= -20) {
				mob.setAirSupply(0);
				mob.hurtServer({"minecraft:drown", nullptr}, 2.0F);
			}
		} else {
			mob.setAirSupply(300);
		}
	}

	// BehaviorUtils.getRandomSwimmablePos: a random position in something swimmable
	std::optional<Vec3> randomSwimmablePos(Mob& mob, int horizontal, int vertical) {
		Level&				level = mob.level();
		std::optional<Vec3> pos	  = RandomPos::defaultPos(mob, horizontal, vertical);
		for (int tries = 0; pos && tries < 10; tries++) {
			int state = level.getBlockState({Mth::floor(pos->x), Mth::floor(pos->y), Mth::floor(pos->z)});
			if (level.gameData().getStateProperties(state).pathfindable & static_cast<int>(PathComputationType::Water)) break;
			pos = RandomPos::defaultPos(mob, horizontal, vertical);
		}
		return pos;
	}

	// Squid.SquidRandomMovementGoal: a new direction now and then, none once it has been still for 5 seconds
	class SquidRandomMovementGoal : public Goal {
	  public:
		explicit SquidRandomMovementGoal(Squid& squid) : _squid(squid) {}
		bool canUse() override { return true; }
		void tick() override {
			if (_squid.noActionTime() > 100) {
				_squid.setMovementVector({});
			} else if (_squid.random().nextInt(reducedTickDelay(50)) == 0 || !_squid.wasTouchingWater() || !_squid.hasMovementVector()) {
				float angle = _squid.random().nextFloat() * static_cast<float>(M_PI * 2);
				_squid.setMovementVector({Mth::cos(angle) * 0.2F, -0.1F + _squid.random().nextFloat() * 0.2F, Mth::sin(angle) * 0.2F});
			}
		}

	  private:
		Squid& _squid;
	};

	// Squid.SquidFleeGoal: away from who hurt it, within 10 blocks
	class SquidFleeGoal : public Goal {
	  public:
		explicit SquidFleeGoal(Squid& squid) : _squid(squid) {}
		bool canUse() override {
			Actor* attacker = _squid.getLastHurtByMob();
			return _squid.isInWaterNow() && attacker && _squid.distanceToSqr(*attacker) < 100.0;
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* attacker = _squid.getLastHurtByMob();
			if (!attacker) return;
			Level&		level = _squid.level();
			const Vec3& p	  = _squid.position();
			Vec3		away  = p - attacker->position();
			BlockPos	at{Mth::floor(p.x + away.x), Mth::floor(p.y + away.y), Mth::floor(p.z + away.z)};
			int			state = level.getBlockState(at);
			bool		air	  = level.blocks().isAir(state);
			if (!level.fluids().isWater(level.fluids().stateOf(state).type) && !air) return;
			double length = away.length();
			if (length > 0.0) {
				// As vanilla: the vector isn't normalized (its result is dropped)
				double speed = 3.0;
				if (length > 5.0) speed -= (length - 5.0) / 5.0;
				if (speed > 0.0) away = away.scale(speed);
			}
			if (air) away.y = 0.0;
			_squid.setMovementVector({away.x / 20.0, away.y / 20.0, away.z / 20.0});
		}

	  private:
		Squid& _squid;
	};

	// AbstractFish.FishMoveControl
	class FishMoveControl : public MoveControl {
	  public:
		explicit FishMoveControl(AbstractFish& fish) : MoveControl(fish), _fish(fish) {}
		void tick() override {
			if (_fish.isEyeInWater()) _fish.setDeltaMovement(_fish.deltaMovement() + Vec3{0.0, 0.005, 0.0});
			if (_operation != Operation::MoveTo || _fish.navigation().isDone()) {
				_fish.setSpeed(0.0F);
				return;
			}
			float wanted = static_cast<float>(_speedModifier * _fish.getAttributeValue(_fish.attributeIds().movementSpeed));
			_fish.setSpeed(_fish.speed() + 0.125F * (wanted - _fish.speed()));
			double dx = _wantedX - _fish.position().x, dy = _wantedY - _fish.position().y, dz = _wantedZ - _fish.position().z;
			if (dy != 0.0) {
				double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
				_fish.setDeltaMovement(_fish.deltaMovement() + Vec3{0.0, _fish.speed() * (dy / distance) * 0.1, 0.0});
			}
			if (dx != 0.0 || dz != 0.0) {
				float yaw = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
				_fish.setYRotPublic(rotlerp(_fish.yRot(), yaw, 90.0F));
				_fish.setYBodyRot(_fish.yRot());
			}
		}

	  private:
		AbstractFish& _fish;
	};

	// AbstractFish.FishSwimGoal (RandomSwimmingGoal)
	class FishSwimGoal : public RandomStrollGoal {
	  public:
		explicit FishSwimGoal(AbstractFish& fish) : RandomStrollGoal(fish, 1.0, 40), _fish(fish) {}
		bool canUse() override { return _fish.canRandomSwim() && RandomStrollGoal::canUse(); }

	  protected:
		std::optional<Vec3> getPosition() override { return randomSwimmablePos(_mob, 10, 7); }

	  private:
		AbstractFish& _fish;
	};

	// FollowFlockLeaderGoal: joins (or founds) a school every 10 seconds or so, then follows its leader
	class FollowFlockLeaderGoal : public Goal {
	  public:
		explicit FollowFlockLeaderGoal(AbstractSchoolingFish& fish) : _mob(fish) { _nextStartTick = nextStartTick(); }
		bool canUse() override {
			if (_mob.hasFollowers()) return false;
			if (_mob.isFollower()) return true;
			if (_nextStartTick > 0) {
				_nextStartTick--;
				return false;
			}
			_nextStartTick = nextStartTick();
			// The fish of its type around that lead a school with room, or follow none
			std::vector<AbstractSchoolingFish*> around;
			_mob.level().entities().forEachIn(_mob.boundingBox().inflate(8.0), [&](Entity& entity) {
				auto* fish = dynamic_cast<AbstractSchoolingFish*>(&entity);
				if (fish && !fish->isRemoved() && fish->typeId() == _mob.typeId() && (fish->canBeFollowed() || !fish->isFollower())) around.push_back(fish);
			});
			AbstractSchoolingFish* leader = &_mob;
			for (AbstractSchoolingFish* fish : around) {
				if (fish->canBeFollowed()) {
					leader = fish;
					break;
				}
			}
			std::vector<AbstractSchoolingFish*> free;
			for (AbstractSchoolingFish* fish : around) {
				if (!fish->isFollower()) free.push_back(fish);
			}
			leader->addFollowers(free);
			return _mob.isFollower();
		}
		bool canContinueToUse() override { return _mob.isFollower() && _mob.inRangeOfLeader(); }
		void start() override { _timeToRecalcPath = 0; }
		void stop() override { _mob.stopFollowing(); }
		void tick() override {
			if (--_timeToRecalcPath <= 0) {
				_timeToRecalcPath = adjustedTickDelay(10);
				_mob.pathToLeader();
			}
		}

	  private:
		AbstractSchoolingFish& _mob;
		int					   _timeToRecalcPath = 0, _nextStartTick = 0;
		int					   nextStartTick() { return reducedTickDelay(200 + _mob.random().nextInt(200) % 20); }
	};

	struct SchoolSpawnGroupData : SpawnGroupData {
		EntityRef leader;
		int		  variant = -1; // TropicalFishGroupData's
	};

	class PufferfishPuffGoal : public Goal {
	  public:
		explicit PufferfishPuffGoal(Pufferfish& fish) : _fish(fish) {}
		bool canUse() override { return _fish.scaryNearby(2.0); }
		void start() override { _fish.startPuffing(); }
		void stop() override { _fish.stopPuffing(); }

	  private:
		Pufferfish& _fish;
	};

	// TropicalFish.COMMON_VARIANTS: pattern (base | index << 8), base color, pattern color (DyeColor ids)
	constexpr int KOB = 0 | 0 << 8, SUNSTREAK = 0 | 1 << 8, SNOOPER = 0 | 2 << 8, DASHER = 0 | 3 << 8, BRINELY = 0 | 4 << 8, SPOTTY = 0 | 5 << 8,
				  FLOPPER = 1 | 0 << 8, STRIPEY = 1 | 1 << 8, GLITTER = 1 | 2 << 8, BLOCKFISH = 1 | 3 << 8, BETTY = 1 | 4 << 8, CLAYFISH = 1 | 5 << 8;
	constexpr int PATTERNS[12] = {KOB, SUNSTREAK, SNOOPER, DASHER, BRINELY, SPOTTY, FLOPPER, STRIPEY, GLITTER, BLOCKFISH, BETTY, CLAYFISH};
	constexpr int WHITE = 0, ORANGE = 1, LIGHT_BLUE = 3, YELLOW = 4, LIME = 5, PINK = 6, GRAY = 7, CYAN = 9, PURPLE = 10, BLUE = 11, RED = 14;
	constexpr int pack(int pattern, int base, int color) { return (pattern & 65535) | (base & 0xFF) << 16 | (color & 0xFF) << 24; }
	constexpr int COMMON_VARIANTS[22] = {
			pack(STRIPEY, ORANGE, GRAY),   pack(FLOPPER, GRAY, GRAY),	  pack(FLOPPER, GRAY, BLUE),	pack(CLAYFISH, WHITE, GRAY),
			pack(SUNSTREAK, BLUE, GRAY),   pack(KOB, ORANGE, WHITE),	  pack(SPOTTY, PINK, LIGHT_BLUE), pack(BLOCKFISH, PURPLE, YELLOW),
			pack(CLAYFISH, WHITE, RED),	   pack(SPOTTY, WHITE, YELLOW),	  pack(GLITTER, WHITE, GRAY),	pack(CLAYFISH, WHITE, ORANGE),
			pack(DASHER, CYAN, PINK),	   pack(BRINELY, LIME, LIGHT_BLUE), pack(BETTY, RED, WHITE),	pack(SNOOPER, GRAY, RED),
			pack(BLOCKFISH, RED, WHITE),   pack(FLOPPER, WHITE, YELLOW),  pack(KOB, RED, WHITE),		pack(SUNSTREAK, GRAY, WHITE),
			pack(DASHER, CYAN, YELLOW),	   pack(FLOPPER, YELLOW, YELLOW),
	};
} // namespace

void WaterAnimal::livingBaseTick() {
	int air = _airSupply;
	Mob::livingBaseTick();
	handleAirSupply(*this, air);
}

void AgeableWaterCreature::livingBaseTick() {
	int air = _airSupply;
	AgeableMob::livingBaseTick();
	if (suffocatesOnLand()) handleAirSupply(*this, air);
}

// ===================== Goals of the water =====================

std::optional<Vec3> RandomSwimmingGoal::getPosition() { return randomSwimmablePos(_mob, 10, 7); }

bool BreathAirGoal::canUse() { return _mob.airSupply() < 140; }

void BreathAirGoal::findAirPosition() {
	Level&					level = _mob.level();
	const GameData&			data  = level.gameData();
	const Vec3&				p	  = _mob.position();
	std::optional<BlockPos> found;
	// BlockPos.betweenClosed: x fastest, then y, then z
	for (int z = Mth::floor(p.z - 1.0); z <= Mth::floor(p.z + 1.0) && !found; z++) {
		for (int y = Mth::floor(p.y); y <= Mth::floor(p.y + 8.0) && !found; y++) {
			for (int x = Mth::floor(p.x - 1.0); x <= Mth::floor(p.x + 1.0) && !found; x++) {
				int	 state	= level.getBlockState({x, y, z});
				bool bubble = data.getBlocks().blockOf(state) == level.bubbleColumnBlock();
				bool land	= data.getStateProperties(state).pathfindable & static_cast<int>(PathComputationType::Land);
				if ((level.fluids().stateOf(state).isEmpty() || bubble) && land) found = BlockPos{x, y, z};
			}
		}
	}
	if (!found) found = BlockPos{Mth::floor(p.x), Mth::floor(p.y + 8.0), Mth::floor(p.z)};
	_mob.navigation().moveTo(found->x, found->y + 1, found->z, 1.0);
}

void BreathAirGoal::tick() {
	findAirPosition();
	_mob.moveWithInput(0.02F);
}

bool TryFindWaterGoal::canUse() {
	Level& level = _mob.level();
	return _mob.onGround() && !level.fluids().isWater(level.fluids().stateOf(level.getBlockState(_mob.blockPosition())).type);
}

void TryFindWaterGoal::start() {
	Level&		level = _mob.level();
	const Vec3& p	  = _mob.position();
	for (int z = Mth::floor(p.z - 2.0); z <= Mth::floor(p.z + 2.0); z++) {
		for (int y = Mth::floor(p.y - 2.0); y <= Mth::floor(p.y); y++) {
			for (int x = Mth::floor(p.x - 2.0); x <= Mth::floor(p.x + 2.0); x++) {
				if (!level.fluids().isWater(level.fluids().stateOf(level.getBlockState({x, y, z})).type)) continue;
				_mob.moveControl().setWantedPosition(x, y, z, 1.0);
				return;
			}
		}
	}
}

// ===================== Squid =====================

Squid::Squid(Level& level, int typeId) : AgeableWaterCreature(level, typeId) {
	_random.setSeed(_id);
	_tentacleSpeed = 1.0F / (_random.nextFloat() + 1.0F) * 0.2F;
}

void Squid::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<SquidRandomMovementGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<SquidFleeGoal>(*this));
}

void Squid::aiStep() {
	AgeableWaterCreature::aiStep();
	if (isRemoved()) return;
	_xBodyRotO	 = _xBodyRot;
	_yBodyRotOld = _yBodyRot;
	_tentacleMovement += _tentacleSpeed;
	if (_tentacleMovement > M_PI * 2) {
		_tentacleMovement -= static_cast<float>(M_PI * 2);
		if (_random.nextInt(10) == 0) _tentacleSpeed = 1.0F / (_random.nextFloat() + 1.0F) * 0.2F;
		broadcastEntityEvent(EVENT_RESET_TENTACLES);
	}
	if (isInWater()) {
		if (_tentacleMovement < static_cast<float>(M_PI)) {
			float phase = _tentacleMovement / static_cast<float>(M_PI);
			if (phase > 0.75) {
				_delta		 = _movementVector; // The burst
				_rotateSpeed = 1.0F;
			} else {
				_rotateSpeed *= 0.8F;
			}
		} else {
			_delta = _delta.scale(0.9);
			_rotateSpeed *= 0.99F;
		}
		double horizontal = std::sqrt(_delta.horizontalDistanceSqr());
		_yBodyRot += (-static_cast<float>(Mth::atan2(_delta.x, _delta.z)) * (180.0F / (float)M_PI) - _yBodyRot) * 0.1F;
		_yRot = _yBodyRot;
		_zBodyRot += static_cast<float>(M_PI) * _rotateSpeed * 1.5F;
		_xBodyRot += (-static_cast<float>(Mth::atan2(horizontal, _delta.y)) * (180.0F / (float)M_PI) - _xBodyRot) * 0.1F;
	} else {
		// Out of the water: it falls straight down
		double y = _delta.y;
		if (const MobEffects::Instance* levitation = _effects.get(MobEffects::Levitation)) {
			y = 0.05 * (levitation->amplifier + 1);
		} else {
			y -= getAttributeValue(_ids.gravity);
		}
		_delta = {0.0, y * 0.98F, 0.0};
		_xBodyRot += (-90.0F - _xBodyRot) * 0.02F;
	}
}

bool Squid::hurtServer(const Combat::DamageSource& source, float amount) {
	if (AgeableWaterCreature::hurtServer(source, amount) && getLastHurtByMob()) {
		spawnInk();
		return true;
	}
	return false;
}

Vec3 Squid::rotateVector(const Vec3& v) const {
	// Vec3.xRot then yRot
	const float toRadians = static_cast<float>(M_PI / 180.0);
	float		pitch = _xBodyRotO * toRadians, yaw = -_yBodyRotOld * toRadians;
	float		cp = Mth::cos(pitch), sp = Mth::sin(pitch);
	Vec3		a{v.x, v.y * cp + v.z * sp, v.z * cp - v.y * sp};
	float		cy = Mth::cos(yaw), sy = Mth::sin(yaw);
	return {a.x * cy + a.z * sy, a.y, a.z * cy - a.x * sy};
}

void Squid::spawnInk() {
	playSound(squirtSound(), soundVolume(), voicePitch());
	Vec3 origin = rotateVector({0.0, -1.0, 0.0}) + _position;
	for (int i = 0; i < 30; i++) {
		Vec3  direction = rotateVector({_random.nextFloat() * 0.6 - 0.3, -1.0, _random.nextFloat() * 0.6 - 0.3});
		float base		= isBaby() ? 0.1F : 0.3F;
		Vec3  velocity	= direction.scale(base + _random.nextFloat() * 2.0F);
		sendParticles(_level, inkParticle(), origin.x, origin.y + 0.5, origin.z, 0, velocity.x, velocity.y, velocity.z, 0.1F);
	}
}

// ===================== GlowSquid =====================

void GlowSquid::setDarkTicks(int ticks) {
	if (ticks == _darkTicks) return;
	_darkTicks = ticks;
	markData(DATA_DARK_TICKS_REMAINING);
}

void GlowSquid::aiStep() {
	Squid::aiStep();
	if (_darkTicks > 0) setDarkTicks(_darkTicks - 1);
	// The glow particle is the clients'; its random draws are made here too
	_random.nextDouble();
	_random.nextDouble();
	_random.nextDouble();
}

bool GlowSquid::hurtServer(const Combat::DamageSource& source, float amount) {
	bool hurt = Squid::hurtServer(source, amount);
	if (hurt) setDarkTicks(100);
	return hurt;
}

void GlowSquid::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Squid::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_DARK_TICKS_REMAINING, onlyNonDefault, _darkTicks == 0)) writeIntData(buf, DATA_DARK_TICKS_REMAINING, _darkTicks);
}

void GlowSquid::save(Buffer& buf) const {
	Squid::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeVarInt(_darkTicks);
}

void GlowSquid::load(Buffer& buf) {
	Squid::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown glow squid data version");
	_darkTicks = buf.readVarInt();
	_dirtyData = 0;
}

// ===================== AbstractFish =====================

AbstractFish::AbstractFish(Level& level, int typeId) : WaterAnimal(level, typeId) { setMoveControl(std::make_unique<FishMoveControl>(*this)); }

std::unique_ptr<PathNavigation> AbstractFish::createNavigation() { return std::make_unique<WaterBoundPathNavigation>(*this, _level); }

void AbstractFish::setFromBucket(bool from) {
	if (from == _fromBucket) return;
	_fromBucket = from;
	markData(DATA_FROM_BUCKET);
}

void AbstractFish::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<PanicGoal>(*this, 1.25));
	// EntitySelector.NO_SPECTATORS: creative players too
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Player", 8.0F, 1.6, 1.4, nullptr, [](Actor& actor) { return !actor.isSpectator(); }));
	_goalSelector.addGoal(4, std::make_unique<FishSwimGoal>(*this));
}

void AbstractFish::checkDespawn() {
	if (_fromBucket) {
		_noActionTime = 0; // requiresCustomPersistence
		return;
	}
	WaterAnimal::checkDespawn();
}

void AbstractFish::travel(const Vec3& input) {
	if (!isInWater()) {
		WaterAnimal::travel(input);
		return;
	}
	moveRelative(0.01F, input);
	move(_delta);
	_delta = _delta.scale(0.9);
	if (!getTarget()) _delta.y -= 0.005;
}

void AbstractFish::aiStep() {
	// On land: it flops
	if (!isInWater() && _onGround && _verticalCollision) {
		_delta	   = _delta + Vec3{(_random.nextFloat() * 2.0F - 1.0F) * 0.05F, 0.4F, (_random.nextFloat() * 2.0F - 1.0F) * 0.05F};
		_onGround  = false;
		hasImpulse = true;
		playSound(typeSound("flop", ""), soundVolume(), voicePitch());
	}
	WaterAnimal::aiStep();
}

std::string AbstractFish::bucketItem() const {
	const std::string& type = _level.gameData().getStaticName("minecraft:entity_type", _typeId);
	return type + "_bucket";
}

bool AbstractFish::mobInteract(Player& player, int hand) {
	// Bucketable.bucketMobPickup: into a water bucket (its own data isn't kept in the bucket yet)
	const ItemStack& stack = player.getStackInHand(hand);
	const GameData&	 data  = _level.gameData();
	if (stack.isEmpty() || stack.item != data.getStaticId("minecraft:item", "minecraft:water_bucket") || !isAlive()) return WaterAnimal::mobInteract(player, hand);
	int bucket = data.getStaticId("minecraft:item", bucketItem());
	if (bucket < 0) return WaterAnimal::mobInteract(player, hand);
	playSound("minecraft:item.bucket.fill_fish", 1.0F, 1.0F);
	ItemStack held = ItemUse::filledResult(_level, player, stack, ItemStack(bucket, 1));
	player.inventory().set(player.handSlot(hand), std::move(held));
	discard();
	return true;
}

void AbstractFish::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	WaterAnimal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FROM_BUCKET, onlyNonDefault, !_fromBucket)) writeBoolData(buf, DATA_FROM_BUCKET, _fromBucket);
}

void AbstractFish::save(Buffer& buf) const {
	WaterAnimal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_fromBucket);
}

void AbstractFish::load(Buffer& buf) {
	WaterAnimal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown fish data version");
	_fromBucket = buf.readBool();
	_dirtyData	= 0;
}

// ===================== AbstractSchoolingFish =====================

void AbstractSchoolingFish::registerGoals() {
	AbstractFish::registerGoals();
	_goalSelector.addGoal(5, std::make_unique<FollowFlockLeaderGoal>(*this));
}

AbstractSchoolingFish* AbstractSchoolingFish::leader() {
	if (!_leader.isSet()) return nullptr;
	auto* fish = dynamic_cast<AbstractSchoolingFish*>(_level.actorByRef(_leader));
	return fish && fish->isAlive() ? fish : nullptr;
}

bool AbstractSchoolingFish::isFollower() { return leader() != nullptr; }

void AbstractSchoolingFish::startFollowing(AbstractSchoolingFish& leader) {
	_leader = EntityRef(leader);
	leader._schoolSize++;
}

void AbstractSchoolingFish::stopFollowing() {
	if (AbstractSchoolingFish* fish = dynamic_cast<AbstractSchoolingFish*>(_leader.isSet() ? _level.actorByRef(_leader) : nullptr)) fish->_schoolSize--;
	_leader.clear();
}

bool AbstractSchoolingFish::inRangeOfLeader() {
	AbstractSchoolingFish* fish = leader();
	return fish && distanceToSqr(*fish) <= 121.0;
}

void AbstractSchoolingFish::pathToLeader() {
	if (AbstractSchoolingFish* fish = leader()) navigation().moveTo(*fish, 1.0);
}

void AbstractSchoolingFish::addFollowers(const std::vector<AbstractSchoolingFish*>& fish) {
	int room = maxSchoolSize() - _schoolSize;
	for (AbstractSchoolingFish* other : fish) {
		if (room-- <= 0) break; // Stream.limit, then the filter
		if (other != this) other->startFollowing(*this);
	}
}

void AbstractSchoolingFish::tick() {
	AbstractFish::tick();
	if (isRemoved()) return;
	// A leader left alone forgets its school
	if (hasFollowers() && _level.random().nextInt(200) == 1) {
		int around = 0;
		_level.entities().forEachIn(boundingBox().inflate(8.0), [&](Entity& entity) { around += !entity.isRemoved() && entity.typeId() == _typeId; });
		if (around <= 1) _schoolSize = 1;
	}
}

std::shared_ptr<SpawnGroupData> AbstractSchoolingFish::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	AbstractFish::finalizeSpawn(difficulty, reason, group);
	if (!group) {
		auto school	   = std::make_shared<SchoolSpawnGroupData>();
		school->leader = EntityRef(*this);
		return school;
	}
	if (auto* school = dynamic_cast<SchoolSpawnGroupData*>(group.get())) {
		// The leader isn't in the level yet when its school spawns with it: found once it is
		_leader = school->leader;
	}
	return group;
}

// ===================== Salmon =====================

void Salmon::setVariant(int variant) {
	variant = std::clamp(variant, 0, 2);
	float scale = variant == 0 ? 0.5F : variant == 2 ? 1.5F : 1.0F;
	setDimensions(_type.width * scale, _type.height * scale, _type.eyeHeight * scale);
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_TYPE);
}

std::shared_ptr<SpawnGroupData> Salmon::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	// WeightedList: small 30, medium 50, large 15
	int roll = _random.nextInt(95);
	setVariant(roll < 30 ? 0 : roll < 80 ? 1 : 2);
	return AbstractSchoolingFish::finalizeSpawn(difficulty, reason, group);
}

void Salmon::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractSchoolingFish::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_TYPE, onlyNonDefault, _variant == 1)) writeIntData(buf, DATA_TYPE, _variant);
}

void Salmon::save(Buffer& buf) const {
	AbstractSchoolingFish::save(buf);
	buf.writeUByte(static_cast<uint8_t>(_variant));
}

void Salmon::load(Buffer& buf) {
	AbstractSchoolingFish::load(buf);
	setVariant(buf.readUByte());
	_dirtyData = 0;
}

// ===================== TropicalFish =====================

void TropicalFish::setPackedVariant(int variant) {
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_VARIANT);
}

std::shared_ptr<SpawnGroupData> TropicalFish::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group				  = AbstractSchoolingFish::finalizeSpawn(difficulty, reason, group);
	JavaRandom& random	  = _level.random();
	auto*		school	  = dynamic_cast<SchoolSpawnGroupData*>(group.get());
	int			variant;
	if (school && school->variant != -1) {
		variant = school->variant;
	} else if (random.nextFloat() < 0.9) {
		variant = COMMON_VARIANTS[random.nextInt(22)];
		if (school) school->variant = variant;
	} else {
		int pattern = PATTERNS[random.nextInt(12)];
		int base	= random.nextInt(16);
		variant		= pack(pattern, base, random.nextInt(16));
	}
	setPackedVariant(variant);
	return group;
}

void TropicalFish::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractSchoolingFish::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_VARIANT, onlyNonDefault, _variant == 0)) writeIntData(buf, DATA_VARIANT, _variant);
}

void TropicalFish::save(Buffer& buf) const {
	AbstractSchoolingFish::save(buf);
	buf.writeInt(_variant);
}

void TropicalFish::load(Buffer& buf) {
	AbstractSchoolingFish::load(buf);
	_variant   = buf.readInt();
	_dirtyData = 0;
}

// ===================== Pufferfish =====================

Pufferfish::Pufferfish(Level& level, int typeId) : AbstractFish(level, typeId) { setPuffState(0); }

void Pufferfish::setPuffState(int state) {
	float scale = state == 0 ? 0.5F : state == 1 ? 0.7F : 1.0F;
	setDimensions(_type.width * scale, _type.height * scale, _type.eyeHeight * scale);
	if (state == _puffState) return;
	_puffState = state;
	markData(DATA_PUFF_STATE);
}

void Pufferfish::registerGoals() {
	AbstractFish::registerGoals();
	_goalSelector.addGoal(1, std::make_unique<PufferfishPuffGoal>(*this));
}

bool Pufferfish::scaryNearby(double range) {
	// SCARY_MOB: not a creative player, not one of #not_scary_for_pufferfish; no line of sight needed
	AABB			area  = boundingBox().inflate(range);
	const GameData& data  = _level.gameData();
	bool			scary = false;
	for (const auto& player : _level.players()) {
		if (player->isSpectator() || player->combat().dead || player->isCreative()) continue;
		scary |= player->boundingBox().intersects(area);
	}
	_level.entities().forEachIn(area, [&](Entity& entity) {
		LivingEntity* living = entity.asLiving();
		if (!living || living == this || !living->isAlive()) return;
		scary |= !data.isInTag("minecraft:entity_type", "minecraft:not_scary_for_pufferfish", entity.typeId());
	});
	return scary;
}

void Pufferfish::tick() {
	if (isAlive() && isEffectiveAi()) {
		if (_inflateCounter > 0) {
			if (_puffState == 0) {
				playSound("minecraft:entity.puffer_fish.blow_up", soundVolume(), voicePitch());
				setPuffState(1);
			} else if (_inflateCounter > 40 && _puffState == 1) {
				playSound("minecraft:entity.puffer_fish.blow_up", soundVolume(), voicePitch());
				setPuffState(2);
			}
			_inflateCounter++;
		} else if (_puffState != 0) {
			if (_deflateTimer > 60 && _puffState == 2) {
				playSound("minecraft:entity.puffer_fish.blow_out", soundVolume(), voicePitch());
				setPuffState(1);
			} else if (_deflateTimer > 100 && _puffState == 1) {
				playSound("minecraft:entity.puffer_fish.blow_out", soundVolume(), voicePitch());
				setPuffState(0);
			}
			_deflateTimer++;
		}
	}
	AbstractFish::tick();
}

void Pufferfish::aiStep() {
	AbstractFish::aiStep();
	if (!isAlive() || _puffState <= 0) return;
	// It stings the mobs that touch it
	std::vector<Mob*> touched;
	const GameData&	  data = _level.gameData();
	_level.entities().forEachIn(boundingBox().inflate(0.3), [&](Entity& entity) {
		auto* mob = dynamic_cast<Mob*>(&entity);
		if (mob && mob != this && mob->isAlive() && !data.isInTag("minecraft:entity_type", "minecraft:not_scary_for_pufferfish", mob->typeId())) touched.push_back(mob);
	});
	for (Mob* mob : touched) {
		if (mob->hurtServer({"minecraft:mob_attack", this, nullptr, std::nullopt}, static_cast<float>(1 + _puffState))) {
			mob->addEffect(MobEffects::Instance(MobEffects::Poison, 60 * _puffState, 0), this);
			playSound("minecraft:entity.puffer_fish.sting", 1.0F, 1.0F);
		}
	}
}

int Pufferfish::touchedByPlayer(Player& player) {
	if (_puffState <= 0) return 0;
	if (Combat::damage(_level.server(), player, static_cast<float>(1 + _puffState), {"minecraft:mob_attack", this, nullptr, std::nullopt})) {
		Buffer event;
		event.writeUByte(GAME_EVENT_PUFFER_FISH_STING);
		event.writeFloat(0.0F);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::GAME_EVENT, event, _level.server());
		player.addEffect(MobEffects::Instance(MobEffects::Poison, 60 * _puffState, 0), this);
	}
	return 0;
}

void Pufferfish::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractFish::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_PUFF_STATE, onlyNonDefault, _puffState == 0)) writeIntData(buf, DATA_PUFF_STATE, _puffState);
}

void Pufferfish::save(Buffer& buf) const {
	AbstractFish::save(buf);
	buf.writeUByte(static_cast<uint8_t>(_puffState));
}

void Pufferfish::load(Buffer& buf) {
	AbstractFish::load(buf);
	setPuffState(std::min<int>(buf.readUByte(), 2));
	_dirtyData = 0;
}
