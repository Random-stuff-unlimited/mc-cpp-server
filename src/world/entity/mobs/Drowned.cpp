#include "world/entity/mobs/Drowned.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/Arrow.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"

#include <cmath>

namespace {
	// Drowned.DrownedMoveControl: swims straight toward the wanted position when it wants to swim, else walks (sinking)
	class DrownedMoveControl : public MoveControl {
	  public:
		explicit DrownedMoveControl(Drowned& drowned) : MoveControl(drowned), _drowned(drowned) {}
		void tick() override {
			Actor* target = _drowned.getTarget();
			if (_drowned.wantsToSwim() && _drowned.isInWaterNow()) {
				if ((target && target->position().y > _drowned.position().y) || _drowned.searchingForLand()) {
					_drowned.setDeltaMovement(_drowned.deltaMovement() + Vec3{0.0, 0.002, 0.0});
				}
				if (_operation != Operation::MoveTo || _drowned.navigation().isDone()) {
					_drowned.setSpeed(0.0F);
					return;
				}
				double dx = _wantedX - _drowned.position().x, dy = _wantedY - _drowned.position().y, dz = _wantedZ - _drowned.position().z;
				double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
				dy /= distance;
				float yaw = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
				_drowned.setYRotPublic(rotlerp(_drowned.yRot(), yaw, 90.0F));
				_drowned.setYBodyRot(_drowned.yRot());
				float wanted = static_cast<float>(_speedModifier * _drowned.getAttributeValue(_drowned.attributeIds().movementSpeed));
				float speed	 = _drowned.speed() + 0.125F * (wanted - _drowned.speed()); // Mth.lerp
				_drowned.setSpeed(speed);
				_drowned.setDeltaMovement(_drowned.deltaMovement() + Vec3{speed * dx * 0.005, speed * dy * 0.1, speed * dz * 0.005});
			} else {
				if (!_drowned.onGround()) _drowned.setDeltaMovement(_drowned.deltaMovement() + Vec3{0.0, -0.008, 0.0});
				MoveControl::tick();
			}
		}

	  private:
		Drowned& _drowned;
	};

	class DrownedAttackGoal : public ZombieAttackGoal {
	  public:
		DrownedAttackGoal(Drowned& drowned, double speed, bool followEvenIfNotSeen) : ZombieAttackGoal(drowned, speed, followEvenIfNotSeen), _drowned(drowned) {}
		bool canUse() override { return ZombieAttackGoal::canUse() && _drowned.okTarget(_drowned.getTarget()); }
		bool canContinueToUse() override { return ZombieAttackGoal::canContinueToUse() && _drowned.okTarget(_drowned.getTarget()); }

	  private:
		Drowned& _drowned;
	};

	// By day, out of the water: back to a water block nearby
	class DrownedGoToWaterGoal : public Goal {
	  public:
		DrownedGoToWaterGoal(Mob& mob, double speed) : _mob(mob), _speedModifier(speed) { setFlags({Flag::Move}); }
		bool canUse() override {
			Level& level = _mob.level();
			if (!level.isBrightOutside() || _mob.isInWaterNow()) return false;
			JavaRandom& random = _mob.random();
			BlockPos	pos	   = _mob.blockPosition();
			int			water  = level.gameData().getStaticId("minecraft:block", "minecraft:water");
			for (int i = 0; i < 10; i++) {
				BlockPos at = pos.offset(random.nextInt(20) - 10, 2 - random.nextInt(8), random.nextInt(20) - 10);
				if (level.blocks().blockOf(level.getBlockState(at)) == water) {
					_wanted = {at.x + 0.5, static_cast<double>(at.y), at.z + 0.5};
					return true;
				}
			}
			return false;
		}
		bool canContinueToUse() override { return !_mob.navigation().isDone(); }
		void start() override { _mob.navigation().moveTo(_wanted.x, _wanted.y, _wanted.z, _speedModifier); }

	  private:
		Mob&   _mob;
		Vec3   _wanted;
		double _speedModifier;
	};

	// At night, near the surface: to a block of the shore it can stand on
	class DrownedGoToBeachGoal : public MoveToBlockGoal {
	  public:
		DrownedGoToBeachGoal(Drowned& drowned, double speed) : MoveToBlockGoal(drowned, speed, 8, 2), _drowned(drowned) {}
		bool canUse() override {
			return MoveToBlockGoal::canUse() && !_drowned.level().isBrightOutside() && _drowned.isInWaterNow() &&
				   _drowned.position().y >= _drowned.level().seaLevel() - 3;
		}
		void start() override {
			_drowned.setSearchingForLand(false);
			MoveToBlockGoal::start();
		}

	  protected:
		bool isValidTarget(Level& level, const BlockPos& pos) override {
			BlockPos above = pos.above();
			if (!level.blocks().isAir(level.getBlockState(above)) || !level.blocks().isAir(level.getBlockState(above.above()))) return false;
			// BlockState.entityCanStandOn: its collision's top face is full
			for (const GameData::Box& box : level.gameData().getCollisionShape(level.getBlockState(pos))) {
				if (box.minX <= 0.0 && box.maxX >= 1.0 && box.minZ <= 0.0 && box.maxZ >= 1.0 && box.maxY >= 1.0) return true;
			}
			return false;
		}

	  private:
		Drowned& _drowned;
	};

	// At night, deep: swims up toward the surface
	class DrownedSwimUpGoal : public Goal {
	  public:
		DrownedSwimUpGoal(Drowned& drowned, double speed, int seaLevel) : _drowned(drowned), _speedModifier(speed), _seaLevel(seaLevel) {}
		bool canUse() override { return !_drowned.level().isBrightOutside() && _drowned.isInWaterNow() && _drowned.position().y < _seaLevel - 2; }
		bool canContinueToUse() override { return canUse() && !_stuck; }
		void start() override {
			_drowned.setSearchingForLand(true);
			_stuck = false;
		}
		void stop() override { _drowned.setSearchingForLand(false); }
		void tick() override {
			if (_drowned.position().y < _seaLevel - 1 && (_drowned.navigation().isDone() || _drowned.closeToNextPos())) {
				std::optional<Vec3> pos =
						RandomPos::defaultPosTowards(_drowned, 4, 8, {_drowned.position().x, static_cast<double>(_seaLevel - 1), _drowned.position().z}, M_PI / 2);
				if (!pos) {
					_stuck = true;
					return;
				}
				_drowned.navigation().moveTo(pos->x, pos->y, pos->z, _speedModifier);
			}
		}

	  private:
		Drowned& _drowned;
		double	 _speedModifier;
		int		 _seaLevel;
		bool	 _stuck = false;
	};

	// Throws its trident from 10 blocks, every 2 seconds
	class DrownedTridentAttackGoal : public RangedAttackGoal {
	  public:
		DrownedTridentAttackGoal(Drowned& drowned, double speed, int interval, float radius) : RangedAttackGoal(drowned, speed, interval, radius), _drowned(drowned) {}
		bool canUse() override { return RangedAttackGoal::canUse() && _drowned.isHolding("minecraft:trident") && holdsTridentInMainHand(); }
		void start() override {
			_drowned.setAggressive(true);
			_drowned.startUsingItem(0);
		}
		void stop() override {
			RangedAttackGoal::stop();
			_drowned.stopUsingItem();
			_drowned.setAggressive(false);
		}

	  private:
		Drowned& _drowned;
		bool	 holdsTridentInMainHand() {
			const ItemStack& held = _drowned.getMainHandItem();
			return !held.isEmpty() && held.item == _drowned.level().gameData().getStaticId("minecraft:item", "minecraft:trident");
		}
	};
} // namespace

Drowned::Drowned(Level& level, int typeId) : Zombie(level, typeId) {
	setMoveControl(std::make_unique<DrownedMoveControl>(*this));
	setPathfindingMalus(PathType::Water, 0.0F);
}

std::unique_ptr<PathNavigation> Drowned::createNavigation() { return std::make_unique<AmphibiousPathNavigation>(*this, _level); }

void Drowned::addBehaviourGoals() {
	_goalSelector.addGoal(1, std::make_unique<DrownedGoToWaterGoal>(*this, 1.0));
	_goalSelector.addGoal(2, std::make_unique<DrownedTridentAttackGoal>(*this, 1.0, 40, 10.0F));
	_goalSelector.addGoal(2, std::make_unique<DrownedAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(5, std::make_unique<DrownedGoToBeachGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<DrownedSwimUpGoal>(*this, 1.0, _level.seaLevel()));
	_goalSelector.addGoal(7, std::make_unique<RandomStrollGoal>(*this, 1.0));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Drowned"});
	hurtBy->setAlertOthers({"ZombifiedPiglin"});
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																		   [this](Actor& target, Level&) { return okTarget(&target); }));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractVillager", false));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", true));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Axolotl", 10, true, false));
	_targetSelector.addGoal(5, std::make_unique<NearestAttackableTargetGoal>(*this, "Turtle", 10, true, false, [](Actor& target, Level&) {
		LivingEntity* turtle = target.asLiving();
		return turtle && turtle->isBaby() && !turtle->isInWaterNow();
	}));
}

void Drowned::populateDefaultEquipmentSlots(DifficultyInstance&) {
	JavaRandom& random = _level.random();
	if (random.nextFloat() > 0.9) {
		const char* item = random.nextInt(16) < 10 ? "minecraft:trident" : "minecraft:fishing_rod";
		setItemSlot(EquipmentSlot::MainHand, ItemStack(_level.gameData().getStaticId("minecraft:item", item), 1));
	}
}

std::shared_ptr<SpawnGroupData> Drowned::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group = Zombie::finalizeSpawn(difficulty, reason, group);
	if (getItemBySlot(EquipmentSlot::OffHand).isEmpty() && _level.random().nextFloat() < 0.03F) {
		setItemSlot(EquipmentSlot::OffHand, ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:nautilus_shell"), 1));
		setGuaranteedDrop(EquipmentSlot::OffHand);
	}
	return group;
}

bool Drowned::canReplaceCurrentItem(const ItemStack& candidate, const ItemStack& current, EquipmentSlot slot) {
	if (!current.isEmpty() && current.item == _level.gameData().getStaticId("minecraft:item", "minecraft:nautilus_shell")) return false;
	return Zombie::canReplaceCurrentItem(candidate, current, slot);
}

bool Drowned::okTarget(Actor* target) {
	if (!target) return false;
	if (!_level.isBrightOutside()) return true;
	if (Player* player = target->asPlayer()) return player->survival().inWater;
	LivingEntity* living = target->asLiving();
	return living && living->isInWaterNow();
}

bool Drowned::wantsToSwim() {
	if (_searchingForLand) return true;
	Actor* target = getTarget();
	if (!target) return false;
	if (Player* player = target->asPlayer()) return player->survival().inWater;
	LivingEntity* living = target->asLiving();
	return living && living->isInWaterNow();
}

void Drowned::travel(const Vec3& input) {
	if (isUnderWater() && wantsToSwim()) {
		moveRelative(0.01F, input);
		move(_delta);
		_delta = _delta.scale(0.9);
	} else {
		Zombie::travel(input);
	}
}

void Drowned::aiStep() {
	// updateSwimming: the swimming flag for its viewers
	bool swimming = isEffectiveAi() && isUnderWater() && wantsToSwim();
	if (swimming != _swimming) {
		_swimming = swimming;
		setSharedFlag(4, swimming);
	}
	Zombie::aiStep();
}

bool Drowned::closeToNextPos() {
	Path* path = navigation().getPath();
	if (!path) return false;
	const BlockPos& target = path->getTarget();
	return distanceToSqr(Vec3{static_cast<double>(target.x), static_cast<double>(target.y), static_cast<double>(target.z)}) < 4.0;
}

void Drowned::performRangedAttack(Actor& target, float) {
	const ItemStack& held	 = getMainHandItem();
	int				 trident = _level.gameData().getStaticId("minecraft:item", "minecraft:trident");
	auto	thrown = std::make_unique<ThrownTrident>(_level, *this, !held.isEmpty() && held.item == trident ? held : ItemStack(trident, 1));
	AABB	box	   = target.boundingBox();
	double	dx	   = target.position().x - _position.x;
	double	dy	   = target.position().y + (box.maxY - box.minY) / 3.0 - thrown->position().y;
	double	dz	   = target.position().z - _position.z;
	double	d	   = std::sqrt(dx * dx + dz * dz);
	thrown->shoot(dx, dy + d * 0.2F, dz, 1.6F, static_cast<float>(14 - _level.difficulty() * 4));
	_level.entities().add(std::move(thrown));
	playSound("minecraft:entity.drowned.shoot", 1.0F, 1.0F / (_random.nextFloat() * 0.4F + 0.8F));
}
