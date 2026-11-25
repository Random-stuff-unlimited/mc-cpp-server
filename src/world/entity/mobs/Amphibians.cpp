#include "world/entity/ai/LongJumpGoal.hpp"
#include "world/entity/mobs/Amphibians.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Fluids.hpp"
#include "world/Level.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/Variants.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/Slime.hpp"
#include "world/item/ItemUse.hpp"

#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION			  = 1;
	constexpr int SERIALIZER_FROG_VARIANT = 25;

	bool isItem(Level& level, const ItemStack& stack, const char* name) {
		return !stack.isEmpty() && stack.item == level.gameData().getStaticId("minecraft:item", name);
	}
	ItemStack itemOf(Level& level, const char* name) { return ItemStack(level.gameData().getStaticId("minecraft:item", name), 1); }
	bool	  isWaterAt(Level& level, const BlockPos& pos) { return level.fluids().isWater(level.getFluidState(pos).type); }

	// Bucketable.bucketMobPickup: a water bucket takes the mob
	bool bucketPickup(Mob& mob, Player& player, int hand, const char* bucketItem) {
		Level&			 level = mob.level();
		const ItemStack& stack = player.getStackInHand(hand);
		if (!isItem(level, stack, "minecraft:water_bucket") || !mob.isAlive()) return false;
		mob.level().playSoundAt(nullptr, mob.position().x, mob.position().y, mob.position().z, "minecraft:item.bucket.fill_axolotl", Level::SoundSource::Neutral,
								1.0F, 1.0F);
		ItemStack held = ItemUse::filledResult(level, player, stack, itemOf(level, bucketItem));
		player.inventory().set(player.handSlot(hand), std::move(held));
		mob.discard();
		return true;
	}

	// ----- Frog -----

	// The frog's tongue (FrogAi: ShootTongue): walks up to its prey, sticks its tongue out at it from 3 blocks and eats it
	class FrogTongueGoal : public Goal {
	  public:
		explicit FrogTongueGoal(Frog& frog) : _frog(frog) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			Actor* target = _frog.getTarget();
			return target && target->isAlive() && _frog.canEat(*target);
		}
		bool canContinueToUse() override { return _tongueTicks > 0 || canUse(); }
		void start() override { _tongueTicks = 0; }
		void stop() override {
			_tongueTicks = 0;
			_frog.setPose(LivingEntity::POSE_STANDING);
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _frog.getTarget();
			if (_tongueTicks > 0) {
				// The tongue is out: the prey is eaten on its sixth tick
				if (++_tongueTicks == 6 && target && target->isAlive()) _frog.eat(*target);
				if (_tongueTicks >= 10) {
					_tongueTicks = 0;
					_frog.setPose(LivingEntity::POSE_STANDING);
					_frog.setTarget(nullptr);
				}
				return;
			}
			if (!target) return;
			_frog.lookControl().setLookAt(*target, 30.0F, 30.0F);
			if (_frog.distanceToSqr(*target) <= 3.25 * 3.25 && _frog.sensing().hasLineOfSight(*target)) {
				_frog.navigation().stop();
				_frog.setPose(Frog::POSE_USING_TONGUE);
				_frog.playSound("minecraft:entity.frog.tongue", 2.0F, 1.0F);
				_tongueTicks = 1;
			} else if (--_pathDelay <= 0) {
				_pathDelay = 10;
				_frog.navigation().moveTo(*target, 1.75);
			}
		}

	  private:
		Frog& _frog;
		int	  _tongueTicks = 0, _pathDelay = 0;
	};

	// FrogAi's TryLaySpawnOnWaterNearLand: with an egg, it walks to the shore and lays frogspawn on the water next to it
	class FrogLayEggGoal : public MoveToBlockGoal {
	  public:
		explicit FrogLayEggGoal(Frog& frog) : MoveToBlockGoal(frog, 1.0, 8), _frog(frog) {}
		bool canUse() override { return _frog.hasEgg() && MoveToBlockGoal::canUse(); }
		bool canContinueToUse() override { return _frog.hasEgg() && MoveToBlockGoal::canContinueToUse(); }
		void tick() override {
			MoveToBlockGoal::tick();
			if (!isReachedTarget() || !_frog.onGround() || _frog.isInWaterNow()) return;
			if (std::optional<BlockPos> water = waterNextTo(_frog.blockPosition().below())) {
				Level& level = _frog.level();
				level.setBlock(water->above(), level.gameData().getDefaultBlockState("minecraft:frogspawn"), Level::UPDATE_ALL);
				_frog.playSound("minecraft:entity.frog.lay_spawn", 1.0F, 1.0F);
				_frog.setHasEgg(false);
			}
		}

	  protected:
		int nextStartTick() override { return reducedTickDelay(40); }
		// A block to stand on with water beside it (open above)
		bool isValidTarget(Level& level, const BlockPos& pos) override {
			return !level.blocks().isAir(level.getBlockState(pos)) && !isWaterAt(level, pos) && level.blocks().isAir(level.getBlockState(pos.above())) &&
				   waterNextTo(pos).has_value();
		}

	  private:
		Frog& _frog;
		std::optional<BlockPos> waterNextTo(const BlockPos& ground) {
			Level& level = _frog.level();
			for (Direction direction : {Direction::North, Direction::South, Direction::West, Direction::East}) {
				BlockPos side = ground.relative(direction);
				if (isWaterAt(level, side) && level.gameData().getCollisionShape(level.getBlockState(side)).empty() &&
					level.blocks().isAir(level.getBlockState(side.above()))) {
					return side;
				}
			}
			return std::nullopt;
		}
	};

	// ----- Axolotl -----

	// The axolotl's play dead (AxolotlAi's PlayDead): it stops, rolls over and heals; nothing else runs meanwhile
	class AxolotlPlayDeadGoal : public Goal {
	  public:
		explicit AxolotlPlayDeadGoal(Axolotl& axolotl) : _axolotl(axolotl) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return _axolotl.isPlayingDead(); }
		bool isInterruptable() override { return false; }
		void start() override {
			_axolotl.navigation().stop();
			_axolotl.setTarget(nullptr);
			_axolotl.addEffect({MobEffects::Regeneration, Axolotl::PLAY_DEAD_TICKS, 0});
		}

	  private:
		Axolotl& _axolotl;
	};
} // namespace

// ===================== Frog =====================

Frog::Frog(Level& level, int typeId) : VariantAnimal(level, typeId, "minecraft:frog_variant", 17, SERIALIZER_FROG_VARIANT) {
	setPathfindingMalus(PathType::Water, 4.0F);
	setPathfindingMalus(PathType::Trapdoor, -1.0F);
	setMoveControl(std::make_unique<SmoothSwimmingMoveControl>(*this, 85, 10, 0.02F, 0.1F, true));
}

std::unique_ptr<PathNavigation> Frog::createNavigation() { return std::make_unique<AmphibiousPathNavigation>(*this, _level); }

void Frog::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 2.0));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<FrogLayEggGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<TemptGoal>(*this, 1.25, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(5, std::make_unique<FrogTongueGoal>(*this));
	// FrogAi: a hop every 5 to 7 seconds, 4 blocks out, 2 up or down
	_goalSelector.addGoal(6, std::make_unique<LongJumpGoal>(*this, 100, 140, 2, 4, 3.5F, "minecraft:entity.frog.long_jump"));
	_goalSelector.addGoal(6, std::make_unique<RandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	// FrogAi: the nearest prey it sees within 10 blocks
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Slime", 10, true, false,
																			 [this](Actor& actor, Level&) { return canEat(actor); }));
}

void Frog::travel(const Vec3& input) {
	if (isInWater()) {
		moveRelative(speed(), input);
		move(_delta);
		_delta = _delta.scale(0.9);
		return;
	}
	VariantAnimal::travel(input);
}

bool Frog::canEat(Actor& actor) {
	auto* slime = dynamic_cast<Slime*>(&actor);
	if (slime && slime->size() != 1) return false;
	return actor.isAlive() && _level.gameData().isInTag("minecraft:entity_type", "minecraft:frog_food", actor.typeId());
}

void Frog::eat(Actor& prey) {
	playSound("minecraft:entity.frog.eat", 2.0F, 1.0F);
	bool magmaCube = _level.gameData().getStaticName("minecraft:entity_type", prey.typeId()) == "minecraft:magma_cube";
	Vec3 where	   = prey.position();
	doHurtTarget(prey);
	if (prey.isAlive()) return;
	if (Entity* entity = prey.asEntity()) entity->discard(); // Swallowed: no corpse
	if (magmaCube) {
		// The froglight of its color (the magma cube's loot, killed by a frog of that variant)
		std::string variant = Variants::name(_level, "minecraft:frog_variant", this->variant());
		const char* light	= variant == "minecraft:warm" ? "minecraft:pearlescent_froglight"
							: variant == "minecraft:cold" ? "minecraft:verdant_froglight"
														  : "minecraft:ochre_froglight";
		Vec3 saved = _position;
		_position  = where;
		spawnAtLocation(itemOf(_level, light));
		_position = saved;
	}
}

void Frog::spawnChildFromBreeding(Animal& partner) {
	// Frog.spawnChildFromBreeding: no baby, but this one carries the eggs
	setAge(PARENT_AGE_AFTER_BREEDING);
	partner.setAge(PARENT_AGE_AFTER_BREEDING);
	resetLove();
	partner.resetLove();
	broadcastEntityEvent(EVENT_IN_LOVE_HEARTS);
	_hasEgg = true;
}

void Frog::save(Buffer& buf) const {
	VariantAnimal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_hasEgg);
}

void Frog::load(Buffer& buf) {
	VariantAnimal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown frog data version");
	_hasEgg = buf.readBool();
}

// ===================== Tadpole =====================

void Tadpole::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<PanicGoal>(*this, 1.25));
	_goalSelector.addGoal(1, std::make_unique<TemptGoal>(*this, 1.25, [this](const ItemStack& stack) { return isItem(_level, stack, "minecraft:slime_ball"); }, false));
	_goalSelector.addGoal(4, std::make_unique<RandomSwimmingGoal>(*this, 1.0, 40));
}

void Tadpole::ageUp(int ticks) {
	_age += ticks;
	if (_age < MAX_AGE) return;
	// Tadpole.ageUp: a frog of the biome's variant takes its place
	playSound("minecraft:entity.tadpole.grow_up", 0.15F, 1.0F);
	convertTo(_level.gameData().getStaticId("minecraft:entity_type", "minecraft:frog"), false, false, [this](Mob& mob) {
		if (auto* frog = dynamic_cast<Frog*>(&mob)) {
			int variant = Variants::selectToSpawn(_level, "minecraft:frog_variant", blockPosition());
			if (variant >= 0) frog->setVariant(variant);
			frog->setPersistenceRequired();
		}
	});
}

void Tadpole::aiStep() {
	AbstractFish::aiStep();
	if (isAlive()) ageUp(1);
}

bool Tadpole::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isItem(_level, stack, "minecraft:slime_ball")) {
		// Tadpole.feed: a tenth of what is left
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
		ageUp(static_cast<int>((MAX_AGE - _age) * 0.1F));
		return true;
	}
	if (bucketPickup(*this, player, hand, "minecraft:tadpole_bucket")) return true;
	return AbstractFish::mobInteract(player, hand);
}

void Tadpole::save(Buffer& buf) const {
	AbstractFish::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_age);
}

void Tadpole::load(Buffer& buf) {
	AbstractFish::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown tadpole data version");
	_age = buf.readInt();
}

// ===================== Axolotl =====================

Axolotl::Axolotl(Level& level, int typeId) : Animal(level, typeId) {
	setPathfindingMalus(PathType::Water, 0.0F);
	setMoveControl(std::make_unique<SmoothSwimmingMoveControl>(*this, 85, 10, 0.1F, 0.5F, false));
	setLookControl(std::make_unique<SmoothSwimmingLookControl>(*this, 20));
	_airSupply = AXOLOTL_TOTAL_AIR_SUPPLY;
}

std::unique_ptr<PathNavigation> Axolotl::createNavigation() { return std::make_unique<AmphibiousPathNavigation>(*this, _level); }

void Axolotl::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<AxolotlPlayDeadGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<BreedGoal>(*this, 0.2));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 0.6, true));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.1, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 0.6));
	_goalSelector.addGoal(5, std::make_unique<RandomSwimmingGoal>(*this, 0.5, 40));
	_goalSelector.addGoal(6, std::make_unique<RandomStrollGoal>(*this, 0.15));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	// AxolotlAi: what is always hostile (drowned, guardians), and its prey (fish, squids, tadpoles), in the water
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "LivingEntity", 10, true, false, [this](Actor& actor, Level& level) {
		const GameData& data = level.gameData();
		if (!isInWaterNow() || isPlayingDead()) return false;
		return data.isInTag("minecraft:entity_type", "minecraft:axolotl_always_hostiles", actor.typeId()) ||
			   data.isInTag("minecraft:entity_type", "minecraft:axolotl_hunt_targets", actor.typeId());
	}));
}

void Axolotl::setVariant(int variant) {
	variant = ((variant % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT;
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_VARIANT);
}

void Axolotl::setPlayingDead(int ticks) {
	bool before	   = isPlayingDead();
	_playDeadTicks = ticks;
	if (before != isPlayingDead()) markData(DATA_PLAYING_DEAD);
}

void Axolotl::setFromBucket(bool from) {
	if (from == _fromBucket) return;
	_fromBucket = from;
	markData(DATA_FROM_BUCKET);
}

bool Axolotl::isFood(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:axolotl_food", stack.item);
}

void Axolotl::travel(const Vec3& input) {
	if (isInWater()) {
		moveRelative(speed(), input);
		move(_delta);
		_delta = _delta.scale(0.9);
		return;
	}
	Animal::travel(input);
}

void Axolotl::livingBaseTick() {
	int air = _airSupply;
	Animal::livingBaseTick();
	// Axolotl.handleAirSupply: out of the water (and rain) it dries out
	if (isNoAi()) return;
	if (isAlive() && !isInWaterOrRain()) {
		setAirSupply(air - 1);
		if (_airSupply <= -20) {
			setAirSupply(0);
			hurtServer({"minecraft:dry_out", nullptr, nullptr, std::nullopt}, 2.0F);
		}
	} else {
		setAirSupply(maxAirSupply());
	}
}

void Axolotl::customServerAiStep() {
	Animal::customServerAiStep();
	if (_playDeadTicks > 0) setPlayingDead(_playDeadTicks - 1);
}

bool Axolotl::hurtServer(const Combat::DamageSource& source, float amount) {
	float health = _health;
	if (!isNoAi() && _random.nextInt(3) == 0 && (static_cast<float>(_random.nextInt(3)) < amount || health / maxHealth() < 0.5F) && amount < health &&
		isInWater() && (source.causing || source.direct) && !isPlayingDead()) {
		setPlayingDead(PLAY_DEAD_TICKS);
	}
	return Animal::hurtServer(source, amount);
}

bool Axolotl::mobInteract(Player& player, int hand) {
	if (bucketPickup(*this, player, hand, "minecraft:axolotl_bucket")) return true;
	bool food = isFood(player.getStackInHand(hand));
	bool used = Animal::mobInteract(player, hand);
	// Axolotl.usePlayerItem: the tropical fish bucket comes back as a water bucket
	if (food && used && !player.isCreative()) player.inventory().set(player.handSlot(hand), itemOf(_level, "minecraft:water_bucket"));
	return used;
}

std::unique_ptr<Mob> Axolotl::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> baby  = Animal::getBreedOffspring(partner);
	auto*				 child = dynamic_cast<Axolotl*>(baby.get());
	auto*				 other = dynamic_cast<Axolotl*>(&partner);
	if (child && other) {
		child->setVariant(_random.nextInt(1200) == 0 ? VARIANT_BLUE : (_random.nextBoolean() ? _variant : other->_variant));
		child->setPersistenceRequired();
	}
	return baby;
}

std::shared_ptr<SpawnGroupData> Axolotl::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	// The common colors (Variant.getCommonSpawnVariant): every one but blue
	setVariant(_level.random().nextInt(VARIANT_COUNT - 1));
	setAirSupply(maxAirSupply());
	return Animal::finalizeSpawn(difficulty, reason, group);
}

void Axolotl::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_VARIANT, onlyNonDefault, _variant == 0)) writeIntData(buf, DATA_VARIANT, _variant);
	if (wantsData(mask, DATA_PLAYING_DEAD, onlyNonDefault, !isPlayingDead())) writeBoolData(buf, DATA_PLAYING_DEAD, isPlayingDead());
	if (wantsData(mask, DATA_FROM_BUCKET, onlyNonDefault, !_fromBucket)) writeBoolData(buf, DATA_FROM_BUCKET, _fromBucket);
}

void Axolotl::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_variant);
	buf.writeBool(_fromBucket);
	buf.writeInt(_playDeadTicks);
}

void Axolotl::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown axolotl data version");
	setVariant(buf.readInt());
	setFromBucket(buf.readBool());
	setPlayingDead(buf.readInt());
}
