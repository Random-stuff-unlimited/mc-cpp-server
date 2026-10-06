#include "world/entity/mobs/Fox.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ExperienceOrb.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/TamableAnimal.hpp"
#include "world/entity/mobs/WaterAnimals.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int SERIALIZER_OPTIONAL_LIVING_REFERENCE = 13;
	constexpr int LEVEL_EVENT_DESTROY_BLOCK = 2001;

	struct FoxGroupData : AgeableMobGroupData {
		int variant;
		explicit FoxGroupData(int v) : AgeableMobGroupData(false, 0.05F), variant(v) {}
	};

	// Entity.isDiscrete: a sneaking player
	bool isDiscrete(Actor& actor) {
		Player* player = actor.asPlayer();
		return player && player->survival().pose == Pose::Crouching;
	}
	bool isStalkablePrey(Actor& actor) { return isOfClass(actor, "Chicken") || isOfClass(actor, "Rabbit"); }

	// Fox.FoxAlertableEntitiesSelector: what keeps a fox from resting
	bool alertsFox(Fox& fox, Actor& actor) {
		if (isOfClass(actor, "Fox")) return false;
		if (isStalkablePrey(actor) || isOfClass(actor, "Monster")) return true;
		if (Entity* entity = actor.asEntity()) {
			if (auto* tamable = dynamic_cast<TamableAnimal*>(entity)) return !tamable->isTame();
		}
		Player* player = actor.asPlayer();
		if (player && (player->isSpectator() || player->isCreative())) return false;
		if (fox.trusts(actor)) return false;
		return !(player && player->survival().sleeping) && !isDiscrete(actor);
	}

	// FoxBehaviorGoal
	bool hasShelter(Fox& fox) {
		BlockPos pos{Mth::floor(fox.position().x), Mth::floor(fox.boundingBox().maxY), Mth::floor(fox.position().z)};
		return !fox.level().canSeeSky(pos) && fox.getWalkTargetValue(pos) >= 0.0F;
	}
	bool alertable(Fox& fox) {
		Level&				level	   = fox.level();
		TargetingConditions conditions = TargetingConditions::forCombat();
		conditions.withRange(12.0).ignoreLineOfSight().withSelector([&fox](Actor& actor, Level&) { return alertsFox(fox, actor); });
		AABB area  = fox.boundingBox().inflate(12.0, 6.0, 12.0);
		bool found = false;
		for (const auto& player : level.players()) {
			if (!found && player->boundingBox().intersects(area)) found = conditions.test(level, &fox, *player);
		}
		if (found) return true;
		level.entities().forEachIn(area, [&](Entity& entity) {
			if (found || &entity == &fox || !entity.asLiving() || entity.isRemoved()) return;
			found = conditions.test(level, &fox, entity);
		});
		return found;
	}

	class FoxLookControl : public LookControl {
	  public:
		explicit FoxLookControl(Fox& fox) : LookControl(fox), _fox(fox) {}
		void tick() override {
			if (!_fox.isSleeping()) LookControl::tick();
		}

	  protected:
		bool resetXRotOnTick() const override { return !_fox.isPouncing() && !_fox.isCrouching() && !_fox.isInterested() && !_fox.isFaceplanted(); }

	  private:
		Fox& _fox;
	};

	class FoxMoveControl : public MoveControl {
	  public:
		explicit FoxMoveControl(Fox& fox) : MoveControl(fox), _fox(fox) {}
		void tick() override {
			if (_fox.canMove()) MoveControl::tick();
		}

	  private:
		Fox& _fox;
	};

	class FoxFloatGoal : public FloatGoal {
	  public:
		explicit FoxFloatGoal(Fox& fox) : FloatGoal(fox), _fox(fox) {}
		void start() override {
			FloatGoal::start();
			_fox.clearStates();
		}
		bool canUse() override { return (_fox.isInWaterNow() && _fox.waterHeight() > 0.25) || _fox.isInLavaNow(); }

	  private:
		Fox& _fox;
	};

	class FaceplantGoal : public Goal {
	  public:
		explicit FaceplantGoal(Fox& fox) : _fox(fox) { setFlags({Flag::Look, Flag::Jump, Flag::Move}); }
		bool canUse() override { return _fox.isFaceplanted(); }
		bool canContinueToUse() override { return canUse() && _countdown > 0; }
		void start() override { _countdown = adjustedTickDelay(40); }
		void stop() override { _fox.setFaceplanted(false); }
		void tick() override { _countdown--; }

	  private:
		Fox& _fox;
		int	 _countdown = 0;
	};

	class FoxPanicGoal : public PanicGoal {
	  public:
		FoxPanicGoal(Fox& fox, double speed) : PanicGoal(fox, speed), _fox(fox) {}

	  protected:
		bool shouldPanic() override { return !_fox.isDefending() && PanicGoal::shouldPanic(); }

	  private:
		Fox& _fox;
	};

	class FoxBreedGoal : public BreedGoal {
	  public:
		FoxBreedGoal(Fox& fox, double speed) : BreedGoal(fox, speed), _fox(fox) {}
		void start() override {
			_fox.clearStates();
			Actor* actor = _partner.isSet() ? _fox.level().actorByRef(_partner) : nullptr;
			if (auto* partner = dynamic_cast<Fox*>(actor ? actor->asEntity() : nullptr)) partner->clearStates();
			BreedGoal::start();
		}

	  private:
		Fox& _fox;
	};

	// Follows its prey until 6 blocks away, then crouches, ready to pounce
	class StalkPreyGoal : public Goal {
	  public:
		explicit StalkPreyGoal(Fox& fox) : _fox(fox) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			if (_fox.isSleeping()) return false;
			Actor* target = _fox.getTarget();
			return target && target->isAlive() && isStalkablePrey(*target) && _fox.distanceToSqr(*target) > 36.0 && !_fox.isCrouching() && !_fox.isInterested() &&
				   !_fox.isJumping();
		}
		void start() override {
			_fox.setSitting(false);
			_fox.setFaceplanted(false);
		}
		void stop() override {
			Actor* target = _fox.getTarget();
			if (target && _fox.isPathClear(*target)) {
				_fox.setIsInterested(true);
				_fox.setIsCrouching(true);
				_fox.navigation().stop();
				_fox.lookControl().setLookAt(*target, static_cast<float>(_fox.maxHeadYRot()), static_cast<float>(_fox.maxHeadXRot()));
			} else {
				_fox.setIsInterested(false);
				_fox.setIsCrouching(false);
			}
		}
		void tick() override {
			Actor* target = _fox.getTarget();
			if (!target) return;
			_fox.lookControl().setLookAt(*target, static_cast<float>(_fox.maxHeadYRot()), static_cast<float>(_fox.maxHeadXRot()));
			if (_fox.distanceToSqr(*target) <= 36.0) {
				_fox.setIsInterested(true);
				_fox.setIsCrouching(true);
				_fox.navigation().stop();
			} else {
				_fox.navigation().moveTo(*target, 1.5);
			}
		}

	  private:
		Fox& _fox;
	};

	// Fully crouched with a clear way: leaps onto its target
	class FoxPounceGoal : public Goal {
	  public:
		explicit FoxPounceGoal(Fox& fox) : _fox(fox) { setFlags({Flag::Move, Flag::Jump}); }
		bool canUse() override {
			if (!_fox.isFullyCrouched()) return false;
			Actor* target = _fox.getTarget();
			if (!target || !target->isAlive()) return false;
			bool clear = _fox.isPathClear(*target);
			if (!clear) {
				_fox.setIsCrouching(false);
				_fox.setIsInterested(false);
			}
			return clear;
		}
		bool canContinueToUse() override {
			Actor* target = _fox.getTarget();
			if (!target || !target->isAlive()) return false;
			double y = _fox.deltaMovement().y;
			return (!(y * y < 0.05F) || !(std::abs(_fox.xRot()) < 15.0F) || !_fox.onGround()) && !_fox.isFaceplanted();
		}
		bool isInterruptable() override { return false; }
		void start() override {
			_fox.setJumping(true);
			_fox.setIsPouncing(true);
			_fox.setIsInterested(false);
			if (Actor* target = _fox.getTarget()) {
				_fox.lookControl().setLookAt(*target, 60.0F, 30.0F);
				Vec3 to = (target->position() - _fox.position()).normalize();
				_fox.setDeltaMovement(_fox.deltaMovement() + Vec3{to.x * 0.8, 0.9, to.z * 0.8});
			}
			_fox.navigation().stop();
		}
		void stop() override {
			_fox.setIsCrouching(false);
			_fox.resetCrouchAmount();
			_fox.setIsInterested(false);
			_fox.setIsPouncing(false);
		}
		void tick() override {
			Actor* target = _fox.getTarget();
			if (target) _fox.lookControl().setLookAt(*target, 60.0F, 30.0F);
			if (!_fox.isFaceplanted()) {
				const Vec3& delta = _fox.deltaMovement();
				if (delta.y * delta.y < 0.03F && _fox.xRot() != 0.0F) {
					_fox.setXRot(_fox.xRot() + 0.2F * Mth::wrapDegrees(0.0F - _fox.xRot()));
				} else {
					double horizontal = std::sqrt(delta.horizontalDistanceSqr());
					double sign		  = -delta.y > 0 ? 1.0 : (-delta.y < 0 ? -1.0 : 0.0);
					_fox.setXRot(static_cast<float>(sign * std::acos(horizontal / delta.length()) * 180.0F / (float)M_PI));
				}
			}
			Level& level = _fox.level();
			if (target && std::sqrt(_fox.distanceToSqr(*target)) <= 2.0F) {
				_fox.doHurtTarget(*target);
			} else if (_fox.xRot() > 0.0F && _fox.onGround() && static_cast<float>(_fox.deltaMovement().y) != 0.0F &&
					   level.blocks().blockOf(level.getBlockState(_fox.blockPosition())) == level.gameData().getStaticId("minecraft:block", "minecraft:snow")) {
				_fox.setXRot(60.0F);
				_fox.setTarget(nullptr);
				_fox.setFaceplanted(true);
			}
		}

	  private:
		Fox& _fox;
	};

	// By day under the sky (or in a thunderstorm): to a shaded place
	class SeekShelterGoal : public FleeSunGoal {
	  public:
		SeekShelterGoal(Fox& fox, double speed) : FleeSunGoal(fox, speed), _fox(fox) {}
		bool canUse() override {
			if (_fox.isSleeping() || _fox.getTarget()) return false;
			Level& level = _fox.level();
			if (level.isThundering() && level.canSeeSky(_fox.blockPosition())) return setWantedPos();
			if (_interval > 0) {
				_interval--;
				return false;
			}
			_interval	 = 100;
			BlockPos pos = _fox.blockPosition();
			return level.isBrightOutside() && level.canSeeSky(pos) && !level.isVillage(pos) && setWantedPos();
		}
		void start() override {
			_fox.clearStates();
			FleeSunGoal::start();
		}

	  private:
		Fox& _fox;
		int	 _interval = reducedTickDelay(100);
	};

	class FoxMeleeAttackGoal : public MeleeAttackGoal {
	  public:
		FoxMeleeAttackGoal(Fox& fox, double speed, bool follow) : MeleeAttackGoal(fox, speed, follow), _fox(fox) {}
		void start() override {
			_fox.setIsInterested(false);
			MeleeAttackGoal::start();
		}
		bool canUse() override { return !_fox.isSitting() && !_fox.isSleeping() && !_fox.isCrouching() && !_fox.isFaceplanted() && MeleeAttackGoal::canUse(); }

	  protected:
		void checkAndPerformAttack(Actor& target) override {
			if (!canPerformAttack(target)) return;
			resetAttackCooldown();
			_mob.doHurtTarget(target);
			_fox.playSound("minecraft:entity.fox.bite", 1.0F, 1.0F);
		}

	  private:
		Fox& _fox;
	};

	// Still, by day, sheltered and with nothing alarming around: sleeps
	class SleepGoal : public Goal {
	  public:
		explicit SleepGoal(Fox& fox) : _fox(fox), _countdown(fox.random().nextInt(WAIT_TIME_BEFORE_SLEEP)) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return !_fox.hasInput() ? canSleep() || _fox.isSleeping() : false; }
		bool canContinueToUse() override { return canSleep(); }
		void stop() override {
			_countdown = _fox.random().nextInt(WAIT_TIME_BEFORE_SLEEP);
			_fox.clearStates();
		}
		void start() override {
			_fox.setSitting(false);
			_fox.setIsCrouching(false);
			_fox.setIsInterested(false);
			_fox.setJumping(false);
			_fox.setSleeping(true);
			_fox.navigation().stop();
			_fox.moveControl().setWantedPosition(_fox.position().x, _fox.position().y, _fox.position().z, 0.0);
		}

	  private:
		static constexpr int WAIT_TIME_BEFORE_SLEEP = 70; // reducedTickDelay(140)
		Fox&				 _fox;
		int					 _countdown;

		bool canSleep() {
			if (_countdown > 0) {
				_countdown--;
				return false;
			}
			return _fox.level().isBrightOutside() && hasShelter(_fox) && !alertable(_fox) && !_fox.inPowderSnow();
		}
	};

	class FoxFollowParentGoal : public FollowParentGoal {
	  public:
		FoxFollowParentGoal(Fox& fox, double speed) : FollowParentGoal(fox, speed), _fox(fox) {}
		bool canUse() override { return !_fox.isDefending() && FollowParentGoal::canUse(); }
		bool canContinueToUse() override { return !_fox.isDefending() && FollowParentGoal::canContinueToUse(); }
		void start() override {
			_fox.clearStates();
			FollowParentGoal::start();
		}

	  private:
		Fox& _fox;
	};

	class FoxStrollThroughVillageGoal : public StrollThroughVillageGoal {
	  public:
		FoxStrollThroughVillageGoal(Fox& fox, int interval) : StrollThroughVillageGoal(fox, interval), _fox(fox) {}
		void start() override {
			_fox.clearStates();
			StrollThroughVillageGoal::start();
		}
		bool canUse() override { return StrollThroughVillageGoal::canUse() && canFoxMove(); }
		bool canContinueToUse() override { return StrollThroughVillageGoal::canContinueToUse() && canFoxMove(); }

	  private:
		Fox& _fox;
		bool canFoxMove() { return !_fox.isSleeping() && !_fox.isSitting() && !_fox.isDefending() && !_fox.getTarget(); }
	};

	// To a berry bush within 12 blocks, which it picks after 2 seconds next to it
	class FoxEatBerriesGoal : public MoveToBlockGoal {
	  public:
		FoxEatBerriesGoal(Fox& fox, double speed, int range, int vertical) : MoveToBlockGoal(fox, speed, range, vertical), _fox(fox) {}
		bool canUse() override { return !_fox.isSleeping() && MoveToBlockGoal::canUse(); }
		void start() override {
			_ticksWaited = 0;
			_fox.setSitting(false);
			MoveToBlockGoal::start();
		}
		void tick() override {
			if (isReachedTarget()) {
				if (_ticksWaited >= 40) {
					onReachedTarget();
				} else {
					_ticksWaited++;
				}
			} else if (_fox.random().nextFloat() < 0.05F) {
				_fox.playSound("minecraft:entity.fox.sniff", 1.0F, 1.0F);
			}
			MoveToBlockGoal::tick();
		}

	  protected:
		double acceptedDistance() const override { return 2.0; }
		bool   shouldRecalculatePath() const override { return _tryTicks % 100 == 0; }
		bool   isValidTarget(Level& level, const BlockPos& pos) override {
			  int state = level.getBlockState(pos);
			  return (isBush(level, state) && level.blocks().getInt(state, level.blocks().property("age")) >= 2) || hasGlowBerries(level, state);
		}

	  private:
		Fox& _fox;
		int	 _ticksWaited = 0;

		static bool isBush(Level& level, int state) {
			return level.blocks().blockOf(state) == level.gameData().getStaticId("minecraft:block", "minecraft:sweet_berry_bush");
		}
		static bool hasGlowBerries(Level& level, int state) {
			const BlockRegistry& blocks	 = level.blocks();
			int					 berries = blocks.property("berries");
			return blocks.has(state, berries) && blocks.getBool(state, berries);
		}
		void onReachedTarget() {
			Level&				 level	= _fox.level();
			const BlockRegistry& blocks = level.blocks();
			const GameData&		 data	= level.gameData();
			int					 state	= level.getBlockState(_blockPos);
			if (isBush(level, state)) {
				int age	  = blocks.getInt(state, blocks.property("age"));
				int count = 1 + level.random().nextInt(2) + (age == 3 ? 1 : 0);
				int item  = data.getStaticId("minecraft:item", "minecraft:sweet_berries");
				if (_fox.getItemBySlot(EquipmentSlot::MainHand).isEmpty()) {
					_fox.setItemSlot(EquipmentSlot::MainHand, ItemStack(item, 1));
					count--;
				}
				if (count > 0) level.popResource(_blockPos, ItemStack(item, count));
				_fox.playSound("minecraft:block.sweet_berry_bush.pick_berries", 1.0F, 1.0F);
				level.setBlock(_blockPos, blocks.withInt(state, blocks.property("age"), 1), Level::UPDATE_CLIENTS);
			} else if (hasGlowBerries(level, state)) {
				// CaveVines.use
				level.popResource(_blockPos, ItemStack(data.getStaticId("minecraft:item", "minecraft:glow_berries"), 1));
				float pitch = 0.8F + level.random().nextFloat() * 0.4F;
				level.playSound(nullptr, _blockPos, "minecraft:block.cave_vines.pick_berries", Level::SoundSource::Blocks, 1.0F, pitch);
				level.setBlock(_blockPos, blocks.withBool(state, blocks.property("berries"), false), Level::UPDATE_CLIENTS);
			}
		}
	};

	// With nothing in its mouth: to the items lying within 8 blocks
	class FoxSearchForItemsGoal : public Goal {
	  public:
		explicit FoxSearchForItemsGoal(Fox& fox) : _fox(fox) { setFlags({Flag::Move}); }
		bool canUse() override {
			if (!_fox.getItemBySlot(EquipmentSlot::MainHand).isEmpty()) return false;
			if (_fox.getTarget() || _fox.getLastHurtByMob()) return false;
			if (!_fox.canMove()) return false;
			if (_fox.random().nextInt(reducedTickDelay(10)) != 0) return false;
			return firstItem() != nullptr;
		}
		void tick() override { start(); }
		void start() override {
			if (!_fox.getItemBySlot(EquipmentSlot::MainHand).isEmpty()) return;
			if (ItemEntity* item = firstItem()) _fox.navigation().moveTo(*item, static_cast<double>(1.2F));
		}

	  private:
		Fox& _fox;
		ItemEntity* firstItem() {
			ItemEntity* found = nullptr;
			_fox.level().entities().forEachIn(_fox.boundingBox().inflate(8.0), [&](Entity& entity) {
				if (found || entity.isRemoved()) return;
				auto* item = dynamic_cast<ItemEntity*>(&entity);
				if (item && item->pickupDelay() <= 0) found = item;
			});
			return found;
		}
	};

	class FoxLookAtPlayerGoal : public LookAtPlayerGoal {
	  public:
		FoxLookAtPlayerGoal(Fox& fox, float distance) : LookAtPlayerGoal(fox, "Player", distance), _fox(fox) {}
		bool canUse() override { return LookAtPlayerGoal::canUse() && !_fox.isFaceplanted() && !_fox.isInterested(); }
		bool canContinueToUse() override { return LookAtPlayerGoal::canContinueToUse() && !_fox.isFaceplanted() && !_fox.isInterested(); }

	  private:
		Fox& _fox;
	};

	// With nothing else to do: sits and looks around a few times
	class PerchAndSearchGoal : public Goal {
	  public:
		explicit PerchAndSearchGoal(Fox& fox) : _fox(fox) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			return !_fox.getLastHurtByMob() && _fox.random().nextFloat() < 0.02F && !_fox.isSleeping() && !_fox.getTarget() && _fox.navigation().isDone() &&
				   !alertable(_fox) && !_fox.isPouncing() && !_fox.isCrouching();
		}
		bool canContinueToUse() override { return _looksRemaining > 0; }
		void start() override {
			resetLook();
			_looksRemaining = 2 + _fox.random().nextInt(3);
			_fox.setSitting(true);
			_fox.navigation().stop();
		}
		void stop() override { _fox.setSitting(false); }
		void tick() override {
			if (--_lookTime <= 0) {
				_looksRemaining--;
				resetLook();
			}
			_fox.lookControl().setLookAt(_fox.position().x + _relX, _fox.eyeY(), _fox.position().z + _relZ, static_cast<float>(_fox.maxHeadYRot()),
										 static_cast<float>(_fox.maxHeadXRot()));
		}

	  private:
		Fox&   _fox;
		double _relX = 0, _relZ = 0;
		int	   _lookTime = 0, _looksRemaining = 0;
		void   resetLook() {
			  double angle = (M_PI * 2) * _fox.random().nextDouble();
			  _relX		   = std::cos(angle);
			  _relZ		   = std::sin(angle);
			  _lookTime	   = adjustedTickDelay(80 + _fox.random().nextInt(20));
		}
	};

	// Attacks who hurt the one it trusts
	class DefendTrustedTargetGoal : public NearestAttackableTargetGoal {
	  public:
		explicit DefendTrustedTargetGoal(Fox& fox)
			: NearestAttackableTargetGoal(fox, "LivingEntity", 10, false, false,
										  [&fox](Actor& target, Level& level) {
											  // TRUSTED_TARGET_SELECTOR: it hurt something lately
											  bool hurt = false;
											  if (Player* player = target.asPlayer()) {
												  hurt = player->lastHurtMob().isSet() && level.actorByRef(player->lastHurtMob());
											  } else if (LivingEntity* living = target.asLiving()) {
												  hurt = living->getLastHurtMob() != nullptr;
											  }
											  return hurt && !fox.trusts(target);
										  }),
			  _fox(fox) {}
		bool canUse() override {
			if (_randomInterval > 0 && _mob.random().nextInt(_randomInterval) != 0) return false;
			Level& level = _fox.level();
			for (int i = 0; i < 2; i++) {
				if (!_fox.trusted(i).isSet()) continue;
				Actor* trusted = level.actorByRef(_fox.trusted(i));
				if (!trusted) continue;
				Actor* by		 = nullptr;
				int	   timestamp = 0;
				if (Player* player = trusted->asPlayer()) {
					by		  = player->lastHurtByMob().isSet() ? level.actorByRef(player->lastHurtByMob()) : nullptr;
					timestamp = player->lastHurtByMobTimestamp();
				} else if (LivingEntity* living = trusted->asLiving()) {
					by		  = living->getLastHurtByMob();
					timestamp = living->getLastHurtByMobTimestamp();
				}
				_trustedTimestamp = timestamp;
				_hurtBy			  = by ? EntityRef(*by) : EntityRef();
				return timestamp != _timestamp && canAttack(by, _targetConditions);
			}
			return false;
		}
		void start() override {
			Actor* by = _hurtBy.isSet() ? _fox.level().actorByRef(_hurtBy) : nullptr;
			setTarget(by);
			_timestamp = _trustedTimestamp;
			_fox.playSound("minecraft:entity.fox.aggro", 1.0F, 1.0F);
			_fox.setDefending(true);
			_fox.wakeUp();
			NearestAttackableTargetGoal::start();
		}

	  private:
		Fox&	  _fox;
		EntityRef _hurtBy;
		int		  _timestamp = 0, _trustedTimestamp = 0;
	};
} // namespace

Fox::Fox(Level& level, int typeId) : Animal(level, typeId) {
	setLookControl(std::make_unique<FoxLookControl>(*this));
	setMoveControl(std::make_unique<FoxMoveControl>(*this));
	setPathfindingMalus(PathType::DangerOther, 0.0F);
	setPathfindingMalus(PathType::DamageOther, 0.0F);
	setCanPickUpLoot(true);
}

void Fox::registerGoals() {
	_landTargetGoal = std::make_shared<NearestAttackableTargetGoal>(*this, "Animal", 10, false, false, [](Actor& target, Level&) { return isStalkablePrey(target); });
	_turtleEggTargetGoal = std::make_shared<NearestAttackableTargetGoal>(*this, "Turtle", 10, false, false, [](Actor& target, Level&) {
		LivingEntity* living = target.asLiving();
		return living && living->isBaby() && !living->touchingWater();
	});
	_fishTargetGoal = std::make_shared<NearestAttackableTargetGoal>(*this, "AbstractFish", 20, false, false, [](Actor& target, Level&) {
		return dynamic_cast<AbstractSchoolingFish*>(target.asEntity()) != nullptr;
	});
	_goalSelector.addGoal(0, std::make_unique<FoxFloatGoal>(*this));
	// ClimbOnTopOfPowderSnowGoal: powder snow isn't walked on yet
	_goalSelector.addGoal(1, std::make_unique<FaceplantGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<FoxPanicGoal>(*this, 2.2));
	_goalSelector.addGoal(3, std::make_unique<FoxBreedGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<AvoidEntityGoal>(*this, "Player", 16.0F, 1.6, 1.4, [this](Actor& actor) {
		Player* player = actor.asPlayer();
		return !isDiscrete(actor) && player && !player->isCreative() && !player->isSpectator() && !trusts(actor) && !isDefending();
	}));
	_goalSelector.addGoal(4, std::make_unique<AvoidEntityGoal>(*this, "Wolf", 8.0F, 1.6, 1.4, [this](Actor& actor) {
		auto* wolf = dynamic_cast<TamableAnimal*>(actor.asEntity());
		return wolf && !wolf->isTame() && !isDefending();
	}));
	_goalSelector.addGoal(4, std::make_unique<AvoidEntityGoal>(*this, "PolarBear", 8.0F, 1.6, 1.4, [this](Actor&) { return !isDefending(); }));
	_goalSelector.addGoal(5, std::make_unique<StalkPreyGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<FoxPounceGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<SeekShelterGoal>(*this, 1.25));
	_goalSelector.addGoal(7, std::make_unique<FoxMeleeAttackGoal>(*this, static_cast<double>(1.2F), true));
	_goalSelector.addGoal(7, std::make_unique<SleepGoal>(*this));
	_goalSelector.addGoal(8, std::make_unique<FoxFollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(9, std::make_unique<FoxStrollThroughVillageGoal>(*this, 200));
	_goalSelector.addGoal(10, std::make_unique<FoxEatBerriesGoal>(*this, static_cast<double>(1.2F), 12, 1));
	_goalSelector.addGoal(10, std::make_unique<LeapAtTargetGoal>(*this, 0.4F));
	_goalSelector.addGoal(11, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(11, std::make_unique<FoxSearchForItemsGoal>(*this));
	_goalSelector.addGoal(12, std::make_unique<FoxLookAtPlayerGoal>(*this, 24.0F));
	_goalSelector.addGoal(13, std::make_unique<PerchAndSearchGoal>(*this));
	_targetSelector.addGoal(3, std::make_unique<DefendTrustedTargetGoal>(*this));
}

void Fox::setTargetGoals() {
	if (_targetGoalsSet || !_landTargetGoal) return;
	_targetGoalsSet = true;
	if (_variant == VARIANT_RED) {
		_targetSelector.addSharedGoal(4, _landTargetGoal);
		_targetSelector.addSharedGoal(4, _turtleEggTargetGoal);
		_targetSelector.addSharedGoal(6, _fishTargetGoal);
	} else {
		_targetSelector.addSharedGoal(4, _fishTargetGoal);
		_targetSelector.addSharedGoal(6, _landTargetGoal);
		_targetSelector.addSharedGoal(6, _turtleEggTargetGoal);
	}
}

void Fox::setFlag(int flag, bool value) {
	uint8_t flags = static_cast<uint8_t>(value ? _foxFlags | flag : _foxFlags & ~flag);
	if (flags == _foxFlags) return;
	_foxFlags = flags;
	markData(DATA_FLAGS);
}

void Fox::setVariant(int variant) {
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_TYPE);
}

void Fox::clearStates() {
	setIsInterested(false);
	setIsCrouching(false);
	setSitting(false);
	setSleeping(false);
	setDefending(false);
	setFaceplanted(false);
}

bool Fox::trusts(const Actor& actor) const {
	for (const EntityRef& ref : _trusted) {
		if (ref.isSet() && ref.uuid == actor.uuid()) return true;
	}
	return false;
}

void Fox::addTrusted(const EntityRef& ref) {
	int index = _trusted[0].isSet() ? 1 : 0;
	_trusted[index] = ref;
	markData(index == 0 ? DATA_TRUSTED_0 : DATA_TRUSTED_1);
}

bool Fox::isPathClear(Actor& target) {
	double dz = target.position().z - _position.z, dx = target.position().x - _position.x;
	double slope = dz / dx;
	for (int i = 0; i < 6; i++) {
		double z = slope == 0.0 ? 0.0 : dz * (i / 6.0F);
		double x = slope == 0.0 ? dx * (i / 6.0F) : z / slope;
		for (int up = 1; up < 4; up++) {
			int state = _level.getBlockState({Mth::floor(_position.x + x), Mth::floor(_position.y + up), Mth::floor(_position.z + z)});
			if (!_level.gameData().getStateProperties(state).replaceable) return false;
		}
	}
	return true;
}

void Fox::setTarget(Actor* target) {
	if (isDefending() && !target) setDefending(false);
	Animal::setTarget(target);
}

bool Fox::isFood(const ItemStack& stack) const { return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:fox_food", stack.item); }

bool Fox::isFoodItem(const ItemStack& stack) const {
	if (stack.isEmpty()) return false;
	const GameData::ItemProperties* props = _level.gameData().getItemProperties(stack.item);
	return props && props->food;
}

bool Fox::canHoldItem(const ItemStack& stack) {
	const ItemStack& held = getItemBySlot(EquipmentSlot::MainHand);
	return held.isEmpty() || (_ticksSinceEaten > 0 && isFoodItem(stack) && !isFoodItem(held));
}

void Fox::spitOutItem(const ItemStack& stack) {
	if (stack.isEmpty()) return;
	const float toRadians = static_cast<float>(M_PI / 180.0);
	Vec3		look{-Mth::sin(_yRot * toRadians) * Mth::cos(_xRot * toRadians), 0.0, Mth::cos(_yRot * toRadians) * Mth::cos(_xRot * toRadians)};
	auto		item = ItemEntity::create(_level, {_position.x + look.x, _position.y + 1.0, _position.z + look.z}, stack);
	item->setPickupDelay(40);
	playSound("minecraft:entity.fox.spit", 1.0F, 1.0F);
	_level.entities().add(std::move(item));
}

void Fox::pickUpItem(ItemEntity& item) {
	ItemStack stack = item.item();
	if (!canHoldItem(stack)) return;
	if (stack.count > 1) _level.entities().add(ItemEntity::create(_level, _position, stack.copyWithCount(stack.count - 1)));
	spitOutItem(getItemBySlot(EquipmentSlot::MainHand));
	Buffer take;
	take.writeVarInt(item.id());
	take.writeVarInt(_id);
	take.writeVarInt(1);
	_level.entities().broadcast(item, PacketId::Play::Clientbound::TAKE_ITEM_ENTITY, take);
	setItemSlot(EquipmentSlot::MainHand, stack.copyWithCount(1));
	setGuaranteedDrop(EquipmentSlot::MainHand);
	item.discard();
	_ticksSinceEaten = 0;
}

void Fox::dropAllDeathLoot(const Combat::DamageSource& source) {
	ItemStack held = getItemBySlot(EquipmentSlot::MainHand);
	if (!held.isEmpty()) {
		spawnAtLocation(held);
		setItemSlot(EquipmentSlot::MainHand, ItemStack());
	}
	Animal::dropAllDeathLoot(source);
}

void Fox::aiStep() {
	if (isAlive() && isEffectiveAi()) {
		_ticksSinceEaten++;
		ItemStack held = getItemBySlot(EquipmentSlot::MainHand);
		// canEat: food in its mouth, no target, on the ground, awake
		if (isFoodItem(held) && !getTarget() && _onGround && !isSleeping()) {
			if (_ticksSinceEaten > 600) {
				// ItemStack.finishUsingItem: one eaten (its consume effects aren't applied to mobs yet)
				held.shrink(1);
				setItemSlot(EquipmentSlot::MainHand, held);
				_ticksSinceEaten = 0;
			} else if (_ticksSinceEaten > 560 && _random.nextFloat() < 0.1F) {
				playEatingSound();
				broadcastEntityEvent(EVENT_EATING);
			}
		}
		Actor* target = getTarget();
		if (!target || !target->isAlive()) {
			setIsCrouching(false);
			setIsInterested(false);
		}
	}
	if (isSleeping() || isImmobile()) {
		_jumping = false;
		_xxa	 = 0.0F;
		_zza	 = 0.0F;
	}
	Animal::aiStep();
	if (isDefending() && _random.nextFloat() < 0.05F) playSound("minecraft:entity.fox.aggro", 1.0F, 1.0F);
}

void Fox::tick() {
	Animal::tick();
	if (isRemoved()) return;
	if (isEffectiveAi()) {
		bool water = isInWater();
		if (water || getTarget() || _level.isThundering()) wakeUp();
		if (water || isSleeping()) setSitting(false);
		if (isFaceplanted() && _level.random().nextFloat() < 0.2F) {
			BlockPos pos = blockPosition();
			_level.levelEvent(nullptr, LEVEL_EVENT_DESTROY_BLOCK, pos, _level.getBlockState(pos));
		}
	}
	if (isCrouching()) {
		_crouchAmount = std::min(3.0F, _crouchAmount + 0.2F);
	} else {
		_crouchAmount = 0.0F;
	}
}

std::string Fox::ambientSoundName() {
	if (isSleeping()) return "minecraft:entity.fox.sleep";
	if (!_level.isBrightOutside() && _random.nextFloat() < 0.1F) {
		// No player within 16 blocks: it screeches
		AABB area  = boundingBox().inflate(16.0);
		bool found = false;
		for (const auto& player : _level.players()) found |= !player->isSpectator() && player->boundingBox().intersects(area);
		if (!found) return "minecraft:entity.fox.screech";
	}
	return "minecraft:entity.fox.ambient";
}

void Fox::populateDefaultEquipmentSlots(DifficultyInstance&) {
	JavaRandom& random = _level.random();
	if (!(random.nextFloat() < 0.2F)) return;
	float		roll = random.nextFloat();
	const char* item;
	if (roll < 0.05F) {
		item = "minecraft:emerald";
	} else if (roll < 0.2F) {
		item = "minecraft:egg";
	} else if (roll < 0.4F) {
		item = random.nextBoolean() ? "minecraft:rabbit_foot" : "minecraft:rabbit_hide";
	} else if (roll < 0.6F) {
		item = "minecraft:wheat";
	} else if (roll < 0.8F) {
		item = "minecraft:leather";
	} else {
		item = "minecraft:feather";
	}
	setItemSlot(EquipmentSlot::MainHand, ItemStack(_level.gameData().getStaticId("minecraft:item", item), 1));
}

std::shared_ptr<SpawnGroupData> Fox::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	bool snowy	 = _level.gameData().isInTag("minecraft:worldgen/biome", "minecraft:spawns_snow_foxes", _level.getBiomeId(blockPosition()));
	int	 variant = snowy ? VARIANT_SNOW : VARIANT_RED;
	bool baby	 = false;
	if (auto* foxes = dynamic_cast<FoxGroupData*>(group.get())) {
		variant = foxes->variant;
		baby	= foxes->groupSize >= 2;
	} else {
		group = std::make_shared<FoxGroupData>(variant);
	}
	setVariant(variant);
	if (baby) setAge(BABY_START_AGE);
	setTargetGoals();
	populateDefaultEquipmentSlots(difficulty);
	return Animal::finalizeSpawn(difficulty, reason, group);
}

std::unique_ptr<Mob> Fox::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child = Animal::getBreedOffspring(partner);
	auto*				 kit   = dynamic_cast<Fox*>(child.get());
	auto*				 other = dynamic_cast<Fox*>(&partner);
	if (kit && other) {
		kit->setVariant(_random.nextBoolean() ? _variant : other->_variant);
		kit->setTargetGoals();
	}
	return child;
}

void Fox::spawnChildFromBreeding(Animal& partner) {
	// FoxBreedGoal.breed: the kit trusts the players that bred its parents
	std::unique_ptr<Mob> child = getBreedOffspring(partner);
	if (!child) return;
	if (auto* kit = dynamic_cast<Fox*>(child.get())) {
		const EntityRef& mine	= loveCause();
		const EntityRef& theirs = partner.loveCause();
		if (mine.isSet()) kit->addTrusted(mine);
		if (theirs.isSet() && !(mine.isSet() && mine.uuid == theirs.uuid)) kit->addTrusted(theirs);
	}
	setAge(PARENT_AGE_AFTER_BREEDING);
	partner.setAge(PARENT_AGE_AFTER_BREEDING);
	resetLove();
	partner.resetLove();
	child->setBaby(true);
	child->snapTo(_position, 0.0F, 0.0F);
	_level.addFreshEntity(std::move(child));
	broadcastEntityEvent(EVENT_IN_LOVE_HEARTS);
	_level.entities().add(std::make_unique<ExperienceOrb>(_level, _position, _random.nextInt(7) + 1));
}

std::string Fox::lootComponent(const std::string& component) const {
	if (component == "minecraft:fox/variant") return _variant == VARIANT_SNOW ? "snow" : "red";
	return Animal::lootComponent(component);
}

void Fox::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_TYPE, onlyNonDefault, _variant == VARIANT_RED)) writeIntData(buf, DATA_TYPE, _variant);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _foxFlags == 0)) {
		buf.writeUByte(DATA_FLAGS);
		buf.writeVarInt(SERIALIZER_BYTE);
		buf.writeUByte(_foxFlags);
	}
	for (int i = 0; i < 2; i++) {
		int id = i == 0 ? DATA_TRUSTED_0 : DATA_TRUSTED_1;
		if (!wantsData(mask, id, onlyNonDefault, !_trusted[i].isSet())) continue;
		buf.writeUByte(static_cast<uint8_t>(id));
		buf.writeVarInt(SERIALIZER_OPTIONAL_LIVING_REFERENCE);
		buf.writeBool(_trusted[i].isSet());
		if (_trusted[i].isSet()) buf.writeUUID(_trusted[i].uuid);
	}
}

void Fox::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	for (const EntityRef& ref : _trusted) {
		buf.writeBool(ref.isSet());
		if (ref.isSet()) buf.writeUUID(ref.uuid);
	}
	buf.writeUByte(static_cast<uint8_t>(_variant));
	buf.writeUByte(static_cast<uint8_t>(_foxFlags & (FLAG_SLEEPING | FLAG_SITTING | FLAG_CROUCHING)));
}

void Fox::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown fox data version");
	for (EntityRef& ref : _trusted) {
		ref.clear();
		if (buf.readBool()) {
			ref.uuid = buf.readUUID();
			ref.id	 = -2;
		}
	}
	_variant  = buf.readUByte();
	_foxFlags = buf.readUByte();
	setTargetGoals();
	_dirtyData = 0;
}
