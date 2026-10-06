#include "world/entity/mobs/Panda.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

// Panda.PandaLookAtPlayerGoal: keeps looking at who it was told to (the player that can't make it breed)
class PandaLookAtPlayerGoal : public LookAtPlayerGoal {
  public:
	PandaLookAtPlayerGoal(Panda& panda, float distance) : LookAtPlayerGoal(panda, "Player", distance), _panda(panda) {}
	void setTarget(Actor* target) { _lookAt = target ? EntityRef(*target) : EntityRef(); }
	bool canContinueToUse() override { return _lookAt.isSet() && LookAtPlayerGoal::canContinueToUse(); }
	bool canUse() override {
		if (_mob.random().nextFloat() >= _probability) return false;
		if (!_lookAt.isSet() || !_mob.level().actorByRef(_lookAt)) {
			Actor* found = nearestPlayer(_mob.level(), _lookAtContext, &_mob, _mob.position().x, _mob.eyeY(), _mob.position().z);
			_lookAt		 = found ? EntityRef(*found) : EntityRef();
		}
		return _panda.canPerformAction() && _lookAt.isSet();
	}
	void stop() override { _lookAt.clear(); }
	void tick() override {
		if (_lookAt.isSet()) LookAtPlayerGoal::tick();
	}

  private:
	Panda& _panda;
};

namespace {
	constexpr int SAVE_VERSION = 1;

	class PandaMoveControl : public MoveControl {
	  public:
		explicit PandaMoveControl(Panda& panda) : MoveControl(panda), _panda(panda) {}
		void tick() override {
			if (_panda.canPerformAction()) MoveControl::tick();
		}

	  private:
		Panda& _panda;
	};

	class PandaPanicGoal : public PanicGoal {
	  public:
		PandaPanicGoal(Panda& panda, double speed) : PanicGoal(panda, speed, "minecraft:panic_environmental_causes"), _panda(panda) {}
		bool canContinueToUse() override {
			if (_panda.isSitting()) {
				_panda.navigation().stop();
				return false;
			}
			return PanicGoal::canContinueToUse();
		}

	  private:
		Panda& _panda;
	};

	// Breeds only with bamboo within 8 blocks; without, it shakes its head at the nearest player
	class PandaBreedGoal : public BreedGoal {
	  public:
		PandaBreedGoal(Panda& panda, double speed) : BreedGoal(panda, speed), _panda(panda) {}
		bool canUse() override {
			if (!BreedGoal::canUse() || _panda.unhappyCounter() != 0) return false;
			if (canFindBamboo()) return true;
			if (_unhappyCooldown <= _panda.tickCount()) {
				_panda.setUnhappyCounter(32);
				_unhappyCooldown = _panda.tickCount() + 600;
				if (_panda.isEffectiveAiPublic() && _panda.lookAtPlayerGoal) {
					TargetingConditions conditions = TargetingConditions::forNonCombat();
					conditions.withRange(8.0);
					const Vec3& p = _panda.position();
					_panda.lookAtPlayerGoal->setTarget(nearestPlayer(_panda.level(), conditions, &_panda, p.x, p.y, p.z));
				}
			}
			return false;
		}

	  private:
		Panda& _panda;
		int	   _unhappyCooldown = 0;

		bool canFindBamboo() {
			Level&	 level	= _panda.level();
			int		 bamboo = level.gameData().getStaticId("minecraft:block", "minecraft:bamboo");
			BlockPos pos	= _panda.blockPosition();
			for (int y = 0; y < 3; y++) {
				for (int range = 0; range < 8; range++) {
					for (int x = 0; x <= range; x = x > 0 ? -x : 1 - x) {
						for (int z = x < range && x > -range ? range : 0; z <= range; z = z > 0 ? -z : 1 - z) {
							if (level.blocks().blockOf(level.getBlockState(pos.offset(x, y, z))) == bamboo) return true;
						}
					}
				}
			}
			return false;
		}
	};

	class PandaAttackGoal : public MeleeAttackGoal {
	  public:
		PandaAttackGoal(Panda& panda, double speed, bool follow) : MeleeAttackGoal(panda, speed, follow), _panda(panda) {}
		bool canUse() override { return _panda.canPerformAction() && MeleeAttackGoal::canUse(); }

	  private:
		Panda& _panda;
	};

	// Worried pandas keep away from players and monsters
	class PandaAvoidGoal : public AvoidEntityGoal {
	  public:
		PandaAvoidGoal(Panda& panda, std::string avoidClass, float distance, double walk, double sprint)
			: AvoidEntityGoal(panda, std::move(avoidClass), distance, walk, sprint, [](Actor& actor) { return !actor.isSpectator(); }), _panda(panda) {}
		bool canUse() override { return _panda.isWorried() && _panda.canPerformAction() && AvoidEntityGoal::canUse(); }

	  private:
		Panda& _panda;
	};

	// Holding something to eat (or with some lying within 6 blocks): goes to it, sits, eats
	class PandaSitGoal : public Goal {
	  public:
		explicit PandaSitGoal(Panda& panda) : _panda(panda) { setFlags({Flag::Move}); }
		bool canUse() override {
			if (_cooldown > _panda.tickCount() || _panda.isBaby() || _panda.isInWaterNow() || !_panda.canPerformAction() || _panda.unhappyCounter() > 0) return false;
			if (!_panda.getItemBySlot(EquipmentSlot::MainHand).isEmpty()) return true;
			return firstItem(6.0) != nullptr;
		}
		bool canContinueToUse() override {
			JavaRandom& random = _panda.random();
			if (!_panda.isInWaterNow() && (_panda.isLazy() || random.nextInt(reducedTickDelay(600)) != 1)) return random.nextInt(reducedTickDelay(2000)) != 1;
			return false;
		}
		void tick() override {
			if (!_panda.isSitting() && !_panda.getItemBySlot(EquipmentSlot::MainHand).isEmpty()) _panda.tryToSit();
		}
		void start() override {
			if (_panda.getItemBySlot(EquipmentSlot::MainHand).isEmpty()) {
				if (ItemEntity* item = firstItem(8.0)) _panda.navigation().moveTo(*item, static_cast<double>(1.2F));
			} else {
				_panda.tryToSit();
			}
			_cooldown = 0;
		}
		void stop() override {
			ItemStack held = _panda.getItemBySlot(EquipmentSlot::MainHand);
			if (!held.isEmpty()) {
				_panda.dropHeld(held);
				_panda.setItemSlot(EquipmentSlot::MainHand, ItemStack());
				int seconds = _panda.isLazy() ? _panda.random().nextInt(50) + 10 : _panda.random().nextInt(150) + 10;
				_cooldown	= _panda.tickCount() + seconds * 20;
			}
			_panda.sit(false);
		}

	  private:
		Panda& _panda;
		int	   _cooldown = 0;

		ItemEntity* firstItem(double range) {
			ItemEntity* found = nullptr;
			_panda.level().entities().forEachIn(_panda.boundingBox().inflate(range), [&](Entity& entity) {
				if (found || entity.isRemoved()) return;
				auto* item = dynamic_cast<ItemEntity*>(&entity);
				if (item && Panda::canPickUpAndEat(_panda.level(), *item)) found = item;
			});
			return found;
		}
	};

	class PandaLieOnBackGoal : public Goal {
	  public:
		explicit PandaLieOnBackGoal(Panda& panda) : _panda(panda) {}
		bool canUse() override {
			return _cooldown < _panda.tickCount() && _panda.isLazy() && _panda.canPerformAction() && _panda.random().nextInt(reducedTickDelay(400)) == 1;
		}
		bool canContinueToUse() override {
			JavaRandom& random = _panda.random();
			if (!_panda.isInWaterNow() && (_panda.isLazy() || random.nextInt(reducedTickDelay(600)) != 1)) return random.nextInt(reducedTickDelay(2000)) != 1;
			return false;
		}
		void start() override {
			_panda.setOnBack(true);
			_cooldown = 0;
		}
		void stop() override {
			_panda.setOnBack(false);
			_cooldown = _panda.tickCount() + 200;
		}

	  private:
		Panda& _panda;
		int	   _cooldown = 0;
	};

	class PandaSneezeGoal : public Goal {
	  public:
		explicit PandaSneezeGoal(Panda& panda) : _panda(panda) {}
		bool canUse() override {
			if (!_panda.isBaby() || !_panda.canPerformAction()) return false;
			if (_panda.isWeak() && _panda.random().nextInt(reducedTickDelay(500)) == 1) return true;
			return _panda.random().nextInt(reducedTickDelay(6000)) == 1;
		}
		bool canContinueToUse() override { return false; }
		void start() override { _panda.sneeze(true); }

	  private:
		Panda& _panda;
	};

	// Babies and playful ones roll now and then, and off the edges
	class PandaRollGoal : public Goal {
	  public:
		explicit PandaRollGoal(Panda& panda) : _panda(panda) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override {
			if (!(_panda.isBaby() || _panda.isPlayful()) || !_panda.onGround()) return false;
			if (!_panda.canPerformAction()) return false;
			float  yaw = _panda.yRot() * static_cast<float>(M_PI / 180.0);
			float  dx = -Mth::sin(yaw), dz = Mth::cos(yaw);
			int	   stepX = std::abs(dx) > 0.5 ? (dx > 0 ? 1 : -1) : 0, stepZ = std::abs(dz) > 0.5 ? (dz > 0 ? 1 : -1) : 0;
			Level& level = _panda.level();
			if (level.blocks().isAir(level.getBlockState(_panda.blockPosition().offset(stepX, -1, stepZ)))) return true;
			if (_panda.isPlayful() && _panda.random().nextInt(reducedTickDelay(60)) == 1) return true;
			return _panda.random().nextInt(reducedTickDelay(500)) == 1;
		}
		bool canContinueToUse() override { return false; }
		void start() override { _panda.roll(true); }
		bool isInterruptable() override { return false; }

	  private:
		Panda& _panda;
	};

	// Fights back until it bit once or got bamboo; only the aggressive ones around join in
	class PandaHurtByTargetGoal : public HurtByTargetGoal {
	  public:
		explicit PandaHurtByTargetGoal(Panda& panda) : HurtByTargetGoal(panda), _panda(panda) {}
		bool canContinueToUse() override {
			if (!_panda.gotBamboo && !_panda.didBite) return HurtByTargetGoal::canContinueToUse();
			_panda.setTarget(nullptr);
			return false;
		}

	  protected:
		void alertOther(Mob& other, Actor& target) override {
			auto* panda = dynamic_cast<Panda*>(&other);
			if (panda && panda->isAggressiveGene()) other.setTarget(&target);
		}

	  private:
		Panda& _panda;
	};
} // namespace

Panda::Panda(Level& level, int typeId) : Animal(level, typeId) {
	setMoveControl(std::make_unique<PandaMoveControl>(*this));
	setCanPickUpLoot(true);
}

void Panda::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<PandaPanicGoal>(*this, 2.0));
	_goalSelector.addGoal(2, std::make_unique<PandaBreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<PandaAttackGoal>(*this, static_cast<double>(1.2F), true));
	_goalSelector.addGoal(4, std::make_unique<TemptGoal>(*this, 1.0, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(6, std::make_unique<PandaAvoidGoal>(*this, "Player", 8.0F, 2.0, 2.0));
	_goalSelector.addGoal(6, std::make_unique<PandaAvoidGoal>(*this, "Monster", 4.0F, 2.0, 2.0));
	_goalSelector.addGoal(7, std::make_unique<PandaSitGoal>(*this));
	_goalSelector.addGoal(8, std::make_unique<PandaLieOnBackGoal>(*this));
	_goalSelector.addGoal(8, std::make_unique<PandaSneezeGoal>(*this));
	auto look		 = std::make_unique<PandaLookAtPlayerGoal>(*this, 6.0F);
	lookAtPlayerGoal = look.get();
	_goalSelector.addGoal(9, std::move(look));
	_goalSelector.addGoal(10, std::make_unique<RandomLookAroundGoal>(*this));
	_goalSelector.addGoal(12, std::make_unique<PandaRollGoal>(*this));
	_goalSelector.addGoal(13, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(14, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	auto hurtBy = std::make_unique<PandaHurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
}

void Panda::setFlag(int flag, bool value) {
	uint8_t flags = static_cast<uint8_t>(value ? _pandaFlags | flag : _pandaFlags & ~flag);
	if (flags == _pandaFlags) return;
	_pandaFlags = flags;
	markData(DATA_FLAGS);
}

void Panda::setUnhappyCounter(int value) {
	if (value == _unhappyCounter) return;
	_unhappyCounter = value;
	markData(DATA_UNHAPPY_COUNTER);
}

void Panda::setEatCounter(int value) {
	if (value == _eatCounter) return;
	_eatCounter = value;
	markData(DATA_EAT_COUNTER);
}

void Panda::setSneezeCounter(int value) {
	if (value == _sneezeCounter) return;
	_sneezeCounter = value;
	markData(DATA_SNEEZE_COUNTER);
}

void Panda::sneeze(bool value) {
	setFlag(FLAG_SNEEZE, value);
	if (!value) setSneezeCounter(0);
}

int Panda::randomGene(JavaRandom& random) {
	int roll = random.nextInt(16);
	if (roll == 0) return Lazy;
	if (roll == 1) return Worried;
	if (roll == 2) return Playful;
	if (roll == 4) return Aggressive;
	if (roll < 9) return Weak;
	return roll < 11 ? Brown : Normal;
}

void Panda::setMainGene(int gene) {
	if (gene > 6) gene = randomGene(_random);
	if (gene == _mainGene) return;
	_mainGene = static_cast<uint8_t>(gene);
	markData(DATA_MAIN_GENE);
}

void Panda::setHiddenGene(int gene) {
	if (gene > 6) gene = randomGene(_random);
	if (gene == _hiddenGene) return;
	_hiddenGene = static_cast<uint8_t>(gene);
	markData(DATA_HIDDEN_GENE);
}

int Panda::variant() const {
	bool recessive = _mainGene == Brown || _mainGene == Weak;
	if (recessive) return _mainGene == _hiddenGene ? _mainGene : Normal;
	return _mainGene;
}

bool Panda::isScared() const { return isWorried() && _level.isThundering(); }

void Panda::setAttributes() {
	if (isWeak()) {
		if (AttributeInstance* health = _attributes.getInstance(_ids.maxHealth)) health->setBaseValue(10.0);
	}
	if (isLazy()) {
		if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) speed->setBaseValue(0.07F);
	}
}

void Panda::tryToSit() {
	if (isInWater()) return;
	_zza = 0.0F;
	navigation().stop();
	sit(true);
}

bool Panda::canPickUpAndEat(Level& level, ItemEntity& item) {
	return !item.item().isEmpty() && level.gameData().isInTag("minecraft:item", "minecraft:panda_eats_from_ground", item.item().item) && !item.isRemoved() &&
		   item.pickupDelay() <= 0;
}

bool Panda::doHurtTarget(Actor& target) {
	if (!isAggressiveGene()) didBite = true;
	return Animal::doHurtTarget(target);
}

bool Panda::hurtServer(const Combat::DamageSource& source, float amount) {
	sit(false);
	return Animal::hurtServer(source, amount);
}

void Panda::tick() {
	Animal::tick();
	if (isRemoved()) return;
	if (isWorried()) {
		if (_level.isThundering() && !isInWater()) {
			sit(true);
			eat(false);
		} else if (!isEating()) {
			sit(false);
		}
	}
	Actor* target = getTarget();
	if (!target) {
		gotBamboo = false;
		didBite	  = false;
	}
	if (_unhappyCounter > 0) {
		if (target) lookAt(*target, 90.0F, 90.0F);
		if (_unhappyCounter == 29 || _unhappyCounter == 14) playSound("minecraft:entity.panda.cant_breed", 1.0F, 1.0F);
		setUnhappyCounter(_unhappyCounter - 1);
	}
	if (isSneezing()) {
		setSneezeCounter(_sneezeCounter + 1);
		if (_sneezeCounter > 20) {
			sneeze(false);
			afterSneeze();
		} else if (_sneezeCounter == 1) {
			playSound("minecraft:entity.panda.pre_sneeze", 1.0F, 1.0F);
		}
	}
	if (isRolling()) {
		handleRoll();
	} else {
		_rollCounter = 0;
	}
	if (isSitting()) _xRot = 0.0F;
	handleEating();
}

void Panda::handleEating() {
	bool holding = !getItemBySlot(EquipmentSlot::MainHand).isEmpty();
	if (!isEating() && isSitting() && !isScared() && holding && _random.nextInt(80) == 1) {
		eat(true);
	} else if (!holding || !isSitting()) {
		eat(false);
	}
	if (!isEating()) return;
	// addEatingParticles: the sound, and the particles' random draws
	if (_eatCounter % 5 == 0) {
		playSound("minecraft:entity.panda.eat", 0.5F + 0.5F * _random.nextInt(2), (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
		for (int i = 0; i < 6; i++) {
			for (int draws = 0; draws < 5; draws++) _random.nextFloat();
		}
	}
	if (_eatCounter > 80 && _random.nextInt(20) == 1) {
		const ItemStack& held = getItemBySlot(EquipmentSlot::MainHand);
		if (_eatCounter > 100 && !held.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:panda_eats_from_ground", held.item)) {
			setItemSlot(EquipmentSlot::MainHand, ItemStack());
			sit(false);
		}
		eat(false);
		return;
	}
	setEatCounter(_eatCounter + 1);
}

void Panda::handleRoll() {
	_rollCounter++;
	if (_rollCounter > 32) {
		roll(false);
		return;
	}
	if (_rollCounter == 1) {
		float yaw	= _yRot * static_cast<float>(M_PI / 180.0);
		float power = isBaby() ? 0.1F : 0.2F;
		_rollDelta	= {_delta.x + -Mth::sin(yaw) * power, 0.0, _delta.z + Mth::cos(yaw) * power};
		_delta		= _rollDelta + Vec3{0.0, 0.27, 0.0};
	} else if (_rollCounter != 7 && _rollCounter != 15 && _rollCounter != 23) {
		_delta = {_rollDelta.x, _delta.y, _rollDelta.z};
	} else {
		_delta = {0.0, _onGround ? 0.27 : _delta.y, 0.0};
	}
}

void Panda::afterSneeze() {
	playSound("minecraft:entity.panda.sneeze", 1.0F, 1.0F);
	// The pandas around jump
	std::vector<Panda*> around;
	_level.entities().forEachIn(boundingBox().inflate(10.0), [&](Entity& entity) {
		auto* panda = dynamic_cast<Panda*>(&entity);
		if (panda && !panda->isRemoved() && !panda->isBaby() && panda->_onGround && !panda->isInWater() && panda->canPerformAction()) around.push_back(panda);
	});
	for (Panda* panda : around) panda->jumpFromGround();
	// BuiltInLootTables.PANDA_SNEEZE: a slime ball, once in 700
	if (_random.nextInt(700) == 0) spawnAtLocation(ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:slime_ball"), 1));
}

void Panda::pickUpItem(ItemEntity& item) {
	if (!getItemBySlot(EquipmentSlot::MainHand).isEmpty() || !canPickUpAndEat(_level, item)) return;
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

bool Panda::isFood(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:panda_food", stack.item);
}

bool Panda::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isScared()) return false;
	if (isOnBack()) {
		setOnBack(false);
		return true;
	}
	if (!isFood(stack)) return false;
	if (getTarget()) gotBamboo = true;
	if (isBaby()) {
		usePlayerItem(player, hand);
		ageUp(static_cast<int>(-_age / 20 * 0.1F), true);
	} else if (_age == 0 && canFallInLove()) {
		usePlayerItem(player, hand);
		setInLove(&player);
	} else {
		if (isSitting() || isInWater()) return false;
		tryToSit();
		eat(true);
		ItemStack held = getItemBySlot(EquipmentSlot::MainHand);
		if (!held.isEmpty() && !player.isCreative()) spawnAtLocation(held);
		setItemSlot(EquipmentSlot::MainHand, ItemStack(stack.item, 1));
		usePlayerItem(player, hand);
	}
	return true;
}

std::string Panda::ambientSoundName() {
	if (isAggressiveGene()) return "minecraft:entity.panda.aggressive_ambient";
	return isWorried() ? "minecraft:entity.panda.worried_ambient" : "minecraft:entity.panda.ambient";
}

std::unique_ptr<Mob> Panda::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child = Animal::getBreedOffspring(partner);
	auto*				 cub   = dynamic_cast<Panda*>(child.get());
	if (!cub) return child;
	if (auto* other = dynamic_cast<Panda*>(&partner)) {
		// setGeneFromParents
		JavaRandom& random = cub->_random;
		if (random.nextBoolean()) {
			cub->setMainGene(oneOfGenesRandomly());
			cub->setHiddenGene(other->oneOfGenesRandomly());
		} else {
			cub->setMainGene(other->oneOfGenesRandomly());
			cub->setHiddenGene(oneOfGenesRandomly());
		}
		if (random.nextInt(32) == 0) cub->setMainGene(randomGene(random));
		if (random.nextInt(32) == 0) cub->setHiddenGene(randomGene(random));
	}
	cub->setAttributes();
	return child;
}

std::shared_ptr<SpawnGroupData> Panda::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	JavaRandom& random = _level.random();
	setMainGene(randomGene(random));
	setHiddenGene(randomGene(random));
	setAttributes();
	return Animal::finalizeSpawn(difficulty, reason, group);
}

void Panda::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_UNHAPPY_COUNTER, onlyNonDefault, _unhappyCounter == 0)) writeIntData(buf, DATA_UNHAPPY_COUNTER, _unhappyCounter);
	if (wantsData(mask, DATA_SNEEZE_COUNTER, onlyNonDefault, _sneezeCounter == 0)) writeIntData(buf, DATA_SNEEZE_COUNTER, _sneezeCounter);
	if (wantsData(mask, DATA_EAT_COUNTER, onlyNonDefault, _eatCounter == 0)) writeIntData(buf, DATA_EAT_COUNTER, _eatCounter);
	if (wantsData(mask, DATA_MAIN_GENE, onlyNonDefault, _mainGene == 0)) writeByteData(buf, DATA_MAIN_GENE, _mainGene);
	if (wantsData(mask, DATA_HIDDEN_GENE, onlyNonDefault, _hiddenGene == 0)) writeByteData(buf, DATA_HIDDEN_GENE, _hiddenGene);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _pandaFlags == 0)) writeByteData(buf, DATA_FLAGS, _pandaFlags);
}

void Panda::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(_mainGene);
	buf.writeUByte(_hiddenGene);
}

void Panda::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown panda data version");
	_mainGene	= std::min<uint8_t>(buf.readUByte(), 6);
	_hiddenGene = std::min<uint8_t>(buf.readUByte(), 6);
	setAttributes();
	_dirtyData = 0;
}
