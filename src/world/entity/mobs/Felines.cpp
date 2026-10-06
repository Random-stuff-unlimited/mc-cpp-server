#include "world/entity/mobs/Felines.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/blockentity/ContainerEntities.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/Variants.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"

#include <cmath>

namespace {
	constexpr int		  SAVE_VERSION		 = 1;
	constexpr int		  SERIALIZER_CAT	 = 21;
	constexpr const char* CAT_VARIANTS		 = "minecraft:cat_variant";
	constexpr double	  CROUCH_SPEED_MOD = 0.6, SPRINT_SPEED_MOD = 1.33;

	// Ocelot / Cat.customServerAiStep: crouching at the stalking speed, sprinting at the running one
	void updateFelinePose(Mob& mob) {
		double speed = mob.moveControl().hasWanted() ? mob.moveControl().speedModifier() : 0.0;
		mob.setPose(speed == CROUCH_SPEED_MOD ? LivingEntity::POSE_CROUCHING : LivingEntity::POSE_STANDING);
		mob.setSprinting(speed == SPRINT_SPEED_MOD);
	}

	// Turtle.BABY_ON_LAND_SELECTOR
	bool babyOnLand(Actor& target, Level&) {
		LivingEntity* living = target.asLiving();
		return living && living->isBaby() && !living->touchingWater();
	}

	bool blockInTag(Level& level, const BlockPos& pos, const char* tag) {
		const GameData& data = level.gameData();
		return data.isInTag("minecraft:block", tag, data.getBlocks().blockOf(level.getBlockState(pos)));
	}

	// Flees the players, while it doesn't trust them / isn't tamed
	class FelineAvoidPlayerGoal : public AvoidEntityGoal {
	  public:
		FelineAvoidPlayerGoal(Mob& mob, std::function<bool()> friendly)
			: AvoidEntityGoal(mob, "Player", 16.0F, 0.8, 1.33), _friendly(std::move(friendly)) {}
		bool canUse() override { return !_friendly() && AvoidEntityGoal::canUse(); }
		bool canContinueToUse() override { return !_friendly() && AvoidEntityGoal::canContinueToUse(); }

	  private:
		std::function<bool()> _friendly;
	};

	class OcelotTemptGoal : public TemptGoal {
	  public:
		OcelotTemptGoal(Ocelot& ocelot, double speed, std::function<bool(const ItemStack&)> items) : TemptGoal(ocelot, speed, std::move(items), true), _ocelot(ocelot) {}

	  protected:
		bool canScare() override { return TemptGoal::canScare() && !_ocelot.isTrusting(); }

	  private:
		Ocelot& _ocelot;
	};

	// Cat.CatTemptGoal: now and then it picks the player that tempts it, and no longer fears its moves
	class CatTemptGoal : public TemptGoal {
	  public:
		CatTemptGoal(Cat& cat, double speed, std::function<bool(const ItemStack&)> items) : TemptGoal(cat, speed, std::move(items), true), _cat(cat) {}
		void tick() override {
			TemptGoal::tick();
			if (!_selected.isSet() && _mob.random().nextInt(adjustedTickDelay(600)) == 0) {
				_selected = _player;
			} else if (_mob.random().nextInt(adjustedTickDelay(500)) == 0) {
				_selected.clear();
			}
		}
		bool canUse() override { return TemptGoal::canUse() && !_cat.isTame(); }

	  protected:
		bool canScare() override {
			Player* tempting = player();
			if (_selected.isSet() && tempting && _mob.level().actorByRef(_selected) == tempting) return false;
			return TemptGoal::canScare();
		}

	  private:
		Cat&	  _cat;
		EntityRef _selected;
	};

	// Cat.CatRelaxOnOwnerGoal: its owner sleeps within 10 blocks: to the foot of its bed, where it curls up
	class CatRelaxOnOwnerGoal : public Goal {
	  public:
		explicit CatRelaxOnOwnerGoal(Cat& cat) : _cat(cat) {}
		bool canUse() override {
			if (!_cat.isTame() || _cat.isOrderedToSit()) return false;
			Actor*	owner  = _cat.getOwner();
			Player* player = owner ? owner->asPlayer() : nullptr;
			if (!player) return false;
			_owner = EntityRef(*player);
			if (!player->survival().sleeping || _cat.distanceToSqr(*player) > 100.0) return false;
			Level&	 level = _cat.level();
			BlockPos pos{Mth::floor(player->getX()), Mth::floor(player->getY()), Mth::floor(player->getZ())};
			if (!blockInTag(level, pos, "minecraft:beds")) return false;
			int state  = level.getBlockState(pos);
			int facing = level.blocks().property("facing");
			_goalPos   = level.blocks().has(state, facing) ? pos.relative(Directions::opposite(level.blockContext().direction(state, facing))) : pos;
			_hasGoal   = true;
			return !spaceIsOccupied();
		}
		bool canContinueToUse() override {
			Player* player = owner();
			return _cat.isTame() && !_cat.isOrderedToSit() && player && player->survival().sleeping && _hasGoal && !spaceIsOccupied();
		}
		void start() override {
			if (!_hasGoal) return;
			_cat.setInSittingPose(false);
			_cat.navigation().moveTo(_goalPos.x, _goalPos.y, _goalPos.z, static_cast<double>(1.1F));
		}
		void stop() override {
			_cat.setLying(false);
			float	time   = _cat.level().timeOfDay();
			Player* player = owner();
			if (player && player->survival().sleepTimer >= 100 && time > 0.77 && time < 0.8 && _cat.level().random().nextFloat() < 0.7) _cat.giveMorningGift();
			_onBedTicks = 0;
			_cat.setRelaxStateOne(false);
			_cat.navigation().stop();
		}
		void tick() override {
			Player* player = owner();
			if (!player || !_hasGoal) return;
			_cat.setInSittingPose(false);
			_cat.navigation().moveTo(_goalPos.x, _goalPos.y, _goalPos.z, static_cast<double>(1.1F));
			if (_cat.distanceToSqr(*player) < 2.5) {
				_onBedTicks++;
				if (_onBedTicks > adjustedTickDelay(16)) {
					_cat.setLying(true);
					_cat.setRelaxStateOne(false);
				} else {
					_cat.lookAt(*player, 45.0F, 45.0F);
					_cat.setRelaxStateOne(true);
				}
			} else {
				_cat.setLying(false);
			}
		}

	  private:
		Cat&	  _cat;
		EntityRef _owner;
		BlockPos  _goalPos;
		bool	  _hasGoal	  = false;
		int		  _onBedTicks = 0;

		Player* owner() {
			Actor* actor = _owner.isSet() ? _cat.level().actorByRef(_owner) : nullptr;
			return actor ? actor->asPlayer() : nullptr;
		}
		bool spaceIsOccupied() {
			bool occupied = false;
			AABB area{_goalPos.x - 2.0, _goalPos.y - 2.0, _goalPos.z - 2.0, _goalPos.x + 3.0, _goalPos.y + 3.0, _goalPos.z + 3.0};
			_cat.level().entities().forEachIn(area, [&](Entity& entity) {
				auto* other = dynamic_cast<Cat*>(&entity);
				occupied |= other && other != &_cat && !other->isRemoved() && (other->isLying() || other->isRelaxStateOne());
			});
			return occupied;
		}
	};

	// CatLieOnBedGoal: a tamed cat goes to lie on a bed within 8 blocks
	class CatLieOnBedGoal : public MoveToBlockGoal {
	  public:
		CatLieOnBedGoal(Cat& cat, double speed, int range) : MoveToBlockGoal(cat, speed, range, 6), _cat(cat) {
			_verticalSearchStart = -2;
			setFlags({Flag::Jump, Flag::Move});
		}
		bool canUse() override { return _cat.isTame() && !_cat.isOrderedToSit() && !_cat.isLying() && MoveToBlockGoal::canUse(); }
		void start() override {
			MoveToBlockGoal::start();
			_cat.setInSittingPose(false);
		}
		void stop() override {
			MoveToBlockGoal::stop();
			_cat.setLying(false);
		}
		void tick() override {
			MoveToBlockGoal::tick();
			_cat.setInSittingPose(false);
			if (!isReachedTarget()) {
				_cat.setLying(false);
			} else if (!_cat.isLying()) {
				_cat.setLying(true);
			}
		}

	  protected:
		int	 nextStartTick() override { return 40; }
		bool isValidTarget(Level& level, const BlockPos& pos) override {
			return level.blocks().isAir(level.getBlockState(pos.above())) && blockInTag(level, pos, "minecraft:beds");
		}

	  private:
		Cat& _cat;
	};

	// CatSitOnBlockGoal: on a closed chest, a lit furnace or the foot of a bed
	class CatSitOnBlockGoal : public MoveToBlockGoal {
	  public:
		CatSitOnBlockGoal(Cat& cat, double speed) : MoveToBlockGoal(cat, speed, 8), _cat(cat) {}
		bool canUse() override { return _cat.isTame() && !_cat.isOrderedToSit() && MoveToBlockGoal::canUse(); }
		void start() override {
			MoveToBlockGoal::start();
			_cat.setInSittingPose(false);
		}
		void stop() override {
			MoveToBlockGoal::stop();
			_cat.setInSittingPose(false);
		}
		void tick() override {
			MoveToBlockGoal::tick();
			_cat.setInSittingPose(isReachedTarget());
		}

	  protected:
		bool isValidTarget(Level& level, const BlockPos& pos) override {
			if (!level.blocks().isAir(level.getBlockState(pos.above()))) return false;
			const GameData&		 data	= level.gameData();
			const BlockRegistry& blocks = level.blocks();
			int					 state	= level.getBlockState(pos);
			int					 block	= blocks.blockOf(state);
			if (block == data.getStaticId("minecraft:block", "minecraft:chest")) {
				auto* chest = level.getBlockEntity<ChestBlockEntity>(pos);
				return !chest || chest->openCount() < 1;
			}
			if (block == data.getStaticId("minecraft:block", "minecraft:furnace") && blocks.getBool(state, blocks.property("lit"))) return true;
			if (!data.isInTag("minecraft:block", "minecraft:beds", block)) return false;
			int part = blocks.property("part");
			return !blocks.has(state, part) || blocks.get(state, part) != blocks.value("head");
		}

	  private:
		Cat& _cat;
	};
} // namespace

// ===================== OcelotAttackGoal =====================

Actor* OcelotAttackGoal::target() { return _target.isSet() ? _mob.level().actorByRef(_target) : nullptr; }

bool OcelotAttackGoal::canUse() {
	Actor* target = _mob.getTarget();
	if (!target) return false;
	_target = EntityRef(*target);
	return true;
}

bool OcelotAttackGoal::canContinueToUse() {
	Actor* target = this->target();
	if (!target || !target->isAlive()) return false;
	if (_mob.distanceToSqr(*target) > 225.0) return false;
	return !_mob.navigation().isDone() || canUse();
}

void OcelotAttackGoal::stop() {
	_target.clear();
	_mob.navigation().stop();
}

void OcelotAttackGoal::tick() {
	Actor* target = this->target();
	if (!target) return;
	_mob.lookControl().setLookAt(*target, 30.0F, 30.0F);
	double reach	= _mob.width() * 2.0F * (_mob.width() * 2.0F);
	double distance = _mob.distanceToSqr(*target);
	double speed	= 0.8;
	if (distance > reach && distance < 16.0) {
		speed = 1.33;
	} else if (distance < 225.0) {
		speed = 0.6;
	}
	_mob.navigation().moveTo(*target, speed);
	_attackTime = std::max(_attackTime - 1, 0);
	if (distance > reach || _attackTime > 0) return;
	_attackTime = 20;
	_mob.doHurtTarget(*target);
}

// ===================== Ocelot =====================

void Ocelot::registerGoals() {
	auto tempt = std::make_unique<OcelotTemptGoal>(*this, 0.6, [this](const ItemStack& stack) { return isFood(stack); });
	_temptGoal = tempt.get();
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(3, std::move(tempt));
	_goalSelector.addGoal(7, std::make_unique<LeapAtTargetGoal>(*this, 0.3F));
	_goalSelector.addGoal(8, std::make_unique<OcelotAttackGoal>(*this));
	_goalSelector.addGoal(9, std::make_unique<BreedGoal>(*this, 0.8));
	_goalSelector.addGoal(10, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.8, 1.0000001E-5F));
	_goalSelector.addGoal(11, std::make_unique<LookAtPlayerGoal>(*this, "Player", 10.0F));
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Chicken", false));
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Turtle", 10, false, false, babyOnLand));
	reassessTrustingGoals();
}

void Ocelot::reassessTrustingGoals() {
	if (!_avoidPlayersGoal) _avoidPlayersGoal = std::make_shared<FelineAvoidPlayerGoal>(*this, [this] { return _trusting; });
	_goalSelector.removeGoal(*_avoidPlayersGoal);
	if (!_trusting) _goalSelector.addSharedGoal(4, _avoidPlayersGoal);
}

void Ocelot::setTrusting(bool trusting) {
	if (trusting != _trusting) {
		_trusting = trusting;
		markData(DATA_TRUSTING);
	}
	reassessTrustingGoals();
}

void Ocelot::customServerAiStep() { updateFelinePose(*this); }

bool Ocelot::isFood(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:ocelot_food", stack.item);
}

bool Ocelot::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if ((_temptGoal && !_temptGoal->isRunning()) || _trusting || !isFood(stack) || !(distanceToSqr(player) < 9.0)) return Animal::mobInteract(player, hand);
	usePlayerItem(player, hand);
	bool trusts = _random.nextInt(3) == 0;
	if (trusts) setTrusting(true);
	// spawnTrustingParticles: server side, only its random draws
	for (int i = 0; i < 7; i++) {
		_random.nextGaussian();
		_random.nextGaussian();
		_random.nextGaussian();
		_random.nextDouble();
		_random.nextDouble();
		_random.nextDouble();
	}
	broadcastEntityEvent(trusts ? EVENT_TRUSTING_SUCCEEDED : EVENT_TRUSTING_FAILED);
	return true;
}

void Ocelot::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_TRUSTING, onlyNonDefault, !_trusting)) writeBoolData(buf, DATA_TRUSTING, _trusting);
}

void Ocelot::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_trusting);
}

void Ocelot::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown ocelot data version");
	setTrusting(buf.readBool());
	_dirtyData = 0;
}

// ===================== Cat =====================

Cat::Cat(Level& level, int typeId) : TamableAnimal(level, typeId) {
	_defaultVariant = std::max(0, Variants::id(level, CAT_VARIANTS, "minecraft:black"));
	_variant		= _defaultVariant;
}

void Cat::registerGoals() {
	auto tempt = std::make_unique<CatTemptGoal>(*this, 0.6, [this](const ItemStack& stack) { return isFood(stack); });
	_temptGoal = tempt.get();
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<TamableAnimalPanicGoal>(*this, 1.5));
	_goalSelector.addGoal(2, std::make_unique<SitWhenOrderedToGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<CatRelaxOnOwnerGoal>(*this));
	_goalSelector.addGoal(4, std::move(tempt));
	_goalSelector.addGoal(5, std::make_unique<CatLieOnBedGoal>(*this, 1.1, 8));
	_goalSelector.addGoal(6, std::make_unique<FollowOwnerGoal>(*this, 1.0, 10.0F, 5.0F));
	_goalSelector.addGoal(7, std::make_unique<CatSitOnBlockGoal>(*this, 0.8));
	_goalSelector.addGoal(8, std::make_unique<LeapAtTargetGoal>(*this, 0.3F));
	_goalSelector.addGoal(9, std::make_unique<OcelotAttackGoal>(*this));
	_goalSelector.addGoal(10, std::make_unique<BreedGoal>(*this, 0.8));
	_goalSelector.addGoal(11, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.8, 1.0000001E-5F));
	_goalSelector.addGoal(12, std::make_unique<LookAtPlayerGoal>(*this, "Player", 10.0F));
	_targetSelector.addGoal(1, std::make_unique<NonTameRandomTargetGoal>(*this, "Rabbit", false, nullptr));
	_targetSelector.addGoal(1, std::make_unique<NonTameRandomTargetGoal>(*this, "Turtle", false, babyOnLand));
	reassessTameGoals();
}

void Cat::reassessTameGoals() {
	if (!_avoidPlayersGoal) _avoidPlayersGoal = std::make_shared<FelineAvoidPlayerGoal>(*this, [this] { return isTame(); });
	_goalSelector.removeGoal(*_avoidPlayersGoal);
	if (!isTame()) _goalSelector.addSharedGoal(4, _avoidPlayersGoal);
}

void Cat::setTame(bool tame, bool applySideEffects) {
	TamableAnimal::setTame(tame, applySideEffects);
	reassessTameGoals();
}

void Cat::setLying(bool lying) {
	if (lying == _lying) return;
	_lying = lying;
	markData(DATA_LYING);
}

void Cat::setRelaxStateOne(bool relax) {
	if (relax == _relaxStateOne) return;
	_relaxStateOne = relax;
	markData(DATA_RELAX_STATE_ONE);
}

void Cat::setCollarColor(int color) {
	if (color == _collarColor) return;
	_collarColor = color;
	markData(DATA_COLLAR_COLOR);
}

void Cat::setVariant(int variant) {
	if (variant < 0 || variant == _variant) return;
	_variant = variant;
	markData(DATA_VARIANT);
}

void Cat::hiss() { playSound("minecraft:entity.cat.hiss", soundVolume(), voicePitch()); }

void Cat::customServerAiStep() { updateFelinePose(*this); }

std::string Cat::ambientSoundName() {
	if (!isTame()) return "minecraft:entity.cat.stray_ambient";
	if (isInLove()) return "minecraft:entity.cat.purr";
	return _random.nextInt(4) == 0 ? "minecraft:entity.cat.purreow" : "minecraft:entity.cat.ambient";
}

void Cat::tick() {
	TamableAnimal::tick();
	if (isRemoved()) return;
	if (_temptGoal && _temptGoal->isRunning() && !isTame() && _tickCount % 100 == 0) playSound("minecraft:entity.cat.beg_for_food", 1.0F, 1.0F);
	// handleLieDown
	if ((_lying || _relaxStateOne) && _tickCount % 5 == 0) {
		playSound("minecraft:entity.cat.purr", 0.6F + 0.4F * (_random.nextFloat() - _random.nextFloat()), 1.0F);
	}
}

bool Cat::isFood(const ItemStack& stack) const { return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:cat_food", stack.item); }

bool Cat::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isTame()) {
		if (!isOwnedBy(player)) {
			bool consumed = TamableAnimal::mobInteract(player, hand);
			if (consumed) setPersistenceRequired();
			return consumed;
		}
		int dye = dyeColorOfItem(_level, stack);
		if (dye >= 0) {
			if (dye != _collarColor) {
				setCollarColor(dye);
				if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
				setPersistenceRequired();
				return true;
			}
		} else if (isFood(stack) && _health < maxHealth()) {
			const GameData::ItemProperties* props = _level.gameData().getItemProperties(stack.item);
			float nutrition = props && props->food ? static_cast<float>(props->food->nutrition) : 1.0F;
			usePlayerItem(player, hand);
			heal(nutrition);
			playEatingSound();
			return true;
		}
		if (!TamableAnimal::mobInteract(player, hand)) setOrderedToSit(!isOrderedToSit());
		return true;
	}
	if (isFood(stack)) {
		usePlayerItem(player, hand);
		// tryToTame
		if (_random.nextInt(3) == 0) {
			tame(player);
			setOrderedToSit(true);
			broadcastEntityEvent(EVENT_TAMING_SUCCEEDED);
		} else {
			broadcastEntityEvent(EVENT_TAMING_FAILED);
		}
		setPersistenceRequired();
		playEatingSound();
		return true;
	}
	bool consumed = TamableAnimal::mobInteract(player, hand);
	if (consumed) setPersistenceRequired();
	return consumed;
}

bool Cat::canMate(Animal& other) {
	if (!isTame()) return false;
	auto* cat = dynamic_cast<Cat*>(&other);
	return cat && cat->isTame() && TamableAnimal::canMate(other);
}

std::unique_ptr<Mob> Cat::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child	= TamableAnimal::getBreedOffspring(partner);
	auto*				 kitten = dynamic_cast<Cat*>(child.get());
	auto*				 other	= dynamic_cast<Cat*>(&partner);
	if (!kitten || !other) return child;
	kitten->setVariant(_random.nextBoolean() ? _variant : other->_variant);
	if (isTame()) {
		kitten->setOwnerReference(ownerReference());
		kitten->setTame(true, true);
		kitten->setCollarColor(mixedDyeColor(_level, _collarColor, other->_collarColor));
	}
	return child;
}

std::shared_ptr<SpawnGroupData> Cat::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group		= TamableAnimal::finalizeSpawn(difficulty, reason, group);
	int variant = Variants::selectToSpawn(_level, CAT_VARIANTS, blockPosition());
	if (variant >= 0) setVariant(variant);
	return group;
}

void Cat::giveMorningGift() {
	// BuiltInLootTables.CAT_MORNING_GIFT
	static constexpr struct {
		const char* item;
		int			weight;
	} GIFTS[] = {{"minecraft:rabbit_hide", 10}, {"minecraft:rabbit_foot", 10}, {"minecraft:chicken", 10},		   {"minecraft:feather", 10},
				 {"minecraft:rotten_flesh", 10}, {"minecraft:string", 10},	   {"minecraft:phantom_membrane", 2}};
	BlockPos at = blockPosition();
	randomTeleport(at.x + _random.nextInt(11) - 5, at.y + _random.nextInt(5) - 2, at.z + _random.nextInt(11) - 5, false);
	at		 = blockPosition();
	int roll = _random.nextInt(62);
	for (const auto& gift : GIFTS) {
		if ((roll -= gift.weight) >= 0) continue;
		const float toRadians = static_cast<float>(M_PI / 180.0);
		Vec3		pos{at.x - Mth::sin(_yBodyRot * toRadians), static_cast<double>(at.y), at.z + Mth::cos(_yBodyRot * toRadians)};
		_level.entities().add(ItemEntity::create(_level, pos, ItemStack(_level.gameData().getStaticId("minecraft:item", gift.item), 1)));
		break;
	}
}

std::string Cat::lootComponent(const std::string& component) const {
	if (component == "minecraft:cat/variant") return Variants::name(_level, CAT_VARIANTS, _variant);
	return TamableAnimal::lootComponent(component);
}

void Cat::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	TamableAnimal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_VARIANT, onlyNonDefault, _variant == _defaultVariant)) {
		buf.writeUByte(DATA_VARIANT);
		buf.writeVarInt(SERIALIZER_CAT);
		buf.writeVarInt(_variant);
	}
	if (wantsData(mask, DATA_LYING, onlyNonDefault, !_lying)) writeBoolData(buf, DATA_LYING, _lying);
	if (wantsData(mask, DATA_RELAX_STATE_ONE, onlyNonDefault, !_relaxStateOne)) writeBoolData(buf, DATA_RELAX_STATE_ONE, _relaxStateOne);
	if (wantsData(mask, DATA_COLLAR_COLOR, onlyNonDefault, _collarColor == DEFAULT_COLLAR_COLOR)) writeIntData(buf, DATA_COLLAR_COLOR, _collarColor);
}

void Cat::save(Buffer& buf) const {
	TamableAnimal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeString(Variants::name(_level, CAT_VARIANTS, _variant));
	buf.writeVarInt(_collarColor);
}

void Cat::load(Buffer& buf) {
	TamableAnimal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown cat data version");
	int variant = Variants::id(_level, CAT_VARIANTS, buf.readString());
	if (variant >= 0) _variant = variant;
	_collarColor = buf.readVarInt();
	reassessTameGoals();
	_dirtyData = 0;
}
