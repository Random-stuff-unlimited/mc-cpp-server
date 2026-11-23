#include "world/entity/mobs/Piglins.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Arrow.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/Components.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION = 1;

	ItemStack itemOf(Level& level, const char* name, int count = 1) { return ItemStack(level.gameData().getStaticId("minecraft:item", name), count); }
	bool	  isItem(Level& level, const ItemStack& stack, const char* name) {
		 return !stack.isEmpty() && stack.item == level.gameData().getStaticId("minecraft:item", name);
	}
	bool isItemIn(Level& level, const ItemStack& stack, const char* tag) {
		return !stack.isEmpty() && level.gameData().isInTag("minecraft:item", tag, stack.item);
	}
	ItemStack potion(Level& level, const char* item, const char* name) {
		const GameData& data  = level.gameData();
		ItemStack		stack = itemOf(level, item);
		Components::set(stack, data, "minecraft:potion_contents", Components::encodePotion(data.getStaticId("minecraft:potion", name)));
		return stack;
	}

	// The gameplay/piglin_bartering loot table (its soul speed book and boots come without their enchantment here)
	ItemStack barter(Level& level, JavaRandom& random) {
		struct Entry {
			const char* item;
			int			weight, min, max;
		};
		static const Entry ENTRIES[] = {{"minecraft:iron_boots", 8, 1, 1},		  {"potion:fire_resistance", 8, 1, 1},	 {"splash:fire_resistance", 8, 1, 1},
										{"potion:water", 10, 1, 1},				  {"minecraft:iron_nugget", 10, 10, 36}, {"minecraft:ender_pearl", 10, 2, 4},
										{"minecraft:string", 20, 3, 9},			  {"minecraft:quartz", 20, 5, 12},		 {"minecraft:obsidian", 40, 1, 1},
										{"minecraft:crying_obsidian", 40, 1, 3}, {"minecraft:fire_charge", 40, 1, 1},	 {"minecraft:leather", 40, 2, 4},
										{"minecraft:soul_sand", 40, 2, 8},		  {"minecraft:nether_brick", 40, 2, 8}, {"minecraft:spectral_arrow", 40, 6, 12},
										{"minecraft:gravel", 40, 8, 16},		  {"minecraft:blackstone", 40, 8, 16}};
		int total = 0;
		for (const Entry& e : ENTRIES) total += e.weight;
		int roll = random.nextInt(total);
		for (const Entry& e : ENTRIES) {
			if ((roll -= e.weight) >= 0) continue;
			std::string item = e.item;
			if (item.rfind("potion:", 0) == 0) return potion(level, "minecraft:potion", ("minecraft:" + item.substr(7)).c_str());
			if (item.rfind("splash:", 0) == 0) return potion(level, "minecraft:splash_potion", ("minecraft:" + item.substr(7)).c_str());
			return itemOf(level, e.item, Mth::nextInt(random, e.min, e.max));
		}
		return itemOf(level, "minecraft:gravel");
	}

	// PiglinAi's GoToWantedItem: the gold it sees within 9 blocks
	class PiglinPickUpGoal : public Goal {
	  public:
		explicit PiglinPickUpGoal(Piglin& piglin) : _piglin(piglin) { setFlags({Flag::Move}); }
		bool canUse() override {
			if (_piglin.getTarget() || _piglin.isAdmiring()) return false;
			ItemEntity* found = nullptr;
			double		best  = 0.0;
			for (Actor* actor : _piglin.level().actorsIn(_piglin.boundingBox().inflate(9.0, 4.0, 9.0), &_piglin)) {
				auto* item = dynamic_cast<ItemEntity*>(actor);
				if (!item || item->isRemoved() || item->pickupDelay() > 0 || !_piglin.wantsToPickUpItem(item->item())) continue;
				double d = item->distanceToSqr(_piglin);
				if (!found || d < best) found = item, best = d;
			}
			if (!found) return false;
			_itemId = found->id();
			return true;
		}
		bool canContinueToUse() override { return item() && !_piglin.isAdmiring() && !_piglin.getTarget(); }
		void tick() override {
			ItemEntity* item = this->item();
			if (!item) return;
			if (_piglin.boundingBox().inflate(1.0, 0.0, 1.0).intersects(item->boundingBox())) {
				// PiglinAi.pickUpItem: a gold ingot one at a time, anything else whole
				ItemStack stack = item->item();
				ItemStack taken = isItem(_piglin.level(), stack, "minecraft:gold_ingot") ? stack.copyWithCount(1) : stack;
				stack.shrink(taken.count);
				if (stack.isEmpty() || stack.count <= 0) {
					item->discard();
				} else {
					item->setItem(stack);
				}
				_piglin.startAdmiring(taken);
				return;
			}
			if (_piglin.navigation().isDone() || _piglin.tickCount() % 10 == 0) _piglin.navigation().moveTo(item->position().x, item->position().y, item->position().z, 1.0);
		}

	  private:
		Piglin& _piglin;
		int		_itemId = -1;
		ItemEntity* item() {
			auto* item = dynamic_cast<ItemEntity*>(_piglin.level().actorById(_itemId));
			return item && !item->isRemoved() ? item : nullptr;
		}
	};

	// While admiring it stands still and looks at its gold
	class PiglinAdmireGoal : public Goal {
	  public:
		explicit PiglinAdmireGoal(Piglin& piglin) : _piglin(piglin) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override { return _piglin.isAdmiring(); }
		void start() override { _piglin.navigation().stop(); }

	  private:
		Piglin& _piglin;
	};

	// A sword piglin's (or brute's) melee: only without a crossbow in hand
	class PiglinMeleeGoal : public MeleeAttackGoal {
	  public:
		PiglinMeleeGoal(Mob& mob, CrossbowAttackMob* crossbowUser) : MeleeAttackGoal(mob, 1.0, true), _user(crossbowUser) {}
		bool canUse() override { return (!_user || !_user->isHoldingCrossbow()) && MeleeAttackGoal::canUse(); }

	  private:
		CrossbowAttackMob* _user;
	};
} // namespace

// ===================== AbstractPiglin =====================

AbstractPiglin::AbstractPiglin(Level& level, int typeId) : Monster(level, typeId) {
	setPathfindingMalus(PathType::DangerFire, 16.0F);
	setPathfindingMalus(PathType::DamageFire, -1.0F);
	setCanPickUpLoot(false);
}

void AbstractPiglin::setImmuneToZombification(bool immune) {
	if (immune == _immuneToZombification) return;
	_immuneToZombification = immune;
	markData(DATA_IMMUNE_TO_ZOMBIFICATION);
}

bool AbstractPiglin::isConverting() const { return !_level.dimensionType().piglinSafe && !_immuneToZombification && !isNoAi(); }

bool AbstractPiglin::isWearingSafeArmor(Level& level, Actor& actor) {
	if (Player* player = actor.asPlayer()) {
		for (int slot = 5; slot <= 8; slot++) {
			if (isItemIn(level, player->inventory().get(slot), "minecraft:piglin_safe_armor")) return true;
		}
		return false;
	}
	if (auto* mob = dynamic_cast<Mob*>(&actor)) {
		for (EquipmentSlot slot : {EquipmentSlot::Head, EquipmentSlot::Chest, EquipmentSlot::Legs, EquipmentSlot::Feet}) {
			if (isItemIn(level, mob->getItemBySlot(slot), "minecraft:piglin_safe_armor")) return true;
		}
	}
	return false;
}

void AbstractPiglin::customServerAiStep() {
	if (isConverting()) {
		if (++_timeInOverworld > CONVERSION_TIME) {
			// AbstractPiglin.finishConversion: a zombified piglin, with its equipment, nauseous for 10 seconds
			playConvertedSound();
			convertTo(_level.gameData().getStaticId("minecraft:entity_type", "minecraft:zombified_piglin"), true, false,
					  [](Mob& zombified) { zombified.addEffect({MobEffects::Nausea, 200, 0}); });
			return;
		}
	} else {
		_timeInOverworld = 0;
	}
	Monster::customServerAiStep();
}

void AbstractPiglin::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_IMMUNE_TO_ZOMBIFICATION, onlyNonDefault, !_immuneToZombification)) {
		writeBoolData(buf, DATA_IMMUNE_TO_ZOMBIFICATION, _immuneToZombification);
	}
}

void AbstractPiglin::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_immuneToZombification);
	buf.writeInt(_timeInOverworld);
}

void AbstractPiglin::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown piglin data version");
	setImmuneToZombification(buf.readBool());
	_timeInOverworld = buf.readInt();
}

// ===================== Piglin =====================

Piglin::Piglin(Level& level, int typeId) : AbstractPiglin(level, typeId) { _xpReward = 5; }

void Piglin::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<AvoidEntityGoal>(*this, "ZombifiedPiglin", 6.0F, 1.0, 1.2));
	_goalSelector.addGoal(1, std::make_unique<AvoidEntityGoal>(*this, "Zoglin", 6.0F, 1.0, 1.2));
	_goalSelector.addGoal(2, std::make_unique<PiglinAdmireGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<RangedCrossbowAttackGoal>(*this, *this, 1.0, 8.0F));
	_goalSelector.addGoal(3, std::make_unique<PiglinMeleeGoal>(*this, this));
	_goalSelector.addGoal(4, std::make_unique<PiglinPickUpGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers(); // PiglinAi.broadcastAngerTarget
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																			 [](Actor& actor, Level& level) { return !AbstractPiglin::isWearingSafeArmor(level, actor); }));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "WitherSkeleton", true));
}

void Piglin::setBaby(bool baby) {
	if (baby != _baby) {
		_baby = baby;
		markData(DATA_BABY);
	}
	if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) {
		speed->removeModifier("minecraft:baby");
		if (baby) speed->addModifier({"minecraft:baby", 0.2, AttributeModifier::Operation::AddMultipliedBase}, false);
	}
	if (baby) {
		setDimensions(_type.width * 0.5F, _type.height * 0.5F, 0.97F);
	} else {
		setDimensions(_type.width, _type.height, -1.0F);
	}
}

void Piglin::setChargingCrossbow(bool charging) {
	if (charging == _chargingCrossbow) return;
	_chargingCrossbow = charging;
	markData(DATA_IS_CHARGING_CROSSBOW);
}

bool Piglin::isHoldingCrossbow() { return isCrossbow(_level, getMainHandItem()); }

bool Piglin::canAttack(Actor& target) { return !_baby && !isAdmiring() && AbstractPiglin::canAttack(target); }

bool Piglin::isLovedItem(const ItemStack& stack) { return isItemIn(_level, stack, "minecraft:piglin_loved"); }

bool Piglin::wantsToPickUpItem(const ItemStack& stack) {
	if (isAdmiring() || getTarget() || _baby && !isItem(_level, stack, "minecraft:gold_ingot")) return false;
	if (isItem(_level, stack, "minecraft:gold_ingot")) return true;
	return isLovedItem(stack) && _inventory.size() < 8;
}

void Piglin::startAdmiring(ItemStack stack) {
	setItemSlot(EquipmentSlot::OffHand, stack);
	setDropChance(EquipmentSlot::OffHand, 2.0F);
	_admireTicks = ADMIRE_DURATION;
	navigation().stop();
	playSound("minecraft:entity.piglin.admiring_item", 1.0F, 1.0F);
}

void Piglin::finishAdmiring() {
	// PiglinAi.stopHoldingOffHandItem: an ingot is bartered, a loved item kept
	ItemStack held = getItemInHand(1);
	setItemSlot(EquipmentSlot::OffHand, ItemStack{});
	setDropChance(EquipmentSlot::OffHand, DEFAULT_DROP_CHANCE);
	if (held.isEmpty()) return;
	if (isItem(_level, held, "minecraft:gold_ingot") && !_baby) {
		for (int i = 0; i < held.count; i++) throwItem(barter(_level, _random));
	} else if (_inventory.size() < 8) {
		_inventory.push_back(held);
	} else {
		throwItem(held);
	}
}

void Piglin::throwItem(ItemStack stack) {
	// BehaviorUtils.throwItem: toward the nearest player within 16 blocks, else ahead
	Actor* player = nullptr;
	double best	  = 16.0 * 16.0;
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(16.0), this)) {
		if (actor->asPlayer() && actor->distanceToSqr(*this) < best) player = actor, best = actor->distanceToSqr(*this);
	}
	Vec3 from{_position.x, eyeY() - 0.3, _position.z};
	Vec3 to = player ? (player->position() + Vec3{0.0, 1.0, 0.0} - from) : Vec3{-std::sin(_yRot * (float)M_PI / 180.0F), 0.0, std::cos(_yRot * (float)M_PI / 180.0F)};
	auto item = std::make_unique<ItemEntity>(_level, from, std::move(stack), to.normalize().scale(0.3));
	item->setPickupDelay(40);
	_level.entities().add(std::move(item));
}

void Piglin::customServerAiStep() {
	if (_admireTicks > 0 && --_admireTicks == 0) finishAdmiring();
	AbstractPiglin::customServerAiStep();
}

bool Piglin::hurtServer(const Combat::DamageSource& source, float amount) {
	bool hurt = AbstractPiglin::hurtServer(source, amount);
	if (hurt && isAdmiring()) {
		// Hurt while admiring: it drops the item and fights
		_admireTicks = 0;
		ItemStack held = getItemInHand(1);
		setItemSlot(EquipmentSlot::OffHand, ItemStack{});
		if (!held.isEmpty()) spawnAtLocation(held);
	}
	return hurt;
}

bool Piglin::mobInteract(Player& player, int hand) {
	// PiglinAi.mobInteract: a gold ingot from the hand is taken and admired
	ItemStack held = player.getStackInHand(hand);
	if (isItem(_level, held, "minecraft:gold_ingot") && !isAdmiring() && !_baby && !getTarget()) {
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
		startAdmiring(held.copyWithCount(1));
		return true;
	}
	return AbstractPiglin::mobInteract(player, hand);
}

void Piglin::performRangedAttack(Actor& target, float) {
	auto   arrow = std::make_unique<Arrow>(_level, *this, itemOf(_level, "minecraft:arrow"), &getMainHandItem());
	AABB   box	 = target.boundingBox();
	double dx	 = target.position().x - _position.x;
	double dy	 = target.position().y + (box.maxY - box.minY) / 3.0 - arrow->position().y;
	double dz	 = target.position().z - _position.z;
	double d	 = std::sqrt(dx * dx + dz * dz);
	arrow->setCritArrow(true);
	arrow->shoot(dx, dy + d * 0.2F, dz, 1.6F, static_cast<float>(14 - _level.difficulty() * 4));
	_level.entities().add(std::move(arrow));
	playSound("minecraft:item.crossbow.shoot", 1.0F, 1.0F / (_random.nextFloat() * 0.4F + 0.8F));
}

std::shared_ptr<SpawnGroupData> Piglin::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	JavaRandom& random = _level.random();
	if (reason != static_cast<int>(MobRegistry::SpawnReason::Structure) && random.nextFloat() < 0.2F) setBaby(true);
	if (!_baby) {
		// Piglin.createSpawnWeapon: a crossbow or a golden sword; each golden armor piece 1 time in 10
		setItemSlot(EquipmentSlot::MainHand, itemOf(_level, random.nextFloat() < 0.5F ? "minecraft:crossbow" : "minecraft:golden_sword"));
		const std::pair<EquipmentSlot, const char*> ARMOR[] = {{EquipmentSlot::Head, "minecraft:golden_helmet"},
															   {EquipmentSlot::Chest, "minecraft:golden_chestplate"},
															   {EquipmentSlot::Legs, "minecraft:golden_leggings"},
															   {EquipmentSlot::Feet, "minecraft:golden_boots"}};
		for (const auto& [slot, item] : ARMOR) {
			if (random.nextFloat() < 0.1F) setItemSlot(slot, itemOf(_level, item));
		}
	}
	return AbstractPiglin::finalizeSpawn(difficulty, reason, group);
}

std::string Piglin::ambientSoundName() {
	if (getTarget()) return "minecraft:entity.piglin.angry";
	if (isAdmiring()) return "minecraft:entity.piglin.admiring_item";
	return "minecraft:entity.piglin.ambient";
}

void Piglin::dropAllDeathLoot(const Combat::DamageSource& source) {
	AbstractPiglin::dropAllDeathLoot(source);
	for (const ItemStack& stack : _inventory) spawnAtLocation(stack);
	_inventory.clear();
}

void Piglin::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractPiglin::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_BABY, onlyNonDefault, !_baby)) writeBoolData(buf, DATA_BABY, _baby);
	if (wantsData(mask, DATA_IS_CHARGING_CROSSBOW, onlyNonDefault, !_chargingCrossbow)) writeBoolData(buf, DATA_IS_CHARGING_CROSSBOW, _chargingCrossbow);
}

void Piglin::save(Buffer& buf) const {
	AbstractPiglin::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_baby);
	buf.writeVarInt(static_cast<int>(_inventory.size()));
	for (const ItemStack& stack : _inventory) {
		buf.writeVarInt(stack.item);
		buf.writeVarInt(stack.count);
		buf.writeVarInt(static_cast<int32_t>(stack.components.size()));
		buf.writeBytes(stack.components);
	}
}

void Piglin::load(Buffer& buf) {
	AbstractPiglin::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown piglin data version");
	setBaby(buf.readBool());
	_inventory.clear();
	int count = buf.readVarInt();
	for (int i = 0; i < count; i++) {
		ItemStack stack;
		stack.item		 = buf.readVarInt();
		stack.count		 = buf.readVarInt();
		stack.components = buf.readBytes(static_cast<size_t>(buf.readVarInt()));
		if (!stack.isEmpty()) _inventory.push_back(stack);
	}
}

// ===================== PiglinBrute =====================

PiglinBrute::PiglinBrute(Level& level, int typeId) : AbstractPiglin(level, typeId) { _xpReward = 20; }

void PiglinBrute::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<PiglinMeleeGoal>(*this, nullptr));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.6));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "WitherSkeleton", true));
}

std::shared_ptr<SpawnGroupData> PiglinBrute::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	setItemSlot(EquipmentSlot::MainHand, itemOf(_level, "minecraft:golden_axe"));
	return AbstractPiglin::finalizeSpawn(difficulty, reason, group);
}
