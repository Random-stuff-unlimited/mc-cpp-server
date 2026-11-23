#include "world/entity/mobs/FlyingAnimals.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/RandomPos.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace {
	constexpr int SAVE_VERSION			  = 1;
	constexpr int LEVEL_EVENT_BEE_GROWTH = 2011; // The bone meal particles of the crops bees grow

	bool isItemIn(Level& level, const ItemStack& stack, const char* tag) {
		return !stack.isEmpty() && level.gameData().isInTag("minecraft:item", tag, stack.item);
	}
	std::string blockName(Level& level, const BlockPos& pos) {
		return level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(level.getBlockState(pos)));
	}

	// WaterAvoidingRandomFlyingGoal (and Bee.BeeWanderGoal's positions): somewhere ahead in the air, or back to land from
	// the water
	std::optional<Vec3> randomFlyingPos(Mob& mob) {
		if (mob.isInWaterNow()) return RandomPos::landPos(mob, 15, 15);
		Vec3				view = viewVectorOf(mob);
		std::optional<Vec3> pos	 = RandomPos::hoverPos(mob, 8, 7, view.x, view.z, (float)M_PI / 2.0F, 3, 1);
		return pos ? pos : RandomPos::airAndWaterPos(mob, 8, 4, -2, view.x, view.z, M_PI / 2.0);
	}

	class WaterAvoidingRandomFlyingGoal : public WaterAvoidingRandomStrollGoal {
	  public:
		WaterAvoidingRandomFlyingGoal(Mob& mob, double speed) : WaterAvoidingRandomStrollGoal(mob, speed) {}

	  protected:
		std::optional<Vec3> getPosition() override { return randomFlyingPos(_mob); }
	};

	// The hostile mobs a parrot imitates (the entity.parrot.imitate.* sounds)
	const std::unordered_set<std::string> IMITATED = {"blaze",	   "bogged",	"breeze",	   "creaking",	  "creeper",	 "drowned",	 "elder_guardian",
													  "ender_dragon", "endermite", "evoker",	   "ghast",		  "guardian",	 "hoglin",	 "husk",
													  "illusioner", "magma_cube", "phantom",	   "piglin",	  "piglin_brute", "pillager", "ravager",
													  "shulker",	 "silverfish", "skeleton",	   "slime",		  "spider",		 "stray",	 "vex",
													  "vindicator", "warden",	"witch",	   "wither",	  "wither_skeleton", "zoglin", "zombie",
													  "zombie_villager"};

	// ----- Bee -----

	// Bee.BeePollinateGoal: flies to a flower nearby and hovers over it until it has nectar
	class BeePollinateGoal : public Goal {
	  public:
		explicit BeePollinateGoal(Bee& bee) : _bee(bee) { setFlags({Flag::Move}); }
		bool canUse() override {
			if (_bee.hasNectar() || _bee.isAngry() || _cooldown-- > 0) return false;
			if (_bee.random().nextInt(reducedTickDelay(10)) != 0) return false;
			std::optional<BlockPos> flower = _bee.hasSavedFlower() && _bee.isFlower(_bee.savedFlower())
												 ? std::optional<BlockPos>(_bee.savedFlower())
												 : findClosestMatch(_bee.blockPosition(), 5, 5, [this](const BlockPos& pos) { return _bee.isFlower(pos); });
			if (!flower) {
				_cooldown = 60;
				return false;
			}
			_flower = *flower;
			_bee.setSavedFlower(_flower);
			return true;
		}
		bool canContinueToUse() override { return !_bee.hasNectar() && !_bee.isAngry() && _bee.isFlower(_flower) && _ticks < 600; }
		void start() override {
			_ticks = 0, _pollinating = 0;
			_bee.navigation().moveTo(_flower.x + 0.5, _flower.y + 0.6, _flower.z + 0.5, 1.2);
		}
		void stop() override {
			if (_pollinating >= 400) _bee.setHasNectar(true);
			_bee.navigation().stop();
			_cooldown = 200;
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			_ticks++;
			Vec3   hover{_flower.x + 0.5, _flower.y + 0.6, _flower.z + 0.5};
			Vec3   to	   = hover - _bee.position();
			double distSqr = to.lengthSqr();
			if (distSqr > 1.0) {
				if (_bee.navigation().isDone()) _bee.navigation().moveTo(hover.x, hover.y, hover.z, 1.2);
				return;
			}
			// Over the flower: it hovers, moving a little now and then
			_pollinating++;
			if (_pollinating % 20 == 0) {
				JavaRandom& random = _bee.random();
				_offset			   = {random.nextFloat() * 0.66 - 0.33, 0.0, random.nextFloat() * 0.66 - 0.33};
				if (random.nextFloat() < 0.05F) _bee.playSound("minecraft:entity.bee.pollinate", 1.0F, 1.0F);
			}
			_bee.moveControl().setWantedPosition(hover.x + _offset.x, hover.y, hover.z + _offset.z, 0.35);
			if (_pollinating >= 400) _ticks = 600; // Done: stop() gives the nectar
		}

	  private:
		Bee&	 _bee;
		BlockPos _flower;
		Vec3	 _offset;
		int		 _ticks = 0, _pollinating = 0, _cooldown = 0;
	};

	// Bee.BeeLocateHiveGoal: a beehive or bee nest within 20 blocks, looked for every 10 seconds while it has none
	class BeeLocateHiveGoal : public Goal {
	  public:
		explicit BeeLocateHiveGoal(Bee& bee) : _bee(bee) {}
		bool canUse() override {
			if (_bee.hasHive() && _bee.isHive(_bee.hivePos())) return false;
			_bee.clearHive();
			if (--_cooldown > 0) return false;
			_cooldown = reducedTickDelay(200);
			std::optional<BlockPos> hive = findClosestMatch(_bee.blockPosition(), 20, 8, [this](const BlockPos& pos) { return _bee.isHive(pos); });
			if (hive) _bee.setHive(*hive);
			return false;
		}

	  private:
		Bee& _bee;
		int	 _cooldown = 0;
	};

	// Bee.BeeGoToHiveGoal and BeeEnterHiveGoal: with nectar, back to its hive, where the nectar becomes honey
	class BeeGoToHiveGoal : public Goal {
	  public:
		explicit BeeGoToHiveGoal(Bee& bee) : _bee(bee) { setFlags({Flag::Move}); }
		bool canUse() override { return _bee.hasNectar() && _bee.hasHive() && !_bee.isAngry() && _bee.isHive(_bee.hivePos()); }
		bool canContinueToUse() override { return canUse() && _ticks < 1200; }
		void start() override { _ticks = 0; }
		void stop() override { _bee.navigation().stop(); }
		void tick() override {
			_ticks++;
			const BlockPos& hive = _bee.hivePos();
			Vec3			center{hive.x + 0.5, hive.y + 0.5, hive.z + 0.5};
			if ((center - _bee.position()).lengthSqr() <= 4.0) {
				_bee.dropOffNectar();
				return;
			}
			if (_bee.navigation().isDone()) _bee.navigation().moveTo(center.x, center.y + 1.0, center.z, 1.0);
		}

	  private:
		Bee& _bee;
		int	 _ticks = 0;
	};

	// Bee.BeeGrowCropGoal: with nectar, the crops right under it grow a stage now and then (ten at most)
	class BeeGrowCropGoal : public Goal {
	  public:
		explicit BeeGrowCropGoal(Bee& bee) : _bee(bee) {}
		bool canUse() override {
			return _bee.hasNectar() && _bee.cropsGrownSincePollination < 10 && !_bee.isAngry() && _bee.random().nextFloat() >= 0.3F &&
				   _bee.random().nextInt(reducedTickDelay(30)) == 0;
		}
		bool canContinueToUse() override { return false; }
		void start() override {
			Level& level = _bee.level();
			for (int i = 1; i <= 2; i++) {
				BlockPos		pos	  = _bee.blockPosition().offset(0, -i, 0);
				int				state = level.getBlockState(pos);
				const GameData& data  = level.gameData();
				if (!data.isInTag("minecraft:block", "minecraft:bee_growables", level.blocks().blockOf(state))) continue;
				std::string age = data.getProperty(state, "age");
				if (age.empty()) continue;
				int next = data.withProperty(state, "age", std::to_string(std::stoi(age) + 1));
				if (next < 0 || next == state) continue;
				level.levelEvent(nullptr, LEVEL_EVENT_BEE_GROWTH, pos, 15);
				level.setBlock(pos, next, Level::UPDATE_ALL);
				_bee.cropsGrownSincePollination++;
				break;
			}
		}

	  private:
		Bee& _bee;
	};

	// Bee.BeeWanderGoal: around its hive (back toward it when far), else anywhere
	class BeeWanderGoal : public Goal {
	  public:
		explicit BeeWanderGoal(Bee& bee) : _bee(bee) { setFlags({Flag::Move}); }
		bool canUse() override { return _bee.navigation().isDone() && _bee.random().nextInt(10) == 0; }
		bool canContinueToUse() override { return _bee.navigation().isInProgress(); }
		void start() override {
			std::optional<Vec3> pos;
			if (_bee.hasHive() && (Vec3{_bee.hivePos().x + 0.5, _bee.hivePos().y + 0.5, _bee.hivePos().z + 0.5} - _bee.position()).lengthSqr() > 22.0 * 22.0) {
				const BlockPos& hive = _bee.hivePos();
				pos					 = RandomPos::airPosTowards(_bee, 8, 7, 0, {hive.x + 0.5, hive.y + 0.5, hive.z + 0.5}, M_PI / 2.0);
			}
			if (!pos) pos = randomFlyingPos(_bee);
			if (pos) _bee.navigation().moveTo(pos->x, pos->y, pos->z, 1.0);
		}

	  private:
		Bee& _bee;
	};
} // namespace

Vec3 viewVectorOf(const Mob& mob) {
	// Entity.calculateViewVector
	float pitch = mob.xRot() * (float)M_PI / 180.0F, yaw = -mob.yRot() * (float)M_PI / 180.0F;
	float cosYaw = Mth::cos(yaw), sinYaw = Mth::sin(yaw), cosPitch = Mth::cos(pitch), sinPitch = Mth::sin(pitch);
	return {sinYaw * cosPitch, -sinPitch, cosYaw * cosPitch};
}

// ===================== Parrot =====================

Parrot::Parrot(Level& level, int typeId) : TamableAnimal(level, typeId) {
	setMoveControl(std::make_unique<FlyingMoveControl>(*this, 10, false));
	setPathfindingMalus(PathType::DangerFire, -1.0F);
	setPathfindingMalus(PathType::DamageFire, -1.0F);
	setPathfindingMalus(PathType::Cocoa, -1.0F);
}

std::unique_ptr<PathNavigation> Parrot::createNavigation() {
	auto navigation = std::make_unique<FlyingPathNavigation>(*this, _level);
	navigation->setCanOpenDoors(false);
	navigation->setCanFloat(true);
	return navigation;
}

void Parrot::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<TamableAnimalPanicGoal>(*this, 1.25));
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(2, std::make_unique<SitWhenOrderedToGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<FollowOwnerGoal>(*this, 1.0, 5.0F, 1.0F));
	_goalSelector.addGoal(2, std::make_unique<WaterAvoidingRandomFlyingGoal>(*this, 1.0));
}

void Parrot::setVariant(int variant) {
	variant = ((variant % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT;
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_VARIANT);
}

void Parrot::aiStep() {
	TamableAnimal::aiStep();
	// Parrot.calculateFlapping: it flutters down
	if (!_onGround && _delta.y < 0.0) _delta.y *= 0.6;
}

bool Parrot::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (!isTame() && isItemIn(_level, stack, "minecraft:parrot_food")) {
		usePlayerItem(player, hand);
		playSound("minecraft:entity.parrot.eat", 1.0F, 1.0F + (_random.nextFloat() - _random.nextFloat()) * 0.2F);
		if (_random.nextInt(10) == 0) {
			tame(player);
			broadcastEntityEvent(EVENT_TAMING_SUCCEEDED);
		} else {
			broadcastEntityEvent(EVENT_TAMING_FAILED);
		}
		return true;
	}
	if (isItemIn(_level, stack, "minecraft:parrot_poisonous_food")) {
		// A cookie: poisoned, and killed outright
		usePlayerItem(player, hand);
		addEffect({MobEffects::Poison, 900, 0});
		hurtServer({"minecraft:player_attack", &player, nullptr, std::nullopt}, 3.4028235E38F);
		return true;
	}
	if (!isFlying() && isTame() && isOwnedBy(player)) {
		setOrderedToSit(!isOrderedToSit());
		return true;
	}
	return TamableAnimal::mobInteract(player, hand);
}

bool Parrot::hurtServer(const Combat::DamageSource& source, float amount) {
	setOrderedToSit(false);
	return TamableAnimal::hurtServer(source, amount);
}

std::shared_ptr<SpawnGroupData> Parrot::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	setVariant(_level.random().nextInt(VARIANT_COUNT));
	if (!group) group = std::make_shared<AgeableMobGroupData>(false, 0.0F);
	return TamableAnimal::finalizeSpawn(difficulty, reason, group);
}

std::string Parrot::ambientSoundName() {
	// Parrot.getAmbient: now and then, one of the hostile mobs around (not silent)
	if (_random.nextInt(1000) == 0) {
		std::vector<std::string> sounds;
		for (Actor* actor : _level.actorsIn(boundingBox().inflate(20.0), this)) {
			if (!actor->asLiving()) continue;
			std::string name = _level.gameData().getStaticName("minecraft:entity_type", actor->typeId());
			name			 = name.substr(name.find(':') + 1);
			if (IMITATED.count(name)) sounds.push_back("minecraft:entity.parrot.imitate." + name);
		}
		if (!sounds.empty()) return sounds[_random.nextInt(static_cast<int>(sounds.size()))];
	}
	return "minecraft:entity.parrot.ambient";
}

void Parrot::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	TamableAnimal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_VARIANT, onlyNonDefault, _variant == 0)) writeIntData(buf, DATA_VARIANT, _variant);
}

void Parrot::save(Buffer& buf) const {
	TamableAnimal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_variant);
}

void Parrot::load(Buffer& buf) {
	TamableAnimal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown parrot data version");
	setVariant(buf.readInt());
}

// ===================== Bee =====================

Bee::Bee(Level& level, int typeId) : Animal(level, typeId), _anger(*this, 20, 39) {
	setMoveControl(std::make_unique<FlyingMoveControl>(*this, 20, true));
	setPathfindingMalus(PathType::DangerFire, -1.0F);
	setPathfindingMalus(PathType::Water, -1.0F);
	setPathfindingMalus(PathType::WaterBorder, 16.0F);
	setPathfindingMalus(PathType::Cocoa, -1.0F);
	setPathfindingMalus(PathType::Fence, -1.0F);
}

std::unique_ptr<PathNavigation> Bee::createNavigation() {
	auto navigation = std::make_unique<FlyingPathNavigation>(*this, _level);
	navigation->setCanOpenDoors(false);
	navigation->setCanFloat(false);
	return navigation;
}

void Bee::registerGoals() {
	// The attack (a melee charge, BeeAttackGoal), then the bee's business
	_goalSelector.addGoal(0, std::make_unique<MeleeAttackGoal>(*this, 1.4, true));
	_goalSelector.addGoal(1, std::make_unique<BeeGoToHiveGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.25, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(3, std::make_unique<BeeLocateHiveGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<BeePollinateGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(7, std::make_unique<BeeGrowCropGoal>(*this));
	_goalSelector.addGoal(8, std::make_unique<BeeWanderGoal>(*this));
	_goalSelector.addGoal(9, std::make_unique<FloatGoal>(*this));
	// BeeHurtByOtherGoal: the bees around get angry too; BeeBecomeAngryTargetGoal: the player it is angry at
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																			 [this](Actor& target, Level&) { return !hasStung() && _anger.isAngryAt(target); }));
}

void Bee::setFlag(int flag, bool value) {
	uint8_t flags = static_cast<uint8_t>(value ? _beeFlags | flag : _beeFlags & ~flag);
	if (flags == _beeFlags) return;
	_beeFlags = flags;
	markData(DATA_FLAGS);
}

void Bee::setHasNectar(bool nectar) {
	if (nectar) ticksWithoutNectarSinceExitingHive = 0;
	setFlag(FLAG_HAS_NECTAR, nectar);
}

bool Bee::isFood(const ItemStack& stack) const { return isItemIn(_level, stack, "minecraft:bee_food"); }

bool Bee::isFlower(const BlockPos& pos) {
	int block = _level.blocks().blockOf(_level.getBlockState(pos));
	return _level.gameData().isInTag("minecraft:block", "minecraft:bee_attractive", block);
}

bool Bee::isHive(const BlockPos& pos) {
	std::string name = blockName(_level, pos);
	return name == "minecraft:beehive" || name == "minecraft:bee_nest";
}

void Bee::dropOffNectar() {
	setHasNectar(false);
	cropsGrownSincePollination = 0;
	if (!_hasHive || !isHive(_hivePos)) return;
	// BeehiveBlockEntity.addOccupant then releaseOccupant: a honey level more (5 at most)
	int				state = _level.getBlockState(_hivePos);
	const GameData& data  = _level.gameData();
	std::string		level = data.getProperty(state, "honey_level");
	if (!level.empty() && std::stoi(level) < 5) {
		int next = data.withProperty(state, "honey_level", std::to_string(std::stoi(level) + 1));
		if (next >= 0) _level.setBlock(_hivePos, next, Level::UPDATE_ALL);
	}
	_level.playSound(nullptr, _hivePos, "minecraft:block.beehive.enter", Level::SoundSource::Blocks, 1.0F, 1.0F);
}

void Bee::setTarget(Actor* target) {
	Animal::setTarget(target);
	if (target && target->asPlayer() && !_anger.isAngry()) {
		// NeutralMob: angry at the player it targets for 20 to 39 seconds
		_anger.setTarget(&target->uuid());
		_anger.startTimer();
	}
}

bool Bee::hurtServer(const Combat::DamageSource& source, float amount) {
	bool hurt = Animal::hurtServer(source, amount);
	if (hurt && source.causing && source.causing != static_cast<Actor*>(this)) {
		if (source.causing->asPlayer() && !hasStung()) setTarget(source.causing);
	}
	return hurt;
}

bool Bee::doHurtTarget(Actor& target) {
	// Bee.doHurtTarget: a sting, with poison (10 seconds, 18 in hard), and then it has no stinger
	float				 damage = static_cast<float>(getAttributeValue(_ids.attackDamage));
	Combat::DamageSource source{"minecraft:sting", this, nullptr, std::nullopt};
	bool				 hurt = target.asPlayer() ? Combat::damage(_level.server(), *target.asPlayer(), damage, source) : target.hurtServer(source, damage);
	if (!hurt) return false;
	setLastHurtMob(&target);
	int seconds = _level.difficulty() == 2 ? 10 : _level.difficulty() == 3 ? 18 : 0;
	if (seconds > 0) {
		MobEffects::Instance poison{MobEffects::Poison, seconds * 20, 0};
		if (Player* player = target.asPlayer()) {
			player->addEffect(poison);
		} else if (LivingEntity* living = target.asLiving()) {
			living->addEffect(poison);
		}
	}
	setFlag(FLAG_HAS_STUNG, true);
	_anger.stopBeingAngry();
	setTarget(nullptr);
	playSound("minecraft:entity.bee.sting", 1.0F, 1.0F);
	return true;
}

void Bee::customServerAiStep() {
	Animal::customServerAiStep();
	// Under water for a second, it drowns
	_underWaterTicks = isInWaterNow() ? _underWaterTicks + 1 : 0;
	if (_underWaterTicks > 20) hurtServer({"minecraft:drown", nullptr, nullptr, std::nullopt}, 1.0F);
	// Without its stinger, it dies within a minute
	if (hasStung()) {
		_timeSinceSting++;
		if (_timeSinceSting % 5 == 0 && _random.nextInt(std::max(1, std::min(1200, 1200 - _timeSinceSting))) == 0) {
			hurtServer({"minecraft:generic", nullptr, nullptr, std::nullopt}, _health);
		}
	}
	if (!hasNectar()) ticksWithoutNectarSinceExitingHive++;
	_anger.update(false);
}

std::shared_ptr<SpawnGroupData> Bee::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	return Animal::finalizeSpawn(difficulty, reason, group);
}

void Bee::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _beeFlags == 0)) writeByteData(buf, DATA_FLAGS, _beeFlags);
}

void Bee::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(_beeFlags);
	buf.writeBool(_hasHive);
	buf.writeLong(_hivePos.asLong());
	buf.writeBool(_hasSavedFlower);
	buf.writeLong(_savedFlower.asLong());
	buf.writeInt(_timeSinceSting);
	buf.writeInt(cropsGrownSincePollination);
	_anger.save(buf);
}

void Bee::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown bee data version");
	_beeFlags					= buf.readUByte();
	_hasHive					= buf.readBool();
	_hivePos					= BlockPos::fromLong(buf.readLong());
	_hasSavedFlower				= buf.readBool();
	_savedFlower				= BlockPos::fromLong(buf.readLong());
	_timeSinceSting				= buf.readInt();
	cropsGrownSincePollination = buf.readInt();
	_anger.load(buf);
}
