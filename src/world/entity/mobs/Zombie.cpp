#include "world/entity/mobs/Zombie.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/SpawnPlacements.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/ItemDamage.hpp"

#include <ctime>

namespace {
	constexpr int		  SAVE_VERSION = 1;
	constexpr const char* SPEED_MODIFIER_BABY = "minecraft:baby";
	constexpr const char* REINFORCEMENT_CALLER_CHARGE = "minecraft:reinforcement_caller_charge";
	constexpr const char* REINFORCEMENT_CALLEE_CHARGE = "minecraft:reinforcement_callee_charge";
	constexpr const char* LEADER_ZOMBIE_BONUS = "minecraft:leader_zombie_bonus";
	constexpr const char* ZOMBIE_RANDOM_SPAWN_BONUS = "minecraft:zombie_random_spawn_bonus";
	constexpr const char* RANDOM_SPAWN_BONUS = "minecraft:random_spawn_bonus";
	// LivingEntity.entityEventForEquipmentBreak(HEAD)
	constexpr int EVENT_HEAD_BREAK = 49;
	// LevelEvent: ZOMBIE_CONVERTED_TO_DROWNED, HUSK_CONVERTED_TO_ZOMBIE
	constexpr int LEVEL_EVENT_ZOMBIE_TO_DROWNED = 1040, LEVEL_EVENT_HUSK_TO_ZOMBIE = 1041;

	// Zombie.ZombieGroupData
	struct ZombieGroupData : SpawnGroupData {
		bool isBaby, canSpawnJockey;
		ZombieGroupData(bool baby, bool jockey) : isBaby(baby), canSpawnJockey(jockey) {}
	};

	// AttributeInstance.addOrReplacePermanentModifier
	void addOrReplace(AttributeInstance* instance, const AttributeModifier& modifier) {
		if (!instance) return;
		instance->removeModifier(modifier.id);
		instance->addModifier(modifier, true);
	}
} // namespace

void Zombie::setBaby(bool baby) {
	if (_baby != baby) {
		_baby = baby;
		markData(DATA_BABY);
	}
	if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) {
		speed->removeModifier(SPEED_MODIFIER_BABY);
		if (baby) speed->addModifier({SPEED_MODIFIER_BABY, 0.5, AttributeModifier::Operation::AddMultipliedBase}, false);
	}
	// refreshDimensions: BABY_DIMENSIONS, the type's scaled by 0.5 with an eye height of 0.93
	if (baby) {
		setDimensions(_type.width * 0.5F, _type.height * 0.5F, 0.93F);
	} else {
		setDimensions(_type.width, _type.height, -1.0F);
	}
}

void Zombie::setCanBreakDoors(bool can) {
	if (navigation().canNavigateGround()) {
		if (_canBreakDoors != can) {
			_canBreakDoors = can;
			navigation().setCanOpenDoors(can);
			// The BreakDoorGoal (priority 1, hard difficulty) isn't ported: the zombie only paths through doors
		}
	} else {
		_canBreakDoors = false;
	}
}

void Zombie::registerGoals() {
	// ZombieAttackTurtleEggGoal (priority 4) isn't ported: turtle eggs aren't either
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	addBehaviourGoals();
}

void Zombie::addBehaviourGoals() {
	_goalSelector.addGoal(2, std::make_unique<ZombieAttackGoal>(*this, 1.0, false));
	// MoveThroughVillageGoal (priority 6) isn't ported: there are no villages
	_goalSelector.addGoal(7, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers({"ZombifiedPiglin"});
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractVillager", false));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", true));
	// Turtle.BABY_ON_LAND_SELECTOR
	_targetSelector.addGoal(5, std::make_unique<NearestAttackableTargetGoal>(*this, "Turtle", 10, true, false, [](Actor& target, Level&) {
								LivingEntity* living = target.asLiving();
								return living && living->isBaby() && !living->isInWaterNow();
							}));
}

void Zombie::startUnderWaterConversion(int time) {
	_conversionTime = time;
	if (!_drownedConversion) {
		_drownedConversion = true;
		markData(DATA_DROWNED_CONVERSION);
	}
}

void Zombie::tick() {
	if (isAlive() && !isNoAi()) {
		if (_drownedConversion) {
			_conversionTime--;
			if (_conversionTime < 0) doUnderWaterConversion();
		} else if (convertsInWater()) {
			if (_eyeInWater) { // isEyeInFluid(FluidTags.WATER)
				_inWaterTime++;
				if (_inWaterTime >= 600) startUnderWaterConversion(300);
			} else {
				_inWaterTime = -1;
			}
		}
	}
	if (isRemoved()) return; // Converted
	Monster::tick();
}

void Zombie::doUnderWaterConversion() {
	convertToZombieType("minecraft:drowned");
	_level.levelEvent(nullptr, LEVEL_EVENT_ZOMBIE_TO_DROWNED, blockPosition(), 0);
}

void Zombie::convertToZombieType(const std::string& type) {
	int typeId = _level.gameData().getStaticId("minecraft:entity_type", type);
	convertTo(typeId, true, true, [](Mob& mob) {
		if (auto* zombie = dynamic_cast<Zombie*>(&mob)) {
			zombie->handleAttributes(mob.level().getCurrentDifficultyAt(mob.blockPosition()).getSpecialMultiplier());
		}
	});
}

void Zombie::aiStep() {
	if (isAlive()) {
		bool burn = isSunSensitive() && isSunBurnTick();
		if (burn) {
			ItemStack& helmet = _equipment[static_cast<int>(EquipmentSlot::Head)];
			if (!helmet.isEmpty()) {
				const GameData& data = _level.gameData();
				int				max	 = ItemDamage::maxDamage(data, helmet);
				if (max > 0) { // isDamageableItem
					ItemDamage::setDamage(data, helmet, ItemDamage::damage(data, helmet) + _random.nextInt(2));
					if (ItemDamage::damage(data, helmet) >= max) {
						broadcastEntityEvent(EVENT_HEAD_BREAK); // onEquippedItemBroken
						setItemSlot(EquipmentSlot::Head, ItemStack{});
					}
				}
				burn = false;
			}
			if (burn) igniteForTicks(160); // igniteForSeconds(8)
		}
	}
	Monster::aiStep();
}

bool Zombie::hurtServer(const Combat::DamageSource& source, float amount) {
	if (!Monster::hurtServer(source, amount)) return false;
	Actor* target = getTarget();
	if (!target && source.causing && (source.causing->asLiving() || source.causing->isPlayer())) target = source.causing;
	// Reinforcements: hard difficulty, by the spawn_reinforcements chance (ServerLevel.isSpawningMonsters: always)
	if (!target || _level.difficulty() != 3 || !(_random.nextFloat() < getAttributeValue(_ids.spawnReinforcements))) return true;
	int x = Mth::floor(_position.x), y = Mth::floor(_position.y), z = Mth::floor(_position.z);
	std::unique_ptr<Mob> created = _level.mobs().create(_level, _typeId);
	if (!created) return true;
	for (int attempt = 0; attempt < 50; attempt++) {
		int		 rx = x + Mth::nextInt(_random, 7, 40) * Mth::nextInt(_random, -1, 1);
		int		 ry = y + Mth::nextInt(_random, 7, 40) * Mth::nextInt(_random, -1, 1);
		int		 rz = z + Mth::nextInt(_random, 7, 40) * Mth::nextInt(_random, -1, 1);
		BlockPos pos{rx, ry, rz};
		if (!SpawnPlacements::isSpawnPositionOk(_level, _typeId, pos) ||
			!SpawnPlacements::checkSpawnRules(_level, _typeId, MobRegistry::SpawnReason::Reinforcement, pos, _level.random())) {
			continue;
		}
		created->setPos({static_cast<double>(rx), static_cast<double>(ry), static_cast<double>(rz)});
		AABB box = created->boundingBox();
		// hasNearbyAlivePlayer(7), isUnobstructed, noCollision, and no liquid (zombies can't spawn in liquids)
		bool playerNear = false;
		for (const auto& player : _level.players()) {
			if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator || !player->isAlive()) continue;
			double dx = player->getX() - rx, dy = player->getY() - ry, dz = player->getZ() - rz;
			if (dx * dx + dy * dy + dz * dz < 49.0) playerNear = true;
		}
		if (playerNear || _level.countEntities(box, true) > 0 || _level.hasBlockCollision(box) || SpawnPlacements::containsAnyLiquid(_level, box)) continue;
		created->setTarget(target);
		DifficultyInstance difficulty = _level.getCurrentDifficultyAt(created->blockPosition());
		created->finalizeSpawn(difficulty, static_cast<int>(MobRegistry::SpawnReason::Reinforcement));
		Mob* added = static_cast<Mob*>(_level.addFreshEntity(std::move(created)));
		AttributeInstance* chance = _attributes.getInstance(_ids.spawnReinforcements);
		if (chance) {
			double charge = 0.0;
			for (const AttributeModifier& m : chance->modifiers()) {
				if (m.id == REINFORCEMENT_CALLER_CHARGE) charge = m.amount;
			}
			chance->removeModifier(REINFORCEMENT_CALLER_CHARGE);
			chance->addModifier({REINFORCEMENT_CALLER_CHARGE, charge - 0.05, AttributeModifier::Operation::AddValue}, true);
		}
		if (AttributeInstance* callee = added->attributes().getInstance(_ids.spawnReinforcements)) {
			callee->addModifier({REINFORCEMENT_CALLEE_CHARGE, -0.05F, AttributeModifier::Operation::AddValue}, true);
		}
		break;
	}
	return true;
}

bool Zombie::doHurtTarget(Actor& target) {
	bool hurt = Monster::doHurtTarget(target);
	if (hurt) {
		float effective = _level.getCurrentDifficultyAt(blockPosition()).getEffectiveDifficulty();
		if (getMainHandItem().isEmpty() && isOnFire() && _random.nextFloat() < effective * 0.3F) {
			target.igniteForTicks(2 * static_cast<int>(effective) * 20);
		}
	}
	return hurt;
}

void Zombie::populateDefaultEquipmentSlots(DifficultyInstance& difficulty) {
	Monster::populateDefaultEquipmentSlots(difficulty);
	JavaRandom& random = _level.random();
	if (random.nextFloat() < (_level.difficulty() == 3 ? 0.05F : 0.01F)) {
		const char* item = random.nextInt(3) == 0 ? "minecraft:iron_sword" : "minecraft:iron_shovel";
		ItemStack	stack;
		stack.item	= _level.gameData().getStaticId("minecraft:item", item);
		stack.count = 1;
		setItemSlot(EquipmentSlot::MainHand, stack);
	}
}

bool Zombie::wantsToPickUp(const ItemStack& stack) {
	if (_level.gameData().getStaticName("minecraft:item", stack.item) == "minecraft:glow_ink_sac") return false;
	return Monster::wantsToPickUp(stack);
}

std::shared_ptr<SpawnGroupData> Zombie::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	JavaRandom& random = _level.random();
	group			   = Monster::finalizeSpawn(difficulty, reason, group);
	float special	   = difficulty.getSpecialMultiplier();
	bool  conversion   = reason == static_cast<int>(MobRegistry::SpawnReason::Conversion);
	if (!conversion) setCanPickUpLoot(random.nextFloat() < 0.55F * special);
	if (!group) group = std::make_shared<ZombieGroupData>(random.nextFloat() < 0.05F, true); // getSpawnAsBabyOdds
	if (auto* zombieGroup = dynamic_cast<ZombieGroupData*>(group.get())) {
		if (zombieGroup->isBaby) {
			setBaby(true);
			if (zombieGroup->canSpawnJockey) {
				// Chicken jockeys: riding isn't ported, only the random draws are made
				if (random.nextFloat() < 0.05) {
				} else if (random.nextFloat() < 0.05) {
				}
			}
		}
		setCanBreakDoors(random.nextFloat() < special * 0.1F);
		if (!conversion) {
			populateDefaultEquipmentSlots(difficulty);
			populateDefaultEquipmentEnchantments(difficulty);
		}
	}
	if (getItemBySlot(EquipmentSlot::Head).isEmpty()) {
		// Halloween: a pumpkin head
		std::time_t now = std::time(nullptr);
		std::tm		date{};
		localtime_r(&now, &date);
		if (date.tm_mon + 1 == 10 && date.tm_mday == 31 && random.nextFloat() < 0.25F) {
			ItemStack head;
			head.item  = _level.gameData().getStaticId("minecraft:item", random.nextFloat() < 0.1F ? "minecraft:jack_o_lantern" : "minecraft:carved_pumpkin");
			head.count = 1;
			setItemSlot(EquipmentSlot::Head, head);
			setDropChance(EquipmentSlot::Head, 0.0F);
		}
	}
	handleAttributes(special);
	return group;
}

void Zombie::handleAttributes(float special) {
	randomizeReinforcementsChance();
	addOrReplace(_attributes.getInstance(_ids.knockbackResistance),
				 {RANDOM_SPAWN_BONUS, _random.nextDouble() * 0.05F, AttributeModifier::Operation::AddValue});
	double follow = _random.nextDouble() * 1.5 * special;
	if (follow > 1.0) {
		addOrReplace(_attributes.getInstance(_ids.followRange), {ZOMBIE_RANDOM_SPAWN_BONUS, follow, AttributeModifier::Operation::AddMultipliedTotal});
	}
	if (_random.nextFloat() < special * 0.05F) {
		addOrReplace(_attributes.getInstance(_ids.spawnReinforcements),
					 {LEADER_ZOMBIE_BONUS, _random.nextDouble() * 0.25 + 0.5, AttributeModifier::Operation::AddValue});
		addOrReplace(_attributes.getInstance(_ids.maxHealth),
					 {LEADER_ZOMBIE_BONUS, _random.nextDouble() * 3.0 + 1.0, AttributeModifier::Operation::AddMultipliedTotal});
		setCanBreakDoors(true);
	}
}

void Zombie::randomizeReinforcementsChance() {
	if (AttributeInstance* chance = _attributes.getInstance(_ids.spawnReinforcements)) chance->setBaseValue(_random.nextDouble() * 0.1F);
}

void Zombie::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_BABY, onlyNonDefault, !_baby)) writeBoolData(buf, DATA_BABY, _baby);
	if (wantsData(mask, DATA_DROWNED_CONVERSION, onlyNonDefault, !_drownedConversion)) writeBoolData(buf, DATA_DROWNED_CONVERSION, _drownedConversion);
}

void Zombie::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_baby);
	buf.writeBool(_canBreakDoors);
	buf.writeInt(isInWater() ? _inWaterTime : -1);
	buf.writeInt(_drownedConversion ? _conversionTime : -1);
}

void Zombie::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown zombie data version");
	setBaby(buf.readBool());
	setCanBreakDoors(buf.readBool());
	_inWaterTime   = buf.readInt();
	int conversion = buf.readInt();
	if (conversion != -1) {
		startUnderWaterConversion(conversion);
	} else {
		_drownedConversion = false;
	}
	_dirtyData = 0;
}

// ----- Husk -----

bool Husk::doHurtTarget(Actor& target) {
	bool hurt = Zombie::doHurtTarget(target);
	// Hunger for 7 seconds per level of effective difficulty: mob effects aren't ported yet
	return hurt;
}

void Husk::doUnderWaterConversion() {
	convertToZombieType("minecraft:zombie");
	_level.levelEvent(nullptr, LEVEL_EVENT_HUSK_TO_ZOMBIE, blockPosition(), 0);
}
