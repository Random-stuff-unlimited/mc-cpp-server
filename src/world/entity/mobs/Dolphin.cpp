#include "world/entity/mobs/Dolphin.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;

	bool isWaterAt(Level& level, const BlockPos& pos) { return level.fluids().isWater(level.fluids().stateOf(level.getBlockState(pos)).type); }

	// Dolphin.DolphinSwimToTreasureGoal: no structure can be located yet, so it is stuck at once and forgets its fish
	class DolphinSwimToTreasureGoal : public Goal {
	  public:
		explicit DolphinSwimToTreasureGoal(Dolphin& dolphin) : _dolphin(dolphin) { setFlags({Flag::Move, Flag::Look}); }
		bool isInterruptable() override { return false; }
		bool canUse() override { return _dolphin.gotFish() && _dolphin.airSupply() >= 100; }
		bool canContinueToUse() override { return false; }
		void start() override { _dolphin.navigation().stop(); }
		void stop() override { _dolphin.setGotFish(false); }

	  private:
		Dolphin& _dolphin;
	};

	// Dolphin.DolphinSwimWithPlayerGoal: along a swimming player within 10 blocks, which gets dolphin's grace
	class DolphinSwimWithPlayerGoal : public Goal {
	  public:
		DolphinSwimWithPlayerGoal(Dolphin& dolphin, double speed) : _dolphin(dolphin), _speedModifier(speed) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			TargetingConditions conditions = TargetingConditions::forNonCombat();
			conditions.withRange(10.0).ignoreLineOfSight();
			const Vec3& p	   = _dolphin.position();
			Actor*		nearest = nearestPlayer(_dolphin.level(), conditions, &_dolphin, p.x, p.y, p.z);
			Player*		player	= nearest ? nearest->asPlayer() : nullptr;
			if (!player) {
				_player.clear();
				return false;
			}
			_player = EntityRef(*player);
			return player->survival().swimming && _dolphin.getTarget() != player;
		}
		bool canContinueToUse() override {
			Player* player = this->player();
			return player && player->survival().swimming && _dolphin.distanceToSqr(*player) < 256.0;
		}
		void start() override {
			if (Player* player = this->player()) player->addEffect(MobEffects::Instance(MobEffects::DolphinsGrace, 100), &_dolphin);
		}
		void stop() override {
			_player.clear();
			_dolphin.navigation().stop();
		}
		void tick() override {
			Player* player = this->player();
			if (!player) return;
			_dolphin.lookControl().setLookAt(*player, static_cast<float>(_dolphin.maxHeadYRot() + 20), static_cast<float>(_dolphin.maxHeadXRot()));
			if (_dolphin.distanceToSqr(*player) < 6.25) {
				_dolphin.navigation().stop();
			} else {
				_dolphin.navigation().moveTo(*player, _speedModifier);
			}
			if (player->survival().swimming && _dolphin.level().random().nextInt(6) == 0) {
				player->addEffect(MobEffects::Instance(MobEffects::DolphinsGrace, 100), &_dolphin);
			}
		}

	  private:
		Dolphin&  _dolphin;
		double	  _speedModifier;
		EntityRef _player;

		Player* player() {
			if (!_player.isSet()) return nullptr;
			Actor* actor = _dolphin.level().actorByRef(_player);
			return actor ? actor->asPlayer() : nullptr;
		}
	};

	// DolphinJumpGoal: with 8 blocks of clear water ahead and air above, a leap out of the water
	class DolphinJumpGoal : public Goal {
	  public:
		DolphinJumpGoal(Dolphin& dolphin, int interval) : _dolphin(dolphin), _interval(reducedTickDelay(interval)) { setFlags({Flag::Move, Flag::Jump}); }
		bool canUse() override {
			if (_dolphin.random().nextInt(_interval) != 0) return false;
			int stepX, stepZ;
			_dolphin.motionStep(stepX, stepZ);
			BlockPos pos = _dolphin.blockPosition();
			for (int step : {0, 1, 4, 5, 6, 7}) {
				if (!waterIsClear(pos, stepX, stepZ, step) || !surfaceIsClear(pos, stepX, stepZ, step)) return false;
			}
			return true;
		}
		bool canContinueToUse() override {
			double y = _dolphin.deltaMovement().y;
			return (!(y * y < 0.03F) || _dolphin.xRot() == 0.0F || !(std::abs(_dolphin.xRot()) < 10.0F) || !_dolphin.isInWaterNow()) && !_dolphin.onGround();
		}
		bool isInterruptable() override { return false; }
		void start() override {
			int stepX, stepZ;
			_dolphin.motionStep(stepX, stepZ);
			_dolphin.setDeltaMovement(_dolphin.deltaMovement() + Vec3{stepX * 0.6, 0.7, stepZ * 0.6});
			_dolphin.navigation().stop();
		}
		void stop() override { _dolphin.setXRot(0.0F); }
		void tick() override {
			bool was = _breached;
			if (!was) _breached = isWaterAt(_dolphin.level(), _dolphin.blockPosition());
			if (_breached && !was) _dolphin.playSound("minecraft:entity.dolphin.jump", 1.0F, 1.0F);
			const Vec3& delta = _dolphin.deltaMovement();
			if (delta.y * delta.y < 0.03F && _dolphin.xRot() != 0.0F) {
				_dolphin.setXRot(_dolphin.xRot() + 0.2F * Mth::wrapDegrees(0.0F - _dolphin.xRot())); // Mth.rotLerp
			} else if (delta.length() > 1.0E-5F) {
				double horizontal = std::sqrt(delta.horizontalDistanceSqr());
				_dolphin.setXRot(static_cast<float>(std::atan2(-delta.y, horizontal) * 180.0F / (float)M_PI));
			}
		}

	  private:
		Dolphin& _dolphin;
		int		 _interval;
		bool	 _breached = false;

		bool waterIsClear(const BlockPos& pos, int stepX, int stepZ, int step) {
			Level&	 level = _dolphin.level();
			BlockPos at{pos.x + stepX * step, pos.y, pos.z + stepZ * step};
			return isWaterAt(level, at) && !level.gameData().getStateProperties(level.getBlockState(at)).blocksMotion;
		}
		bool surfaceIsClear(const BlockPos& pos, int stepX, int stepZ, int step) {
			Level& level = _dolphin.level();
			return level.blocks().isAir(level.getBlockState({pos.x + stepX * step, pos.y + 1, pos.z + stepZ * step})) &&
				   level.blocks().isAir(level.getBlockState({pos.x + stepX * step, pos.y + 2, pos.z + stepZ * step}));
		}
	};

	// Dolphin.PlayWithItemsGoal: goes to the items floating within 8 blocks, takes one and throws it again
	class PlayWithItemsGoal : public Goal {
	  public:
		explicit PlayWithItemsGoal(Dolphin& dolphin) : _dolphin(dolphin) {}
		bool canUse() override {
			if (_cooldown > _dolphin.tickCount()) return false;
			return firstItem() || !_dolphin.getItemBySlot(EquipmentSlot::MainHand).isEmpty();
		}
		void start() override {
			if (ItemEntity* item = firstItem()) {
				_dolphin.navigation().moveTo(*item, static_cast<double>(1.2F));
				_dolphin.playSound("minecraft:entity.dolphin.play", 1.0F, 1.0F);
			}
			_cooldown = 0;
		}
		void stop() override {
			ItemStack held = _dolphin.getItemBySlot(EquipmentSlot::MainHand);
			if (held.isEmpty()) return;
			drop(held);
			_dolphin.setItemSlot(EquipmentSlot::MainHand, ItemStack());
			_cooldown = _dolphin.tickCount() + _dolphin.random().nextInt(100);
		}
		void tick() override {
			ItemEntity* item = firstItem();
			ItemStack	held = _dolphin.getItemBySlot(EquipmentSlot::MainHand);
			if (!held.isEmpty()) {
				drop(held);
				_dolphin.setItemSlot(EquipmentSlot::MainHand, ItemStack());
			} else if (item) {
				_dolphin.navigation().moveTo(*item, static_cast<double>(1.2F));
			}
		}

	  private:
		Dolphin& _dolphin;
		int		 _cooldown = 0;

		// Dolphin.ALLOWED_ITEMS: no pickup delay, alive, in water
		ItemEntity* firstItem() {
			ItemEntity* found = nullptr;
			_dolphin.level().entities().forEachIn(_dolphin.boundingBox().inflate(8.0), [&](Entity& entity) {
				if (found || entity.isRemoved()) return;
				auto* item = dynamic_cast<ItemEntity*>(&entity);
				if (item && item->pickupDelay() <= 0 && item->touchingWater()) found = item;
			});
			return found;
		}
		void drop(const ItemStack& stack) {
			if (stack.isEmpty()) return;
			JavaRandom& random	  = _dolphin.random();
			const float toRadians = static_cast<float>(M_PI / 180.0);
			float		angle	  = random.nextFloat() * static_cast<float>(M_PI * 2);
			float		spread	  = 0.02F * random.nextFloat();
			float		yaw = _dolphin.yRot() * toRadians, pitch = _dolphin.xRot() * toRadians;
			Vec3		delta{0.3F * -Mth::sin(yaw) * Mth::cos(pitch) + Mth::cos(angle) * spread, 0.3F * Mth::sin(pitch) * 1.5F,
							  0.3F * Mth::cos(yaw) * Mth::cos(pitch) + Mth::sin(angle) * spread};
			const Vec3& p	 = _dolphin.position();
			auto		item = std::make_unique<ItemEntity>(_dolphin.level(), Vec3{p.x, _dolphin.eyeY() - 0.3F, p.z}, stack, delta);
			item->setPickupDelay(40);
			_dolphin.level().entities().add(std::move(item));
		}
	};
} // namespace

Dolphin::Dolphin(Level& level, int typeId) : AgeableWaterCreature(level, typeId) {
	setMoveControl(std::make_unique<SmoothSwimmingMoveControl>(*this, 85, 10, 0.02F, 0.1F, true));
	setLookControl(std::make_unique<SmoothSwimmingLookControl>(*this, 10));
	setCanPickUpLoot(true);
}

std::unique_ptr<PathNavigation> Dolphin::createNavigation() { return std::make_unique<WaterBoundPathNavigation>(*this, _level); }

void Dolphin::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<BreathAirGoal>(*this));
	_goalSelector.addGoal(0, std::make_unique<TryFindWaterGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<DolphinSwimToTreasureGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<DolphinSwimWithPlayerGoal>(*this, 4.0));
	_goalSelector.addGoal(4, std::make_unique<RandomSwimmingGoal>(*this, 1.0, 10));
	_goalSelector.addGoal(4, std::make_unique<RandomLookAroundGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(5, std::make_unique<DolphinJumpGoal>(*this, 10));
	_goalSelector.addGoal(6, std::make_unique<MeleeAttackGoal>(*this, static_cast<double>(1.2F), true));
	_goalSelector.addGoal(8, std::make_unique<PlayWithItemsGoal>(*this));
	// FollowBoatGoal: no boats yet
	_goalSelector.addGoal(9, std::make_unique<AvoidEntityGoal>(*this, "Guardian", 8.0F, 1.0, 1.0));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Guardian"});
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
}

std::shared_ptr<SpawnGroupData> Dolphin::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	setAirSupply(maxAirSupply());
	_xRot = 0.0F;
	return AgeableWaterCreature::finalizeSpawn(difficulty, reason, group);
}

void Dolphin::setGotFish(bool got) {
	if (got == _gotFish) return;
	_gotFish = got;
	markData(DATA_GOT_FISH);
}

void Dolphin::setMoistnessLevel(int level) {
	if (level == _moistness) return;
	_moistness = level;
	markData(DATA_MOISTNESS_LEVEL);
}

void Dolphin::motionStep(int& x, int& z) const {
	// Direction.fromYRot: south, west, north, east
	static constexpr int STEP_X[4] = {0, -1, 0, 1}, STEP_Z[4] = {1, 0, -1, 0};
	int					 index	   = Mth::floor(_yRot / 90.0 + 0.5) & 3;
	x							   = STEP_X[index];
	z							   = STEP_Z[index];
}

void Dolphin::tick() {
	AgeableWaterCreature::tick();
	if (isRemoved()) return;
	if (isNoAi()) {
		setAirSupply(maxAirSupply());
		return;
	}
	if (isInWaterOrRain()) {
		setMoistnessLevel(TOTAL_MOISTNESS_LEVEL);
		return;
	}
	setMoistnessLevel(_moistness - 1);
	if (_moistness <= 0) hurtServer({"minecraft:dry_out", nullptr}, 1.0F);
	if (_onGround) {
		// Stranded: it flops around
		_delta	   = _delta + Vec3{(_random.nextFloat() * 2.0F - 1.0F) * 0.2F, 0.5, (_random.nextFloat() * 2.0F - 1.0F) * 0.2F};
		_yRot	   = _random.nextFloat() * 360.0F;
		_onGround  = false;
		hasImpulse = true;
	}
}

bool Dolphin::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (stack.isEmpty() || !_level.gameData().isInTag("minecraft:item", "minecraft:fishes", stack.item)) return AgeableWaterCreature::mobInteract(player, hand);
	playSound("minecraft:entity.dolphin.eat", 1.0F, 1.0F);
	if (isBaby()) {
		ageUp(Animal::getSpeedUpSecondsWhenFeeding(-_age), true);
	} else {
		setGotFish(true);
	}
	if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
	return true;
}

void Dolphin::pickUpItem(ItemEntity& item) {
	if (!getItemBySlot(EquipmentSlot::MainHand).isEmpty() || !canHoldItem(item.item())) return;
	ItemStack stack = item.item();
	Buffer	  take;
	take.writeVarInt(item.id());
	take.writeVarInt(_id);
	take.writeVarInt(stack.count);
	_level.entities().broadcast(item, PacketId::Play::Clientbound::TAKE_ITEM_ENTITY, take);
	setItemSlot(EquipmentSlot::MainHand, std::move(stack));
	setGuaranteedDrop(EquipmentSlot::MainHand);
	item.discard();
}

void Dolphin::travel(const Vec3& input) {
	if (!isInWater()) {
		AgeableWaterCreature::travel(input);
		return;
	}
	moveRelative(_speed, input);
	move(_delta);
	_delta = _delta.scale(0.9);
	if (!getTarget()) _delta.y -= 0.005;
}

std::string Dolphin::ambientSoundName() { return isInWater() ? "minecraft:entity.dolphin.ambient_water" : "minecraft:entity.dolphin.ambient"; }

void Dolphin::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AgeableWaterCreature::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_GOT_FISH, onlyNonDefault, !_gotFish)) writeBoolData(buf, DATA_GOT_FISH, _gotFish);
	if (wantsData(mask, DATA_MOISTNESS_LEVEL, onlyNonDefault, _moistness == TOTAL_MOISTNESS_LEVEL)) writeIntData(buf, DATA_MOISTNESS_LEVEL, _moistness);
}

void Dolphin::save(Buffer& buf) const {
	AgeableWaterCreature::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_gotFish);
	buf.writeVarInt(_moistness);
}

void Dolphin::load(Buffer& buf) {
	AgeableWaterCreature::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown dolphin data version");
	_gotFish   = buf.readBool();
	_moistness = buf.readVarInt();
	_dirtyData = 0;
}
