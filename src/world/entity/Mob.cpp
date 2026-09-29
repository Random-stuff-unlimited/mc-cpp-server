#include "world/entity/Mob.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/item/Components.hpp"
#include "world/item/Enchantments.hpp"
#include "world/item/ItemDamage.hpp"
#include "world/entity/SpawnPlacements.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/Shapes.hpp"

#include <cmath>
#include <limits>

namespace {
	constexpr int	 SAVE_VERSION = 2;
	constexpr int	 SAVED_NO_AI = 1, SAVED_LEFT_HANDED = 2, SAVED_PERSISTENT = 4, SAVED_CAN_PICK_UP_LOOT = 8;
	const double	 DEFAULT_ATTACK_REACH = std::sqrt(2.04F) - 0.6F; // Mob.DEFAULT_ATTACK_REACH
	constexpr double ITEM_PICKUP_REACH_XZ = 1.0, ITEM_PICKUP_REACH_Y = 0.0;

	void writeStack(Buffer& buf, const ItemStack& stack) {
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.item);
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.count);
		buf.writeVarInt(static_cast<int32_t>(stack.components.size()));
		buf.writeBytes(stack.components);
	}
	ItemStack readStack(Buffer& buf) {
		ItemStack stack;
		stack.item		 = buf.readVarInt();
		stack.count		 = buf.readVarInt();
		stack.components = buf.readBytes(static_cast<size_t>(buf.readVarInt()));
		return stack.isEmpty() ? ItemStack{} : stack;
	}
} // namespace

Mob::Mob(Level& level, int typeId)
	: LivingEntity(level, typeId), _moveControl(std::make_unique<MoveControl>(*this)), _lookControl(std::make_unique<LookControl>(*this)),
	  _jumpControl(std::make_unique<JumpControl>(*this)), _bodyRotationControl(*this), _sensing(*this) {
	_pathfindingMalus.fill(std::numeric_limits<float>::quiet_NaN());
	_dropChances.fill(DEFAULT_DROP_CHANCE);
}

PathNavigation& Mob::navigation() {
	if (!_navigation) _navigation = createNavigation();
	return *_navigation;
}

std::unique_ptr<PathNavigation> Mob::createNavigation() { return std::make_unique<GroundPathNavigation>(*this, _level); }

float Mob::getPathfindingMalus(PathType type) const {
	float malus = _pathfindingMalus[static_cast<int>(type)];
	return std::isnan(malus) ? defaultMalus(type) : malus;
}

// Mob.getMaxFallDistance: further with a target, if it has the health for it
int Mob::getMaxFallDistance() {
	if (!const_cast<Mob*>(this)->getTarget()) return 3;
	int fall = static_cast<int>(_health - maxHealth() * 0.33F);
	fall -= (3 - _level.difficulty()) * 4;
	return std::max(fall, 0) + 3;
}

void Mob::setMobFlag(int flag, bool set) {
	uint8_t flags = static_cast<uint8_t>(set ? (_mobFlags | flag) : (_mobFlags & ~flag));
	if (flags == _mobFlags) return;
	_mobFlags = flags;
	markData(DATA_MOB_FLAGS);
}

// ----- Target -----

Actor* Mob::getTarget() {
	if (!_target.isSet()) return nullptr;
	Actor* target = _level.actorByRef(_target);
	if (!target) _target.clear();
	return target;
}

void Mob::setTarget(Actor* target) {
	if (target) {
		_target = EntityRef(*target);
	} else {
		_target.clear();
	}
}

bool Mob::canAttackType(int typeId) const { return _level.gameData().getStaticName("minecraft:entity_type", typeId) != "minecraft:ghast"; }

bool Mob::canAttack(Actor& target) {
	if (&target == this) return false;
	if (Player* player = target.asPlayer()) {
		if (_level.difficulty() == 0) return false;
		return !player->isCreative() && player->isAlive() && !player->isSpectator(); // canBeSeenAsEnemy
	}
	return target.isAlive() && !target.isSpectator();
}

// ----- Combat -----

bool Mob::doHurtTarget(Actor& target) {
	float			 damage = static_cast<float>(getAttributeValue(_ids.attackDamage));
	const ItemStack& weapon = getMainHandItem();
	Combat::DamageSource source{"minecraft:mob_attack", this, nullptr, std::nullopt};
	damage		= Enchantments::modifyDamage(_level, weapon, target, source, damage);
	bool hurt	= target.hurtServer(source, damage);
	if (!hurt) return false;
	// getKnockback: the attack_knockback attribute, with the weapon's enchantments
	float knockback = static_cast<float>(_attributes.hasAttribute(_ids.attackKnockback) ? getAttributeValue(_ids.attackKnockback) : 0.0);
	knockback		= Enchantments::modifyKnockback(_level, weapon, target, source, knockback);
	if (knockback > 0.0F) {
		float sin = Mth::sin(_yRot * (float)(M_PI / 180.0)), cos = -Mth::cos(_yRot * (float)(M_PI / 180.0));
		if (LivingEntity* living = target.asLiving()) {
			living->knockback(knockback * 0.5F, sin, cos);
		} else if (Player* player = target.asPlayer()) {
			double strength = knockback * 0.5F;
			Vec3   push		= Vec3{sin, 0.0, cos}.normalize().scale(strength);
			player->pushMotion({-push.x, player->isOnGround() ? std::min(0.4, strength) : 0.0, -push.z});
		}
		_delta = _delta.multiply(0.6, 1.0, 0.6);
	}
	// ItemStack.hurtEnemy: the weapon wears
	if (!weapon.isEmpty()) {
		const GameData::ItemProperties* props = _level.gameData().getItemProperties(weapon.item);
		if (props && props->isWeapon) ItemDamage::hurtAndBreak(_level, _equipment[static_cast<int>(EquipmentSlot::MainHand)], props->weaponDamagePerAttack, nullptr, 0);
	}
	Enchantments::doPostAttackEffects(_level, weapon, *this, target, source);
	setLastHurtMob(&target);
	playAttackSound();
	return true;
}

bool Mob::isWithinMeleeAttackRange(Actor& target) {
	AABB attack = boundingBox().inflate(DEFAULT_ATTACK_REACH, 0.0, DEFAULT_ATTACK_REACH);
	return attack.intersects(target.boundingBox()); // getHitbox: the bounding box
}

void Mob::lookAt(Actor& target, float maxYRot, float maxXRot) {
	double dx = target.position().x - _position.x, dz = target.position().z - _position.z;
	double dy;
	if (target.asLiving() || target.isPlayer()) {
		dy = target.eyeY() - eyeY();
	} else {
		AABB box = target.boundingBox();
		dy		 = (box.minY + box.maxY) / 2.0 - eyeY();
	}
	double horizontal = std::sqrt(dx * dx + dz * dz);
	float  yRot		  = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
	float  xRot		  = static_cast<float>(-(Mth::atan2(dy, horizontal) * 180.0F / (float)M_PI));
	auto   rotlerp	  = [](float from, float to, float max) { return from + Mth::clamp(Mth::wrapDegrees(to - from), -max, max); };
	_xRot			  = rotlerp(_xRot, xRot, maxXRot);
	_yRot			  = rotlerp(_yRot, yRot, maxYRot);
}

float Mob::lightLevelDependentMagicValue() {
	if (!_level.hasChunkAt(blockPosition())) return 0.0F;
	return _level.getLightLevelDependentMagicValue({Mth::floor(_position.x), Mth::floor(eyeY()), Mth::floor(_position.z)});
}

bool Mob::isSunBurnTick() {
	if (!_level.isBrightOutside()) return false;
	BlockPos eye{Mth::floor(_position.x), Mth::floor(eyeY()), Mth::floor(_position.z)};
	float	 light = lightLevelDependentMagicValue();
	// isInWaterOrRain
	bool rain = _level.isRainingAt(blockPosition()) || _level.isRainingAt({Mth::floor(_position.x), Mth::floor(boundingBox().maxY), Mth::floor(_position.z)});
	bool wet  = isInWater() || rain;
	return light > 0.5F && _random.nextFloat() * 30.0F < (light - 0.4F) * 2.0F && !wet && _level.canSeeSky(eye);
}

bool Mob::mobInteract(Player&, int) { return false; }

Mob* Mob::convertTo(int typeId, bool keepEquipment, bool preserveCanPickUpLoot, const std::function<void(Mob&)>& finalize) {
	if (isRemoved()) return nullptr;
	std::unique_ptr<Mob> mob = _level.mobs().create(_level, typeId);
	if (!mob) return nullptr;
	// ConversionType.SINGLE.convert
	mob->snapTo(_position, _yRot, _xRot);
	mob->setYHeadRot(_yHeadRot);
	mob->setDeltaMovement(_delta);
	if (keepEquipment) {
		for (int slot = 0; slot < EQUIPMENT_SLOT_COUNT; slot++) {
			if (_equipment[slot].isEmpty()) continue;
			mob->setItemSlot(static_cast<EquipmentSlot>(slot), _equipment[slot]);
			mob->_dropChances[slot] = _dropChances[slot];
			_equipment[slot]		= ItemStack{};
		}
	}
	mob->_fallDistance				 = _fallDistance;
	mob->_lastHurtByPlayer			 = _lastHurtByPlayer;
	mob->_lastHurtByPlayerMemoryTime = _lastHurtByPlayerMemoryTime;
	mob->_hurtTime					 = _hurtTime;
	mob->_yBodyRot					 = _yBodyRot;
	mob->setOnGround(_onGround);
	// convertCommon
	if (isBaby()) mob->setBaby(true);
	if (preserveCanPickUpLoot) mob->setCanPickUpLoot(_canPickUpLoot);
	mob->setLeftHanded(isLeftHanded());
	mob->setNoAi(isNoAi());
	if (_persistenceRequired) mob->setPersistenceRequired();
	mob->portal.cooldown = portal.cooldown;
	if (finalize) finalize(*mob);
	Mob* added = static_cast<Mob*>(_level.addFreshEntity(std::move(mob)));
	discard();
	return added;
}

// ----- Equipment -----

int Mob::equipmentForSlot(EquipmentSlot slot, int tier) const {
	static const char* MATERIALS[6] = {"leather", "copper", "golden", "chainmail", "iron", "diamond"};
	const char*		   piece;
	switch (slot) {
	case EquipmentSlot::Head: piece = "helmet"; break;
	case EquipmentSlot::Chest: piece = "chestplate"; break;
	case EquipmentSlot::Legs: piece = "leggings"; break;
	case EquipmentSlot::Feet: piece = "boots"; break;
	default: return -1;
	}
	if (tier < 0 || tier > 5) return -1;
	return _level.gameData().getStaticId("minecraft:item", std::string("minecraft:") + MATERIALS[tier] + "_" + piece);
}

void Mob::populateDefaultEquipmentSlots(DifficultyInstance& difficulty) {
	JavaRandom& random = _level.random();
	if (!(random.nextFloat() < 0.15F * difficulty.getSpecialMultiplier())) return;
	int tier = random.nextInt(3);
	for (int i = 1; i <= 3; i++) {
		if (random.nextFloat() < 0.1087F) tier++;
	}
	float stopChance = _level.difficulty() == 3 ? 0.1F : 0.25F;
	bool  first		 = true;
	for (EquipmentSlot slot : {EquipmentSlot::Head, EquipmentSlot::Chest, EquipmentSlot::Legs, EquipmentSlot::Feet}) {
		if (!first && random.nextFloat() < stopChance) break;
		first = false;
		if (!getItemBySlot(slot).isEmpty()) continue;
		int item = equipmentForSlot(slot, tier);
		if (item >= 0) setItemSlot(slot, ItemStack(item, 1));
	}
}

void Mob::enchantSpawnedEquipment(EquipmentSlot slot, float chance, DifficultyInstance& difficulty) {
	ItemStack& stack = _equipment[static_cast<int>(slot)];
	if (stack.isEmpty() || !(_level.random().nextFloat() < chance * difficulty.getSpecialMultiplier())) return;
	Enchantments::enchantItemFromProvider(_level.gameData(), stack, "minecraft:mob_spawn_equipment", difficulty.getSpecialMultiplier(), _level.random());
}

void Mob::populateDefaultEquipmentEnchantments(DifficultyInstance& difficulty) {
	enchantSpawnedEquipment(EquipmentSlot::MainHand, 0.25F, difficulty);
	for (EquipmentSlot slot : {EquipmentSlot::Feet, EquipmentSlot::Legs, EquipmentSlot::Chest, EquipmentSlot::Head}) {
		enchantSpawnedEquipment(slot, 0.5F, difficulty);
	}
}

double Mob::approximateAttributeWith(const ItemStack& stack, int attribute, EquipmentSlot slot) {
	double value = _attributes.hasAttribute(attribute) ? _attributes.getBaseValue(attribute) : 0.0;
	const GameData::ItemProperties* props = stack.isEmpty() ? nullptr : _level.gameData().getItemProperties(stack.item);
	if (!props) return value;
	// ItemAttributeModifiers.compute: additions, then multiples
	double base = value;
	for (const auto& m : props->attributeModifiers) {
		if (m.attribute == attribute && EquipmentSlots::groupContains(m.slot, slot) && m.operation == 0) value += m.amount;
	}
	for (const auto& m : props->attributeModifiers) {
		if (m.attribute == attribute && EquipmentSlots::groupContains(m.slot, slot) && m.operation == 1) value += base * m.amount;
	}
	for (const auto& m : props->attributeModifiers) {
		if (m.attribute == attribute && EquipmentSlots::groupContains(m.slot, slot) && m.operation == 2) value *= 1.0 + m.amount;
	}
	return value;
}

bool Mob::canReplaceCurrentItem(const ItemStack& candidate, const ItemStack& current, EquipmentSlot slot) {
	if (current.isEmpty()) return true;
	const GameData& data = _level.gameData();
	auto			equal = [&]() {
		   // canReplaceEqualItem: more enchantments, less damage, a custom name
		   size_t a = Enchantments::of(data, candidate).size(), b = Enchantments::of(data, current).size();
		   if (a != b) return a > b;
		   int da = ItemDamage::damage(data, candidate), db = ItemDamage::damage(data, current);
		   if (da != db) return da < db;
		   return Components::get(candidate, data, "minecraft:custom_name").has_value() && !Components::get(current, data, "minecraft:custom_name").has_value();
	};
	if (EquipmentSlots::isArmor(slot)) {
		// compareArmor (binding curse keeps the current piece)
		for (auto [id, level] : Enchantments::of(data, current)) {
			const auto* d = Enchantments::definition(data, id);
			if (d && d->contains("effects") && d->at("effects").contains("minecraft:prevent_armor_change")) return false;
		}
		double armorA = approximateAttributeWith(candidate, _ids.armor, slot), armorB = approximateAttributeWith(current, _ids.armor, slot);
		if (armorA != armorB) return armorA > armorB;
		double toughA = approximateAttributeWith(candidate, _ids.armorToughness, slot), toughB = approximateAttributeWith(current, _ids.armorToughness, slot);
		if (toughA != toughB) return toughA > toughB;
		return equal();
	}
	if (slot != EquipmentSlot::MainHand) return false;
	double damageA = approximateAttributeWith(candidate, _ids.attackDamage, slot), damageB = approximateAttributeWith(current, _ids.attackDamage, slot);
	if (damageA != damageB) return damageA > damageB;
	return equal();
}

ItemStack Mob::equipItemIfPossible(const ItemStack& stack) {
	const GameData::ItemProperties* props = _level.gameData().getItemProperties(stack.item);
	EquipmentSlot					slot  = props ? EquipmentSlots::fromGameData(props->equipmentSlot) : EquipmentSlot::MainHand;
	// isEquippableInSlot: humanoid armor goes on, bodies and saddles need their own mobs
	if (slot == EquipmentSlot::Body || slot == EquipmentSlot::Saddle) return {};
	const ItemStack current = getItemBySlot(slot);
	bool			replace = canReplaceCurrentItem(stack, current, slot);
	if (EquipmentSlots::isArmor(slot) && !replace) {
		slot	= EquipmentSlot::MainHand;
		replace = getItemBySlot(slot).isEmpty();
	}
	if (!replace || !canHoldItem(stack)) return {};
	const ItemStack& old = getItemBySlot(slot);
	float			 chance = _dropChances[static_cast<int>(slot)];
	if (!old.isEmpty() && std::max(_random.nextFloat() - 0.1F, 0.0F) < chance) spawnAtLocation(old);
	ItemStack equipped = stack.copyWithCount(1); // EquipmentSlot.limit: one per slot
	setItemSlot(slot, equipped);
	setGuaranteedDrop(slot);
	_persistenceRequired = true;
	return equipped;
}

void Mob::pickUpItem(ItemEntity& item) {
	ItemStack taken = equipItemIfPossible(item.item());
	if (taken.isEmpty()) return;
	// Mob.take: the pickup animation for the players around
	Buffer take;
	take.writeVarInt(item.id());
	take.writeVarInt(_id);
	take.writeVarInt(taken.count);
	_level.entities().broadcast(item, PacketId::Play::Clientbound::TAKE_ITEM_ENTITY, take);
	ItemStack rest = item.item();
	rest.shrink(taken.count);
	if (rest.isEmpty()) {
		item.discard();
	} else {
		item.setItem(rest);
	}
}

ItemEntity* Mob::spawnAtLocation(ItemStack stack, float yOffset) {
	if (stack.isEmpty()) return nullptr;
	auto item = ItemEntity::create(_level, {_position.x, _position.y + yOffset, _position.z}, std::move(stack));
	item->setPickupDelay(10);
	return static_cast<ItemEntity*>(_level.entities().add(std::move(item)));
}

void Mob::dropAllDeathLoot(const Combat::DamageSource& source) {
	LivingEntity::dropAllDeathLoot(source);
	// dropCustomDeathLoot: each slot by its chance (looting raises it), a random wear unless it was picked up
	bool killedByPlayer = lastHurtByPlayer() >= 0;
	for (int i = 0; i < EQUIPMENT_SLOT_COUNT; i++) {
		ItemStack& stack  = _equipment[i];
		float	   chance = _dropChances[i];
		if (chance == 0.0F) continue;
		bool preserved = chance > 1.0F;
		if (source.causing && (source.causing->asLiving() || source.causing->isPlayer())) {
			const ItemStack* weapon = nullptr;
			if (LivingEntity* living = source.causing->asLiving()) weapon = &living->getMainHandItem();
			if (Player* player = source.causing->asPlayer()) weapon = &player->getStackInHand(0);
			if (weapon) chance = Enchantments::processEquipmentDropChance(_level, *weapon, *this, source, chance);
		}
		if (stack.isEmpty() || !(killedByPlayer || preserved) || !(_random.nextFloat() < chance)) continue;
		int max = ItemDamage::maxDamage(_level.gameData(), stack);
		if (!preserved && max > 0) ItemDamage::setDamage(_level.gameData(), stack, max - _random.nextInt(1 + _random.nextInt(std::max(max - 3, 1))));
		spawnAtLocation(stack);
		stack = ItemStack{};
	}
}

// ----- Spawning and ticking -----

std::shared_ptr<SpawnGroupData> Mob::finalizeSpawn(DifficultyInstance&, int, std::shared_ptr<SpawnGroupData> group) {
	JavaRandom&		   random = _level.random();
	AttributeInstance* follow = _attributes.getInstance(_ids.followRange);
	if (follow && !follow->hasModifier("minecraft:random_spawn_bonus")) {
		double bonus = 0.0 + 0.11485000000000001 * (random.nextDouble() - random.nextDouble()); // RandomSource.triangle
		follow->addModifier({"minecraft:random_spawn_bonus", bonus, AttributeModifier::Operation::AddMultipliedBase}, true);
	}
	setLeftHanded(random.nextFloat() < 0.05F);
	return group;
}

void Mob::playAmbientSound() {
	std::string sound = typeSound("ambient", "");
	if (!sound.empty()) playSound(sound, 1.0F, voicePitch());
}

void Mob::livingBaseTick() {
	LivingEntity::livingBaseTick();
	if (isAlive() && _random.nextInt(1000) < _ambientSoundTime++) {
		_ambientSoundTime = -ambientSoundInterval();
		playAmbientSound();
	}
}

void Mob::playHurtSound() {
	_ambientSoundTime = -ambientSoundInterval();
	LivingEntity::playHurtSound();
}

void Mob::tick() {
	LivingEntity::tick();
	if (_tickCount % 5 == 0) {
		// updateControlFlags: nothing rides or is ridden, every control stays available
		_goalSelector.setControlFlag(Goal::Flag::Move, true);
		_goalSelector.setControlFlag(Goal::Flag::Jump, true);
		_goalSelector.setControlFlag(Goal::Flag::Look, true);
	}
}

// Mob.aiStep: then it picks up the items it wants
void Mob::aiStep() {
	LivingEntity::aiStep();
	if (!_canPickUpLoot || !isAlive() || _dead) return;
	for (ItemEntity* item : _level.entities().itemsIn(boundingBox().inflate(ITEM_PICKUP_REACH_XZ, ITEM_PICKUP_REACH_Y, ITEM_PICKUP_REACH_XZ))) {
		if (!item->isRemoved() && !item->item().isEmpty() && item->pickupDelay() == 0 && wantsToPickUp(item->item())) pickUpItem(*item);
	}
}

// Mob.serverAiStep: where the AI runs, in vanilla's order
void Mob::serverAiStep() {
	_noActionTime++;
	_sensing.tick();
	if ((_tickCount + _id) % 2 != 0 && _tickCount > 1) {
		_targetSelector.tickRunningGoals(false);
		_goalSelector.tickRunningGoals(false);
	} else {
		_targetSelector.tick();
		_goalSelector.tick();
	}
	navigation().tick();
	customServerAiStep();
	_moveControl->tick();
	_lookControl->tick();
	_jumpControl->tick();
}

bool Mob::removeWhenFarAway(double) const {
	for (const char* keeps : {"Animal", "AbstractGolem", "Villager", "WanderingTrader", "Warden", "Allay"}) {
		if (_type.is(keeps)) return false;
	}
	return true;
}

// Mob.checkDespawn: monsters in peaceful go, the others far from every player
void Mob::checkDespawn() {
	if (_level.difficulty() == 0 && !_type.allowedInPeaceful) {
		discard();
		return;
	}
	if (_persistenceRequired) {
		_noActionTime = 0;
		return;
	}
	double nearest = std::numeric_limits<double>::max();
	bool   found   = false;
	for (const auto& player : _level.players()) {
		if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator) continue; // EntitySelector.NO_SPECTATORS
		double dx = player->getX() - _position.x, dy = player->getY() - _position.y, dz = player->getZ() - _position.z;
		nearest	  = std::min(nearest, dx * dx + dy * dy + dz * dz);
		found	  = true;
	}
	if (!found) return;
	// MobCategory.getDespawnDistance / getNoDespawnDistance: 128 (64 for water ambient) and 32
	int despawn = _type.category == GameData::MobCategory::WaterAmbient ? 64 : 128;
	if (nearest > despawn * despawn && removeWhenFarAway(nearest)) {
		discard();
		return;
	}
	if (_noActionTime > 600 && _random.nextInt(800) == 0 && nearest > 32 * 32 && removeWhenFarAway(nearest)) {
		discard();
	} else if (nearest < 32 * 32) {
		_noActionTime = 0;
	}
}

void Mob::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	LivingEntity::writeData(buf, mask, onlyNonDefault);
	if ((mask & (1u << DATA_MOB_FLAGS)) && !(onlyNonDefault && _mobFlags == 0)) {
		buf.writeUByte(DATA_MOB_FLAGS);
		buf.writeVarInt(SERIALIZER_BYTE);
		buf.writeUByte(_mobFlags);
	}
}

void Mob::save(Buffer& buf) const {
	LivingEntity::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(static_cast<uint8_t>((isNoAi() ? SAVED_NO_AI : 0) | (isLeftHanded() ? SAVED_LEFT_HANDED : 0) | (_persistenceRequired ? SAVED_PERSISTENT : 0) |
										(_canPickUpLoot ? SAVED_CAN_PICK_UP_LOOT : 0)));
	for (int i = 0; i < EQUIPMENT_SLOT_COUNT; i++) {
		writeStack(buf, _equipment[i]);
		buf.writeFloat(_dropChances[i]);
	}
}

void Mob::load(Buffer& buf) {
	LivingEntity::load(buf);
	int version = buf.readUByte();
	if (version != 1 && version != SAVE_VERSION) throw std::runtime_error("unknown mob data version");
	uint8_t flags		 = buf.readUByte();
	_mobFlags			 = static_cast<uint8_t>((flags & SAVED_NO_AI ? FLAG_NO_AI : 0) | (flags & SAVED_LEFT_HANDED ? FLAG_LEFT_HANDED : 0));
	_persistenceRequired = flags & SAVED_PERSISTENT;
	_canPickUpLoot		 = flags & SAVED_CAN_PICK_UP_LOOT;
	if (version >= 2) {
		for (int i = 0; i < EQUIPMENT_SLOT_COUNT; i++) {
			_equipment[i]	= readStack(buf);
			_dropChances[i] = buf.readFloat();
		}
	}
	_dirtyData = 0;
}

// ----- Natural spawning -----

bool Mob::checkSpawnRules(int) { return !_type.is("PathfinderMob") || getWalkTargetValue(blockPosition()) >= 0.0F; }

bool Mob::checkSpawnObstruction() {
	AABB box		 = boundingBox();
	bool unobstructed = _level.countEntities(box, true) == 0; // Level.isUnobstructed: no living entity (blocksBuilding) there
	if (_type.is("WaterAnimal") || _type.is("AgeableWaterCreature") || _type.is("Axolotl") || _type.is("Strider") || _type.is("Drowned") ||
		_type.is("Guardian")) {
		return unobstructed;
	}
	if (_type.is("Ravager")) return !SpawnPlacements::containsAnyLiquid(_level, box);
	if (_type.is("Ocelot")) {
		if (!unobstructed || SpawnPlacements::containsAnyLiquid(_level, box)) return false;
		BlockPos pos = blockPosition();
		if (pos.y < _level.seaLevel()) return false;
		int below = _level.getBlockState(pos.below());
		int block = _level.blocks().blockOf(below);
		return _level.gameData().getStaticName("minecraft:block", block) == "minecraft:grass_block" ||
			   _level.gameData().isInTag("minecraft:block", "minecraft:leaves", block);
	}
	if (_type.is("IronGolem")) {
		BlockPos pos = blockPosition();
		// BlockState.entityCanStandOn: a sturdy top face
		if (!(_level.gameData().getStateProperties(_level.getBlockState(pos.below())).faceSturdy & (1 << (1 * 3)))) return false;
		for (int i = 1; i < 3; i++) {
			BlockPos above = BlockPos{pos.x, pos.y + i, pos.z};
			if (!SpawnPlacements::isValidEmptySpawnBlock(_level, above, _level.getBlockState(above), _typeId)) return false;
		}
		int state = _level.getBlockState(pos);
		return !Shapes::isFullBlock(_level.gameData().getCollisionShape(state)) && !_level.isSignalSource(state) &&
			   !_level.gameData().isInTag("minecraft:block", "minecraft:prevent_mob_spawning_inside", _level.blocks().blockOf(state)) &&
			   !SpawnPlacements::isBlockDangerous(_level, state, _typeId) && unobstructed;
	}
	return !SpawnPlacements::containsAnyLiquid(_level, box) && unobstructed;
}

int Mob::getMaxSpawnClusterSize() const {
	if (_type.is("AbstractFish")) return 8; // Schooling fish: their max school size (the same 8 unless overridden)
	if (_type.is("Wolf")) return 8;
	if (_type.is("AbstractHorse")) return 6;
	if (_type.is("Ghast") || _type.is("Pillager") || _type.is("HappyGhast")) return 1;
	return 4;
}

bool Mob::isPanicking() const {
	for (const auto& wrapped : _goalSelector.goals()) {
		if (wrapped->isRunning() && dynamic_cast<PanicGoal*>(&wrapped->goal())) return true;
	}
	return false;
}
