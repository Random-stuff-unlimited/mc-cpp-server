#include "world/entity/mobs/Zombie.hpp"
#include "world/entity/mobs/Villagers.hpp"

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

// ----- ZombieVillager -----

// VillagerType.byBiome: the villager type of a biome (plains by default)
const char* villagerTypeOfBiome(const GameData& data, int biome) {
		static const std::vector<std::pair<const char*, const char*>> byBiome = {
				{"minecraft:badlands", "minecraft:desert"},			  {"minecraft:desert", "minecraft:desert"},
				{"minecraft:eroded_badlands", "minecraft:desert"},	  {"minecraft:wooded_badlands", "minecraft:desert"},
				{"minecraft:bamboo_jungle", "minecraft:jungle"},	  {"minecraft:jungle", "minecraft:jungle"},
				{"minecraft:sparse_jungle", "minecraft:jungle"},	  {"minecraft:savanna_plateau", "minecraft:savanna"},
				{"minecraft:savanna", "minecraft:savanna"},			  {"minecraft:windswept_savanna", "minecraft:savanna"},
				{"minecraft:deep_frozen_ocean", "minecraft:snow"},	  {"minecraft:frozen_ocean", "minecraft:snow"},
				{"minecraft:frozen_river", "minecraft:snow"},		  {"minecraft:ice_spikes", "minecraft:snow"},
				{"minecraft:snowy_beach", "minecraft:snow"},		  {"minecraft:snowy_taiga", "minecraft:snow"},
				{"minecraft:snowy_plains", "minecraft:snow"},		  {"minecraft:grove", "minecraft:snow"},
				{"minecraft:snowy_slopes", "minecraft:snow"},		  {"minecraft:frozen_peaks", "minecraft:snow"},
				{"minecraft:jagged_peaks", "minecraft:snow"},		  {"minecraft:swamp", "minecraft:swamp"},
				{"minecraft:mangrove_swamp", "minecraft:swamp"},	  {"minecraft:old_growth_spruce_taiga", "minecraft:taiga"},
				{"minecraft:old_growth_pine_taiga", "minecraft:taiga"}, {"minecraft:windswept_gravelly_hills", "minecraft:taiga"},
				{"minecraft:windswept_hills", "minecraft:taiga"},	  {"minecraft:taiga", "minecraft:taiga"},
				{"minecraft:windswept_forest", "minecraft:taiga"},
		};
		for (const auto& [name, type] : byBiome) {
			if (data.getSyncedId("minecraft:worldgen/biome", name) == biome) return type;
		}
		return "minecraft:plains";
}

namespace {
	constexpr int ENTITY_EVENT_CURE_START			= 16;
	constexpr int LEVEL_EVENT_ZOMBIE_VILLAGER_CURED = 1027;
} // namespace

ZombieVillager::ZombieVillager(Level& level, int typeId) : Zombie(level, typeId) {
	// initializeVillagerData, when its entity data is defined: a random profession, the type of the biome where it is
	// made (at 0, 0, 0: it has no position yet)
	const GameData& data	   = level.gameData();
	int				professions = 0;
	while (!data.getStaticName("minecraft:villager_profession", professions).empty()) professions++;
	_profession = professions > 0 ? _random.nextInt(professions) : 0;
	_type		= data.getStaticId("minecraft:villager_type", villagerTypeOfBiome(data, level.getBiomeId({0, 0, 0})));
	_initialType	   = _type;
	_initialProfession = _profession;
}

void ZombieVillager::setVillagerData(int type, int profession, int level) {
	_type		   = type;
	_profession	   = profession;
	_villagerLevel = level;
	markData(DATA_VILLAGER_DATA);
}

void ZombieVillager::tick() {
	if (isAlive() && _converting) {
		_conversionTime -= getConversionProgress();
		if (_conversionTime <= 0) {
			finishConversion();
			return;
		}
	}
	Zombie::tick();
}

bool ZombieVillager::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (!stack.isEmpty() && stack.item == _level.gameData().getStaticId("minecraft:item", "minecraft:golden_apple")) {
		if (!hasEffect(MobEffects::Weakness)) return false; // CONSUME: nothing happens
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
		UUID starter = player.getUUID();
		startConverting(&starter, _random.nextInt(2401) + 3600);
		return true;
	}
	return Zombie::mobInteract(player, hand);
}

void ZombieVillager::startConverting(const UUID* starter, int ticks) {
	_hasStarter = starter != nullptr;
	if (starter) _conversionStarter = *starter;
	_conversionTime = ticks;
	if (!_converting) {
		_converting = true;
		markData(DATA_CONVERTING);
	}
	removeEffect(MobEffects::Weakness);
	addEffect(MobEffects::Instance(MobEffects::Strength, ticks, std::min(_level.difficulty() - 1, 0)));
	broadcastEntityEvent(ENTITY_EVENT_CURE_START); // The cure sound, on the clients
}

int ZombieVillager::getConversionProgress() {
	// Iron bars and beds nearby speed it up now and then
	int progress = 1;
	if (_random.nextFloat() < 0.01F) {
		int				count = 0;
		const GameData& data  = _level.gameData();
		int				bars  = data.getStaticId("minecraft:block", "minecraft:iron_bars");
		int				x0 = static_cast<int>(_position.x), y0 = static_cast<int>(_position.y), z0 = static_cast<int>(_position.z);
		for (int x = x0 - 4; x < x0 + 4 && count < 14; x++) {
			for (int y = y0 - 4; y < y0 + 4 && count < 14; y++) {
				for (int z = z0 - 4; z < z0 + 4 && count < 14; z++) {
					int block = _level.blocks().blockOf(_level.getBlockState({x, y, z}));
					if (block == bars || data.isInTag("minecraft:block", "minecraft:beds", block)) {
						if (_random.nextFloat() < 0.3F) progress++;
						count++;
					}
				}
			}
		}
	}
	return progress;
}

void ZombieVillager::finishConversion() {
	// A villager takes its place, with its villager data and equipment (dropPreservedEquipment); nauseous for 10 s
	BlockPos pos = blockPosition();
	Level&	 level = _level;
	int		 type = _type, profession = _profession, villagerLevel = _villagerLevel;
	std::array<ItemStack, EQUIPMENT_SLOT_COUNT> equipment = _equipment;
	convertTo(level.gameData().getStaticId("minecraft:entity_type", "minecraft:villager"), false, false, [&](Mob& villager) {
		for (int slot = 0; slot < EQUIPMENT_SLOT_COUNT; slot++) {
			if (!equipment[slot].isEmpty() && _dropChances[slot] > 1.0F) villager.setItemSlot(static_cast<EquipmentSlot>(slot), equipment[slot]);
		}
		DifficultyInstance difficulty = level.getCurrentDifficultyAt(villager.blockPosition());
		villager.finalizeSpawn(difficulty, static_cast<int>(MobRegistry::SpawnReason::Conversion));
		// Its villager data is kept (finalizeSpawn would give it its biome's type)
		if (auto* cured = dynamic_cast<Villager*>(&villager)) cured->setVillagerData(type, profession, villagerLevel);
		villager.addEffect(MobEffects::Instance(MobEffects::Nausea, 200, 0));
	});
	level.levelEvent(nullptr, LEVEL_EVENT_ZOMBIE_VILLAGER_CURED, pos, 0);
}

void ZombieVillager::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Zombie::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_CONVERTING, onlyNonDefault, !_converting)) writeBoolData(buf, DATA_CONVERTING, _converting);
	// Always sent: the client draws its own random profession otherwise (vanilla only sends it once it changed, so
	// each client sees another villager)
	if (wantsData(mask, DATA_VILLAGER_DATA, onlyNonDefault, false)) {
		buf.writeUByte(DATA_VILLAGER_DATA);
		buf.writeVarInt(SERIALIZER_VILLAGER_DATA);
		buf.writeVarInt(_type);
		buf.writeVarInt(_profession);
		buf.writeVarInt(_villagerLevel);
	}
}

void ZombieVillager::save(Buffer& buf) const {
	Zombie::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeString(_level.gameData().getStaticName("minecraft:villager_type", _type));
	buf.writeString(_level.gameData().getStaticName("minecraft:villager_profession", _profession));
	buf.writeVarInt(_villagerLevel);
	buf.writeVarInt(_converting ? _conversionTime : -1);
	buf.writeBool(_hasStarter);
	if (_hasStarter) buf.writeUUID(_conversionStarter);
	buf.writeVarInt(_xp);
}

void ZombieVillager::load(Buffer& buf) {
	Zombie::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown zombie villager data version");
	int type	   = _level.gameData().getStaticId("minecraft:villager_type", buf.readString());
	int profession = _level.gameData().getStaticId("minecraft:villager_profession", buf.readString());
	_type		   = type >= 0 ? type : _type;
	_profession	   = profession >= 0 ? profession : _profession;
	_villagerLevel = buf.readVarInt();
	int time	   = buf.readVarInt();
	_hasStarter	   = buf.readBool();
	if (_hasStarter) _conversionStarter = buf.readUUID();
	_xp			   = buf.readVarInt();
	_converting	   = time != -1;
	_conversionTime = time;
	_dirtyData		= 0;
}

// ----- Husk -----

bool Husk::doHurtTarget(Actor& target) {
	bool hurt = Zombie::doHurtTarget(target);
	// Hunger for 7 seconds per level of effective difficulty, bare-handed only
	if (hurt && getMainHandItem().isEmpty() && (target.asPlayer() || target.asLiving())) {
		float effective = _level.getCurrentDifficultyAt(blockPosition()).getEffectiveDifficulty();
		addEffectTo(target, MobEffects::Instance(MobEffects::Hunger, 140 * static_cast<int>(effective)), this);
	}
	return hurt;
}

void Husk::doUnderWaterConversion() {
	convertToZombieType("minecraft:zombie");
	_level.levelEvent(nullptr, LEVEL_EVENT_HUSK_TO_ZOMBIE, blockPosition(), 0);
}
