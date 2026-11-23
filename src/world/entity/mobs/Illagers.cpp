#include "world/entity/mobs/Illagers.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Arrow.hpp"
#include "world/entity/EntityManager.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int DYE_BLUE = 11, DYE_RED = 14;

	ItemStack itemOf(Level& level, const char* name) { return ItemStack(level.gameData().getStaticId("minecraft:item", name), 1); }
	bool	  isItem(Level& level, const ItemStack& stack, const char* name) {
		 return !stack.isEmpty() && stack.item == level.gameData().getStaticId("minecraft:item", name);
	}
	bool isEmptyBlock(Level& level, const BlockPos& pos) { return level.blocks().isAir(level.getBlockState(pos)); }

	// Shoots an arrow from a mob at its target like AbstractSkeleton.performRangedAttack (the crossbow's too)
	void shootArrowAt(Mob& mob, Actor& target, float velocity, float power, const ItemStack& weapon, bool crit) {
		Level& level = mob.level();
		auto   arrow = std::make_unique<Arrow>(level, mob, itemOf(level, "minecraft:arrow"), weapon.isEmpty() ? nullptr : &weapon);
		if (power > 0.0F) arrow->setBaseDamageFromMob(power);
		arrow->setCritArrow(crit);
		AABB   box = target.boundingBox();
		double dx  = target.position().x - mob.position().x;
		double dy  = target.position().y + (box.maxY - box.minY) / 3.0 - arrow->position().y;
		double dz  = target.position().z - mob.position().z;
		double d   = std::sqrt(dx * dx + dz * dz);
		arrow->shoot(dx, dy + d * 0.2F, dz, velocity, static_cast<float>(14 - level.difficulty() * 4));
		level.entities().add(std::move(arrow));
	}

	// ----- Pillager -----

	// RangedCrossbowAttackGoal: walks up to its target, loads its crossbow (25 ticks), waits one or two seconds and
	// fires once it sees it
	class RangedCrossbowAttackGoal : public Goal {
	  public:
		RangedCrossbowAttackGoal(Pillager& mob, double speed, float range) : _mob(mob), _speedModifier(speed), _attackRadiusSqr(range * range) {
			setFlags({Flag::Move, Flag::Look});
		}
		bool canUse() override { return isValidTarget() && _mob.isHoldingCrossbow(); }
		bool canContinueToUse() override { return isValidTarget() && (canUse() || !_mob.navigation().isDone()) && _mob.isHoldingCrossbow(); }
		void start() override { _mob.setAggressive(true); }
		void stop() override {
			_mob.setAggressive(false);
			_mob.setTarget(nullptr);
			_seeTime = 0;
			if (_mob.isUsingItem()) {
				_mob.stopUsingItem();
				_mob.setChargingCrossbow(false);
			}
			_state = State::Uncharged;
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _mob.getTarget();
			if (!target) return;
			bool hasLineOfSight = _mob.sensing().hasLineOfSight(*target);
			if (hasLineOfSight != (_seeTime > 0)) _seeTime = 0;
			_seeTime += hasLineOfSight ? 1 : -1;
			double distance	 = _mob.distanceToSqr(*target);
			bool   approach = (distance > _attackRadiusSqr || _seeTime < 5) && _attackDelay == 0;
			if (approach) {
				if (--_updatePathDelay <= 0) {
					_mob.navigation().moveTo(*target, _state == State::Uncharged ? _speedModifier : _speedModifier * 0.5);
					_updatePathDelay = 20 + _mob.random().nextInt(21); // PATHFINDING_DELAY_RANGE
				}
			} else {
				_updatePathDelay = 0;
				_mob.navigation().stop();
			}
			_mob.lookControl().setLookAt(*target, 30.0F, 30.0F);
			switch (_state) {
				case State::Uncharged:
					if (!approach) {
						_mob.startUsingItem(isItem(_mob.level(), _mob.getMainHandItem(), "minecraft:crossbow") ? 0 : 1);
						_state = State::Charging;
						_mob.setChargingCrossbow(true);
					}
					break;
				case State::Charging:
					if (!_mob.isUsingItem()) _state = State::Uncharged;
					if (_mob.getTicksUsingItem() >= 25) { // CrossbowItem.getChargeDuration, without quick charge
						_mob.stopUsingItem();
						_state		 = State::Charged;
						_attackDelay = 20 + _mob.random().nextInt(20);
						_mob.setChargingCrossbow(false);
						_mob.playSound("minecraft:item.crossbow.loading_end", 1.0F, 1.0F);
					}
					break;
				case State::Charged:
					if (--_attackDelay <= 0) {
						_attackDelay = 0;
						_state		 = State::ReadyToAttack;
					}
					break;
				case State::ReadyToAttack:
					if (hasLineOfSight) {
						_mob.performRangedAttack(*target, 1.0F);
						_state = State::Uncharged;
					}
					break;
			}
		}

	  private:
		enum class State { Uncharged, Charging, Charged, ReadyToAttack };
		Pillager& _mob;
		State	  _state = State::Uncharged;
		double	  _speedModifier;
		float	  _attackRadiusSqr;
		int		  _seeTime = 0, _attackDelay = 0, _updatePathDelay = 0;

		bool isValidTarget() {
			Actor* target = _mob.getTarget();
			return target && target->isAlive();
		}
	};

	// ----- Evoker's spells -----

	class EvokerCastingSpellGoal : public SpellcasterCastingSpellGoal {
	  public:
		using SpellcasterCastingSpellGoal::SpellcasterCastingSpellGoal;
	};

	// Evoker.EvokerAttackSpellGoal: the fangs
	class EvokerAttackSpellGoal : public SpellcasterUseSpellGoal {
	  public:
		using SpellcasterUseSpellGoal::SpellcasterUseSpellGoal;

	  protected:
		int							castingTime() const override { return 40; }
		int							castingInterval() const override { return 100; }
		const char*					spellPrepareSound() const override { return "minecraft:entity.evoker.prepare_attack"; }
		SpellcasterIllager::Spell	spell() const override { return SpellcasterIllager::Spell::Fangs; }
		void						performSpellCasting() override {
			   Actor* target = _caster.getTarget();
			   if (!target) return;
			   double minY = std::min(target->position().y, _caster.position().y);
			   double maxY = std::max(target->position().y, _caster.position().y) + 1.0;
			   float  angle = static_cast<float>(Mth::atan2(target->position().z - _caster.position().z, target->position().x - _caster.position().x));
			   if (_caster.distanceToSqr(*target) < 9.0) {
				   for (int i = 0; i < 5; i++) {
					   float a = angle + i * (float)M_PI * 0.4F;
					   createSpellEntity(_caster.position().x + Mth::cos(a) * 1.5, _caster.position().z + Mth::sin(a) * 1.5, minY, maxY, a, 0);
				   }
				   for (int i = 0; i < 8; i++) {
					   float a = angle + i * (float)M_PI * 2.0F / 8.0F + (float)M_PI * 2.0F / 5.0F;
					   createSpellEntity(_caster.position().x + Mth::cos(a) * 2.5, _caster.position().z + Mth::sin(a) * 2.5, minY, maxY, a, 3);
				   }
			   } else {
				   for (int i = 0; i < 16; i++) {
					   double distance = 1.25 * (i + 1);
					   createSpellEntity(_caster.position().x + Mth::cos(angle) * distance, _caster.position().z + Mth::sin(angle) * distance, minY,
										 maxY, angle, i);
				   }
			   }
		}

	  private:
		// The fangs on the first block with a sturdy top from maxY down to minY
		void createSpellEntity(double x, double z, double minY, double maxY, float yRot, int warmupDelay) {
			Level&	 level = _caster.level();
			BlockPos pos{Mth::floor(x), Mth::floor(maxY), Mth::floor(z)};
			bool	 found	 = false;
			double	 yOffset = 0.0;
			do {
				BlockPos below = pos.below();
				int		 state = level.getBlockState(below);
				if (level.gameData().isFaceSturdy(state, static_cast<int>(Direction::Up), GameData::SupportType::Full)) {
					if (!isEmptyBlock(level, pos)) {
						double top = 0.0;
						for (const GameData::Box& box : level.gameData().getCollisionShape(level.getBlockState(pos))) top = std::max(top, box.maxY);
						yOffset = top;
					}
					found = true;
					break;
				}
				pos = below;
			} while (pos.y >= Mth::floor(minY) - 1);
			if (found) {
				level.entities().add(std::make_unique<EvokerFangs>(level, x, pos.y + yOffset, z, yRot * (180.0F / (float)M_PI), warmupDelay, &_caster));
			}
		}
	};

	// Evoker.EvokerSummonSpellGoal: three vexes, while fewer than 1 to 8 are around
	class EvokerSummonSpellGoal : public SpellcasterUseSpellGoal {
	  public:
		using SpellcasterUseSpellGoal::SpellcasterUseSpellGoal;
		bool canUse() override {
			if (!SpellcasterUseSpellGoal::canUse()) return false;
			int vexes = static_cast<int>(actorsOfClass(_caster.level(), "Vex", _caster.boundingBox().inflate(16.0)).size());
			return _caster.random().nextInt(8) + 1 > vexes;
		}

	  protected:
		int							castingTime() const override { return 100; }
		int							castingInterval() const override { return 340; }
		const char*					spellPrepareSound() const override { return "minecraft:entity.evoker.prepare_summon"; }
		SpellcasterIllager::Spell	spell() const override { return SpellcasterIllager::Spell::SummonVex; }
		void						performSpellCasting() override {
			   Level&	   level = _caster.level();
			   JavaRandom& random = _caster.random();
			   int		   vexType = level.gameData().getStaticId("minecraft:entity_type", "minecraft:vex");
			   for (int i = 0; i < 3; i++) {
				   BlockPos pos = _caster.blockPosition().offset(-2 + random.nextInt(5), 1, -2 + random.nextInt(5));
				   Mob*		mob = level.mobs().spawn(level, vexType, pos, MobRegistry::SpawnReason::MobSummoned);
				   if (auto* vex = dynamic_cast<Vex*>(mob)) {
					   vex->setOwner(&_caster);
					   vex->setBoundOrigin(pos);
					   vex->setLimitedLife(20 * (30 + random.nextInt(90)));
				   }
			   }
		}
	};

	// Evoker.EvokerWololoSpellGoal: a blue sheep within 16 blocks turns red
	class EvokerWololoSpellGoal : public SpellcasterUseSpellGoal {
	  public:
		explicit EvokerWololoSpellGoal(Evoker& evoker) : SpellcasterUseSpellGoal(evoker), _evoker(evoker) {}
		bool canUse() override {
			if (_caster.getTarget() || _caster.isCastingSpell() || _caster.tickCount() < _nextAttackTickCount) return false;
			std::vector<Actor*> sheep = actorsOfClass(_caster.level(), "Sheep", _caster.boundingBox().inflate(16.0, 4.0, 16.0), [](Actor& actor) {
				auto* s = dynamic_cast<Sheep*>(&actor);
				return s && s->isAlive() && s->color() == DYE_BLUE;
			});
			if (sheep.empty()) return false;
			_evoker.wololoTarget = EntityRef(*sheep[_caster.random().nextInt(static_cast<int>(sheep.size()))]);
			return true;
		}
		bool canContinueToUse() override { return wololoSheep() && _attackWarmupDelay > 0; }
		void stop() override { _evoker.wololoTarget.clear(); }

	  protected:
		int							castWarmupTime() const override { return 40; }
		int							castingTime() const override { return 60; }
		int							castingInterval() const override { return 140; }
		const char*					spellPrepareSound() const override { return "minecraft:entity.evoker.prepare_wololo"; }
		SpellcasterIllager::Spell	spell() const override { return SpellcasterIllager::Spell::Wololo; }
		void						performSpellCasting() override {
			   if (Sheep* sheep = wololoSheep()) {
				   if (sheep->color() == DYE_BLUE) sheep->setColor(DYE_RED);
			   }
		}

	  private:
		Evoker& _evoker;
		Sheep*	wololoSheep() {
			 Actor* actor = _evoker.wololoTarget.isSet() ? _caster.level().actorByRef(_evoker.wololoTarget) : nullptr;
			 auto*	sheep = dynamic_cast<Sheep*>(actor);
			 return sheep && sheep->isAlive() ? sheep : nullptr;
		}
	};

	// ----- Illusioner's spells -----

	// Illusioner.IllusionerMirrorSpellGoal: invisible for a minute
	class IllusionerMirrorSpellGoal : public SpellcasterUseSpellGoal {
	  public:
		using SpellcasterUseSpellGoal::SpellcasterUseSpellGoal;
		bool canUse() override { return SpellcasterUseSpellGoal::canUse() && !_caster.hasEffect(MobEffects::Invisibility); }

	  protected:
		int							castingTime() const override { return 20; }
		int							castingInterval() const override { return 340; }
		const char*					spellPrepareSound() const override { return "minecraft:entity.illusioner.prepare_mirror"; }
		SpellcasterIllager::Spell	spell() const override { return SpellcasterIllager::Spell::Disappear; }
		void						performSpellCasting() override { _caster.addEffect({MobEffects::Invisibility, 1200, 0}); }
	};

	// Illusioner.IllusionerBlindnessSpellGoal: each new target is blinded for 20 seconds (not in peaceful/easy... any)
	class IllusionerBlindnessSpellGoal : public SpellcasterUseSpellGoal {
	  public:
		using SpellcasterUseSpellGoal::SpellcasterUseSpellGoal;
		bool canUse() override {
			if (!SpellcasterUseSpellGoal::canUse()) return false;
			Actor* target = _caster.getTarget();
			if (!target || target->id() == _lastTargetId) return false;
			return _caster.level().getCurrentDifficultyAt(_caster.blockPosition()).isHarderThan(2.0F); // Difficulty.NORMAL
		}
		void start() override {
			SpellcasterUseSpellGoal::start();
			if (Actor* target = _caster.getTarget()) _lastTargetId = target->id();
		}

	  protected:
		int							castingTime() const override { return 20; }
		int							castingInterval() const override { return 180; }
		const char*					spellPrepareSound() const override { return "minecraft:entity.illusioner.prepare_blindness"; }
		SpellcasterIllager::Spell	spell() const override { return SpellcasterIllager::Spell::Blindness; }
		void						performSpellCasting() override {
			   Actor* target = _caster.getTarget();
			   if (!target) return;
			   MobEffects::Instance blindness{MobEffects::Blindness, 400, 0};
			   if (Player* player = target->asPlayer()) {
				   player->addEffect(blindness);
			   } else if (LivingEntity* living = target->asLiving()) {
				   living->addEffect(blindness);
			   }
		}

	  private:
		int _lastTargetId = -1;
	};

	// ----- Vex -----

	// Vex.VexMoveControl: flies straight to the wanted position, facing its target (or its movement)
	class VexMoveControl : public MoveControl {
	  public:
		explicit VexMoveControl(Vex& vex) : MoveControl(vex), _vex(vex) {}
		void tick() override {
			if (_operation != Operation::MoveTo) return;
			Vec3   to		= Vec3{_wantedX, _wantedY, _wantedZ} - _vex.position();
			double distance = to.length();
			AABB   box		= _vex.boundingBox();
			double size		= ((box.maxX - box.minX) + (box.maxY - box.minY) + (box.maxZ - box.minZ)) / 3.0;
			if (distance < size) {
				_operation = Operation::Wait;
				_vex.setDeltaMovement(_vex.deltaMovement().scale(0.5));
			} else {
				_vex.setDeltaMovement(_vex.deltaMovement() + to.scale(_speedModifier * 0.05 / distance));
				Actor* target = _vex.getTarget();
				Vec3   facing = target ? target->position() - _vex.position() : _vex.deltaMovement();
				float  yRot	  = -static_cast<float>(Mth::atan2(facing.x, facing.z)) * (180.0F / (float)M_PI);
				_vex.setYRot(yRot);
				_vex.setYBodyRot(yRot);
			}
		}

	  private:
		Vex& _vex;
	};

	// Vex.VexChargeAttackGoal
	class VexChargeAttackGoal : public Goal {
	  public:
		explicit VexChargeAttackGoal(Vex& vex) : _vex(vex) { setFlags({Flag::Move}); }
		bool canUse() override {
			Actor* target = _vex.getTarget();
			return target && target->isAlive() && !_vex.moveControl().hasWanted() && _vex.random().nextInt(reducedTickDelay(7)) == 0 &&
				   _vex.distanceToSqr(*target) > 4.0;
		}
		bool canContinueToUse() override {
			Actor* target = _vex.getTarget();
			return _vex.moveControl().hasWanted() && _vex.isCharging() && target && target->isAlive();
		}
		void start() override {
			Actor* target = _vex.getTarget();
			if (!target) return;
			Vec3 eye = target->eyePosition();
			_vex.moveControl().setWantedPosition(eye.x, eye.y, eye.z, 1.0);
			_vex.setIsCharging(true);
			_vex.playSound("minecraft:entity.vex.charge", 1.0F, 1.0F);
		}
		void stop() override { _vex.setIsCharging(false); }
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _vex.getTarget();
			if (!target) return;
			if (_vex.boundingBox().intersects(target->boundingBox())) {
				_vex.doHurtTarget(*target);
				_vex.setIsCharging(false);
			} else if (_vex.distanceToSqr(*target) < 9.0) {
				Vec3 eye = target->eyePosition();
				_vex.moveControl().setWantedPosition(eye.x, eye.y, eye.z, 1.0);
			}
		}

	  private:
		Vex& _vex;
	};

	// Vex.VexRandomMoveGoal: drifts around its origin, through anything but solid blocks
	class VexRandomMoveGoal : public Goal {
	  public:
		explicit VexRandomMoveGoal(Vex& vex) : _vex(vex) { setFlags({Flag::Move}); }
		bool canUse() override { return !_vex.moveControl().hasWanted() && _vex.random().nextInt(reducedTickDelay(7)) == 0; }
		bool canContinueToUse() override { return false; }
		void tick() override {
			BlockPos	origin = _vex.hasBoundOrigin() ? _vex.boundOrigin() : _vex.blockPosition();
			JavaRandom& random = _vex.random();
			for (int i = 0; i < 3; i++) {
				BlockPos pos = origin.offset(random.nextInt(15) - 7, random.nextInt(11) - 5, random.nextInt(15) - 7);
				if (isEmptyBlock(_vex.level(), pos)) {
					_vex.moveControl().setWantedPosition(pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, 0.25);
					if (!_vex.getTarget()) _vex.lookControl().setLookAt(pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, 180.0F, 20.0F);
					break;
				}
			}
		}

	  private:
		Vex& _vex;
	};

	// Vex.VexCopyOwnerTargetGoal: its evoker's target
	class VexCopyOwnerTargetGoal : public TargetGoal {
	  public:
		explicit VexCopyOwnerTargetGoal(Vex& vex) : TargetGoal(vex, false), _vex(vex) {}
		bool canUse() override {
			Mob*   owner  = _vex.ownerMob();
			Actor* target = owner ? owner->getTarget() : nullptr;
			return target && canAttack(target, TargetingConditions::forNonCombat().ignoreLineOfSight().ignoreInvisibilityTesting());
		}
		void start() override {
			if (Mob* owner = _vex.ownerMob()) _vex.setTarget(owner->getTarget());
			TargetGoal::start();
		}

	  private:
		Vex& _vex;
	};

	// ----- Ravager -----

	// Ravager.RavagerMeleeAttackGoal: bites what its big head reaches
	class RavagerMeleeAttackGoal : public MeleeAttackGoal {
	  public:
		explicit RavagerMeleeAttackGoal(Ravager& ravager) : MeleeAttackGoal(ravager, 1.0, true) {}
	};
} // namespace

// ===================== Raider =====================

bool Raider::isAlliedTo(Actor& other) const {
	if (&other == static_cast<const Actor*>(this)) return true;
	return _level.gameData().isInTag("minecraft:entity_type", "minecraft:raiders", other.typeId());
}

void Raider::addRaiderTargets(bool villagersMustSee) {
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Raider"});
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	auto players = std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true);
	players->setUnseenMemoryTicks(300);
	_targetSelector.addGoal(2, std::move(players));
	auto villagers = std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractVillager", villagersMustSee);
	villagers->setUnseenMemoryTicks(300);
	_targetSelector.addGoal(3, std::move(villagers));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", villagersMustSee));
}

// ===================== Vindicator =====================

Vindicator::Vindicator(Level& level, int typeId) : Raider(level, typeId) {}

void Vindicator::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Creaking", 8.0F, 1.0, 1.2));
	_goalSelector.addGoal(4, std::make_unique<MeleeAttackGoal>(*this, 1.0, false)); // VindicatorMeleeAttackGoal
	_goalSelector.addGoal(8, std::make_unique<RandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(9, std::make_unique<LookAtPlayerGoal>(*this, "Player", 3.0F, 1.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Mob", 8.0F));
	addRaiderTargets(true);
}

void Vindicator::customServerAiStep() {
	// Vindicator.customServerAiStep: it opens doors (breaking them is for raids), aggressive while it has a target
	navigation().setCanOpenDoors(true);
	setAggressive(getTarget() != nullptr);
	Raider::customServerAiStep();
}

void Vindicator::populateDefaultEquipmentSlots(DifficultyInstance&) { setItemSlot(EquipmentSlot::MainHand, itemOf(_level, "minecraft:iron_axe")); }

std::shared_ptr<SpawnGroupData> Vindicator::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group = Raider::finalizeSpawn(difficulty, reason, group);
	populateDefaultEquipmentSlots(difficulty);
	populateDefaultEquipmentEnchantments(difficulty);
	return group;
}

// ===================== Pillager =====================

Pillager::Pillager(Level& level, int typeId) : Raider(level, typeId) {}

void Pillager::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Creaking", 8.0F, 1.0, 1.2));
	_goalSelector.addGoal(3, std::make_unique<RangedCrossbowAttackGoal>(*this, 1.0, 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(9, std::make_unique<LookAtPlayerGoal>(*this, "Player", 15.0F, 1.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Mob", 15.0F));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Raider"});
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractVillager", false));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", true));
}

void Pillager::setChargingCrossbow(bool charging) {
	if (charging == _chargingCrossbow) return;
	_chargingCrossbow = charging;
	markData(DATA_IS_CHARGING_CROSSBOW);
}

bool Pillager::isHoldingCrossbow() { return isItem(_level, getMainHandItem(), "minecraft:crossbow") || isItem(_level, getItemInHand(1), "minecraft:crossbow"); }

void Pillager::performRangedAttack(Actor& target, float) {
	// CrossbowAttackMob.performCrossbowAttack(this, 1.6): a critical arrow, the crossbow's spread by difficulty
	const ItemStack& weapon = isItem(_level, getMainHandItem(), "minecraft:crossbow") ? getMainHandItem() : getItemInHand(1);
	shootArrowAt(*this, target, 1.6F, 0.0F, weapon, true);
	playSound("minecraft:item.crossbow.shoot", 1.0F, 1.0F / (_random.nextFloat() * 0.4F + 0.8F));
}

void Pillager::populateDefaultEquipmentSlots(DifficultyInstance&) { setItemSlot(EquipmentSlot::MainHand, itemOf(_level, "minecraft:crossbow")); }

std::shared_ptr<SpawnGroupData> Pillager::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group = Raider::finalizeSpawn(difficulty, reason, group);
	populateDefaultEquipmentSlots(difficulty);
	populateDefaultEquipmentEnchantments(difficulty);
	return group;
}

void Pillager::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Raider::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_IS_CHARGING_CROSSBOW, onlyNonDefault, !_chargingCrossbow)) writeBoolData(buf, DATA_IS_CHARGING_CROSSBOW, _chargingCrossbow);
}

// ===================== SpellcasterIllager =====================

void SpellcasterIllager::setIsCastingSpell(Spell spell) {
	if (spell == _spell) return;
	_spell = spell;
	markData(DATA_SPELL_CASTING);
}

void SpellcasterIllager::customServerAiStep() {
	Raider::customServerAiStep();
	if (spellCastingTickCount > 0) spellCastingTickCount--;
}

void SpellcasterIllager::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Raider::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SPELL_CASTING, onlyNonDefault, _spell == Spell::None)) writeByteData(buf, DATA_SPELL_CASTING, static_cast<uint8_t>(_spell));
}

void SpellcasterIllager::save(Buffer& buf) const {
	Raider::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(spellCastingTickCount);
}

void SpellcasterIllager::load(Buffer& buf) {
	Raider::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown spellcaster data version");
	spellCastingTickCount = buf.readInt();
}

void SpellcasterCastingSpellGoal::start() { _caster.navigation().stop(); }

void SpellcasterCastingSpellGoal::tick() {
	if (Actor* target = _caster.getTarget()) _caster.lookControl().setLookAt(*target, static_cast<float>(_caster.maxHeadYRot()), static_cast<float>(_caster.maxHeadXRot()));
}

bool SpellcasterUseSpellGoal::canUse() {
	Actor* target = _caster.getTarget();
	return target && target->isAlive() && !_caster.isCastingSpell() && _caster.tickCount() >= _nextAttackTickCount;
}

bool SpellcasterUseSpellGoal::canContinueToUse() {
	Actor* target = _caster.getTarget();
	return target && target->isAlive() && _attackWarmupDelay > 0;
}

void SpellcasterUseSpellGoal::start() {
	_attackWarmupDelay			  = adjustedTickDelay(castWarmupTime());
	_caster.spellCastingTickCount = castingTime();
	_nextAttackTickCount		  = _caster.tickCount() + castingInterval();
	_caster.playSound(spellPrepareSound(), 1.0F, 1.0F);
	_caster.setIsCastingSpell(spell());
}

void SpellcasterUseSpellGoal::tick() {
	if (--_attackWarmupDelay == 0) {
		performSpellCasting();
		const char* sound = dynamic_cast<Evoker*>(&_caster) ? "minecraft:entity.evoker.cast_spell" : "minecraft:entity.illusioner.cast_spell";
		_caster.playSound(sound, 1.0F, 1.0F);
	}
}

// ===================== Evoker =====================

Evoker::Evoker(Level& level, int typeId) : SpellcasterIllager(level, typeId) { _xpReward = 10; }

void Evoker::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<EvokerCastingSpellGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Player", 8.0F, 0.6, 1.0));
	_goalSelector.addGoal(3, std::make_unique<AvoidEntityGoal>(*this, "Creaking", 8.0F, 1.0, 1.2));
	_goalSelector.addGoal(4, std::make_unique<EvokerSummonSpellGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<EvokerAttackSpellGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<EvokerWololoSpellGoal>(*this));
	_goalSelector.addGoal(8, std::make_unique<RandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(9, std::make_unique<LookAtPlayerGoal>(*this, "Player", 3.0F, 1.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Mob", 8.0F));
	addRaiderTargets(false);
}

// ===================== Illusioner =====================

Illusioner::Illusioner(Level& level, int typeId) : SpellcasterIllager(level, typeId) { _xpReward = 5; }

void Illusioner::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<SpellcasterCastingSpellGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<IllusionerMirrorSpellGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<IllusionerBlindnessSpellGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<RangedBowAttackGoal>(*this, 0.5, 20, 15.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(9, std::make_unique<LookAtPlayerGoal>(*this, "Player", 3.0F, 1.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Mob", 8.0F));
	addRaiderTargets(false);
}

void Illusioner::performRangedAttack(Actor& target, float power) {
	shootArrowAt(*this, target, 1.6F, power, getMainHandItem(), false);
	playSound("minecraft:entity.skeleton.shoot", 1.0F, 1.0F / (_random.nextFloat() * 0.4F + 0.8F));
}

std::shared_ptr<SpawnGroupData> Illusioner::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group = SpellcasterIllager::finalizeSpawn(difficulty, reason, group);
	setItemSlot(EquipmentSlot::MainHand, itemOf(_level, "minecraft:bow"));
	return group;
}

// ===================== Vex =====================

Vex::Vex(Level& level, int typeId) : Monster(level, typeId) {
	_moveControl = std::make_unique<VexMoveControl>(*this);
	_xpReward	 = 3;
}

void Vex::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<VexChargeAttackGoal>(*this));
	_goalSelector.addGoal(8, std::make_unique<VexRandomMoveGoal>(*this));
	_goalSelector.addGoal(9, std::make_unique<LookAtPlayerGoal>(*this, "Player", 3.0F, 1.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Mob", 8.0F));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Raider"});
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<VexCopyOwnerTargetGoal>(*this));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

void Vex::setIsCharging(bool charging) {
	uint8_t flags = static_cast<uint8_t>(charging ? _vexFlags | FLAG_IS_CHARGING : _vexFlags & ~FLAG_IS_CHARGING);
	if (flags == _vexFlags) return;
	_vexFlags = flags;
	markData(DATA_FLAGS);
}

void Vex::setOwner(Mob* owner) {
	if (owner) {
		_owner = EntityRef(*owner);
	} else {
		_owner.clear();
	}
}

Mob* Vex::ownerMob() { return _owner.isSet() ? dynamic_cast<Mob*>(_level.actorByRef(_owner)) : nullptr; }

void Vex::tick() {
	// It goes through blocks
	_noPhysics = true;
	Monster::tick();
	_noPhysics = false;
	if (isRemoved()) return;
	if (_hasLimitedLife && --_limitedLifeTicks <= 0) {
		_limitedLifeTicks = 20;
		hurtServer({"minecraft:starve", nullptr, nullptr, std::nullopt}, 1.0F);
	}
}

std::shared_ptr<SpawnGroupData> Vex::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group = Monster::finalizeSpawn(difficulty, reason, group);
	setItemSlot(EquipmentSlot::MainHand, itemOf(_level, "minecraft:iron_sword"));
	setDropChance(EquipmentSlot::MainHand, 0.0F);
	populateDefaultEquipmentEnchantments(difficulty);
	return group;
}

void Vex::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _vexFlags == 0)) writeByteData(buf, DATA_FLAGS, _vexFlags);
}

void Vex::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_hasBoundOrigin);
	buf.writeInt(_boundOrigin.x);
	buf.writeInt(_boundOrigin.y);
	buf.writeInt(_boundOrigin.z);
	buf.writeInt(_hasLimitedLife ? _limitedLifeTicks : -1);
}

void Vex::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown vex data version");
	_hasBoundOrigin = buf.readBool();
	_boundOrigin.x	= buf.readInt();
	_boundOrigin.y	= buf.readInt();
	_boundOrigin.z	= buf.readInt();
	int life		= buf.readInt();
	_hasLimitedLife = life != -1;
	if (_hasLimitedLife) _limitedLifeTicks = life;
}

// ===================== Ravager =====================

Ravager::Ravager(Level& level, int typeId) : Raider(level, typeId) {
	_xpReward = 20;
	setPathfindingMalus(PathType::Leaves, 0.0F);
}

void Ravager::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<RavagerMeleeAttackGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.4));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Mob", 8.0F));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Raider"});
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(2, std::move(hurtBy));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
	_targetSelector.addGoal(4, std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractVillager", 10, true, false,
																			 [](Actor& actor, Level&) { return !actor.asLiving() || !actor.asLiving()->isBaby(); }));
	_targetSelector.addGoal(4, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", true));
}

bool Ravager::isImmobile() const { return Raider::isImmobile() || _attackTick > 0 || _stunnedTick > 0 || _roarTick > 0; }

void Ravager::aiStep() {
	Raider::aiStep();
	if (!isAlive()) return;
	if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) {
		if (isImmobile()) {
			speed->setBaseValue(0.0);
		} else {
			double wanted = getTarget() ? 0.35 : 0.3;
			speed->setBaseValue(Mth::lerp(0.1, speed->baseValue(), wanted));
		}
	}
	if (_horizontalCollision) {
		// The leaves in its way break (mobGriefing: always on here)
		bool destroyed = false;
		AABB box	   = boundingBox().inflate(0.2);
		for (int x = Mth::floor(box.minX); x <= Mth::floor(box.maxX); x++) {
			for (int y = Mth::floor(box.minY); y <= Mth::floor(box.maxY); y++) {
				for (int z = Mth::floor(box.minZ); z <= Mth::floor(box.maxZ); z++) {
					BlockPos pos{x, y, z};
					int		 block = _level.blocks().blockOf(_level.getBlockState(pos));
					if (_level.gameData().isInTag("minecraft:block", "minecraft:leaves", block)) destroyed = _level.destroyBlock(pos, true) || destroyed;
				}
			}
		}
		if (!destroyed && _onGround) jumpFromGround();
	}
	if (_roarTick > 0) {
		if (--_roarTick == 10) roar();
	}
	if (_attackTick > 0) _attackTick--;
	if (_stunnedTick > 0) {
		if (--_stunnedTick == 0) {
			playSound("minecraft:entity.ravager.roar", 1.0F, 1.0F);
			_roarTick = ROAR_DURATION;
		}
	}
}

void Ravager::blockedByShield() {
	if (_random.nextDouble() < 0.5) {
		_stunnedTick = STUN_DURATION;
		playSound("minecraft:entity.ravager.stunned", 1.0F, 1.0F);
		broadcastEntityEvent(EVENT_STUNNED);
		navigation().stop();
	}
}

void Ravager::roar() {
	if (!isAlive()) return;
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(4.0), this)) {
		if (!actor->isAlive() || !actor->asLiving() && !actor->asPlayer()) continue;
		if (dynamic_cast<Ravager*>(actor)) continue;
		if (!_level.gameData().isInTag("minecraft:entity_type", "minecraft:illager", actor->typeId())) {
			Combat::DamageSource source{"minecraft:mob_attack", this, nullptr, std::nullopt};
			if (Player* player = actor->asPlayer()) {
				Combat::damage(_level.server(), *player, 6.0F, source);
			} else {
				actor->hurtServer(source, 6.0F);
			}
		}
		// Ravager.strongKnockback
		double dx = actor->position().x - _position.x, dz = actor->position().z - _position.z;
		double d  = std::max(dx * dx + dz * dz, 0.001);
		actor->pushMotion({dx / d * 4.0, 0.2, dz / d * 4.0});
	}
}

bool Ravager::doHurtTarget(Actor& target) {
	_attackTick = 10;
	broadcastEntityEvent(EVENT_ATTACK);
	playSound("minecraft:entity.ravager.attack", 1.0F, 1.0F);
	return Raider::doHurtTarget(target);
}

void Ravager::save(Buffer& buf) const {
	Raider::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_attackTick);
	buf.writeInt(_stunnedTick);
	buf.writeInt(_roarTick);
}

void Ravager::load(Buffer& buf) {
	Raider::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown ravager data version");
	_attackTick	 = buf.readInt();
	_stunnedTick = buf.readInt();
	_roarTick	 = buf.readInt();
}

// ===================== EvokerFangs =====================

EvokerFangs::EvokerFangs(Level& level, double x, double y, double z, float yRot, int warmupDelay, Actor* owner)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:evoker_fangs"), 0.5F, 0.8F), _warmupDelayTicks(warmupDelay) {
	setPos({x, y, z});
	_oldPosition = _position;
	_yRot		 = yRot;
	if (owner) _owner = EntityRef(*owner);
}

Actor* EvokerFangs::owner() const { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

void EvokerFangs::tick() {
	if (--_warmupDelayTicks >= 0) return;
	if (_warmupDelayTicks == -8) {
		for (Actor* actor : _level.actorsIn(boundingBox().inflate(0.2, 0.0, 0.2), this)) dealDamageTo(*actor);
	}
	if (!_sentSpikeEvent) {
		// EntityEvent 4: the jaws rise on the clients (and they play the bite)
		Buffer data;
		data.writeInt(_id);
		data.writeByte(4);
		_level.entities().broadcast(*this, PacketId::Play::Clientbound::ENTITY_EVENT, data);
		_sentSpikeEvent = true;
	}
	if (--_lifeTicks < 0) discard();
}

void EvokerFangs::dealDamageTo(Actor& target) {
	if (!target.isAlive() || (!target.asLiving() && !target.asPlayer())) return;
	Actor* by = owner();
	if (by == &target) return;
	if (auto* raider = dynamic_cast<Raider*>(by); raider && raider->isAlliedTo(target)) return;
	Combat::DamageSource source = by ? Combat::DamageSource{"minecraft:indirect_magic", by, this, std::nullopt}
									 : Combat::DamageSource{"minecraft:magic", nullptr, nullptr, std::nullopt};
	if (Player* player = target.asPlayer()) {
		Combat::damage(_level.server(), *player, 6.0F, source);
	} else {
		target.hurtServer(source, 6.0F);
	}
}
