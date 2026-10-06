#include "world/entity/mobs/Skeleton.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/Arrow.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/ItemDamage.hpp"

#include <cmath>
#include <ctime>

namespace {
	constexpr int SAVE_VERSION				  = 1;
	constexpr int EVENT_HEAD_BREAK			  = 49; // LivingEntity.EVENT_ITEM_BREAK for the head
	constexpr int LEVEL_EVENT_SKELETON_CONVERTS = 1048;

	ItemStack itemOf(Level& level, const char* name) { return ItemStack(level.gameData().getStaticId("minecraft:item", name), 1); }
} // namespace

// ===================== AbstractSkeleton =====================

AbstractSkeleton::AbstractSkeleton(Level& level, int typeId) : Monster(level, typeId) {
	_bowGoal   = std::make_shared<RangedBowAttackGoal>(*this, 1.0, 20, 15.0F);
	_meleeGoal = std::make_shared<MeleeAttackGoal>(*this, 1.2, false); // Its start and stop set it aggressive
}

void AbstractSkeleton::registerGoals() {
	_goalSelector.addGoal(2, std::make_unique<RestrictSunGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<FleeSunGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<AvoidEntityGoal>(*this, "Wolf", 6.0F, 1.0, 1.2));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(6, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", true));
	// Turtle.BABY_ON_LAND_SELECTOR
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Turtle", 10, true, false, [](Actor& target, Level&) {
		LivingEntity* turtle = target.asLiving();
		return turtle && turtle->isBaby() && !turtle->isInWaterNow();
	}));
	reassessWeaponGoal(); // In its constructor in vanilla: once the goals exist
}

void AbstractSkeleton::reassessWeaponGoal() {
	_goalSelector.removeGoal(*_meleeGoal);
	_goalSelector.removeGoal(*_bowGoal);
	// ProjectileUtil.getWeaponHoldingHand: the main hand if it holds a bow, else the off hand
	int				 bow  = _level.gameData().getStaticId("minecraft:item", "minecraft:bow");
	const ItemStack& held = !getMainHandItem().isEmpty() && getMainHandItem().item == bow ? getMainHandItem() : getItemInHand(1);
	if (!held.isEmpty() && held.item == bow) {
		_bowGoal->setMinAttackInterval(_level.difficulty() != 3 ? attackInterval() : hardAttackInterval());
		_goalSelector.addSharedGoal(4, std::static_pointer_cast<Goal>(_bowGoal));
	} else {
		_goalSelector.addSharedGoal(4, std::static_pointer_cast<Goal>(_meleeGoal));
	}
}

void AbstractSkeleton::setItemSlot(EquipmentSlot slot, ItemStack stack) {
	Monster::setItemSlot(slot, std::move(stack));
	reassessWeaponGoal(); // onEquipItem
}

void AbstractSkeleton::populateDefaultEquipmentSlots(DifficultyInstance& difficulty) {
	Monster::populateDefaultEquipmentSlots(difficulty);
	setItemSlot(EquipmentSlot::MainHand, itemOf(_level, "minecraft:bow"));
}

std::shared_ptr<SpawnGroupData> AbstractSkeleton::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group			   = Monster::finalizeSpawn(difficulty, reason, group);
	JavaRandom& random = _level.random();
	populateDefaultEquipmentSlots(difficulty);
	populateDefaultEquipmentEnchantments(difficulty);
	reassessWeaponGoal();
	setCanPickUpLoot(random.nextFloat() < 0.55F * difficulty.getSpecialMultiplier());
	if (getItemBySlot(EquipmentSlot::Head).isEmpty()) {
		// Halloween: a pumpkin head
		std::time_t now = std::time(nullptr);
		std::tm		date{};
		localtime_r(&now, &date);
		if (date.tm_mon + 1 == 10 && date.tm_mday == 31 && random.nextFloat() < 0.25F) {
			setItemSlot(EquipmentSlot::Head, itemOf(_level, random.nextFloat() < 0.1F ? "minecraft:jack_o_lantern" : "minecraft:carved_pumpkin"));
			setDropChance(EquipmentSlot::Head, 0.0F);
		}
	}
	return group;
}

void AbstractSkeleton::aiStep() {
	// Burns in daylight unless it wears a helmet, which wears out instead
	if (isSunBurnTick()) {
		ItemStack& helmet = _equipment[static_cast<int>(EquipmentSlot::Head)];
		if (!helmet.isEmpty()) {
			const GameData& data = _level.gameData();
			int				max	 = ItemDamage::maxDamage(data, helmet);
			if (max > 0) {
				ItemDamage::setDamage(data, helmet, ItemDamage::damage(data, helmet) + _random.nextInt(2));
				if (ItemDamage::damage(data, helmet) >= max) {
					broadcastEntityEvent(EVENT_HEAD_BREAK); // onEquippedItemBroken
					setItemSlot(EquipmentSlot::Head, ItemStack{});
				}
			}
		} else {
			igniteForTicks(160); // igniteForSeconds(8)
		}
	}
	Monster::aiStep();
}

void AbstractSkeleton::performRangedAttack(Actor& target, float power) {
	// The bow in hand (for its enchantments), an arrow of it (ProjectileUtil.getMobArrow), shot at a third of the
	// target's height, raised by 20 % of the distance, with a spread by difficulty
	int				 bow	= _level.gameData().getStaticId("minecraft:item", "minecraft:bow");
	const ItemStack& weapon = !getMainHandItem().isEmpty() && getMainHandItem().item == bow ? getMainHandItem() : getItemInHand(1);
	auto			 arrow	= std::make_unique<Arrow>(_level, *this, itemOf(_level, "minecraft:arrow"), weapon.isEmpty() ? nullptr : &weapon);
	arrow->setBaseDamageFromMob(power);
	prepareArrow(*arrow);
	AABB   box = target.boundingBox();
	double dx  = target.position().x - _position.x;
	double dy  = target.position().y + (box.maxY - box.minY) / 3.0 - arrow->position().y;
	double dz  = target.position().z - _position.z;
	double d   = std::sqrt(dx * dx + dz * dz);
	arrow->shoot(dx, dy + d * 0.2F, dz, 1.6F, static_cast<float>(14 - _level.difficulty() * 4));
	_level.entities().add(std::move(arrow));
	playSound("minecraft:entity.skeleton.shoot", 1.0F, 1.0F / (_random.nextFloat() * 0.4F + 0.8F));
}

void AbstractSkeleton::load(Buffer& buf) {
	Monster::load(buf);
	reassessWeaponGoal();
}

// ===================== Skeleton =====================

void Skeleton::setFreezeConverting(bool converting) {
	if (converting == _freezeConverting) return;
	_freezeConverting = converting;
	markData(DATA_STRAY_CONVERSION);
}

void Skeleton::startFreezeConversion(int ticks) {
	_conversionTime = ticks;
	setFreezeConverting(true);
}

bool Skeleton::isInPowderSnow() {
	// Entity.isInPowderSnow: a powder snow block inside its box
	AABB box	= boundingBox().deflate(1.0E-5);
	int	 powder = _level.gameData().getStaticId("minecraft:block", "minecraft:powder_snow");
	for (int x = Mth::floor(box.minX); x <= Mth::floor(box.maxX); x++) {
		for (int y = Mth::floor(box.minY); y <= Mth::floor(box.maxY); y++) {
			for (int z = Mth::floor(box.minZ); z <= Mth::floor(box.maxZ); z++) {
				if (_level.blocks().blockOf(_level.getBlockState({x, y, z})) == powder) return true;
			}
		}
	}
	return false;
}

void Skeleton::tick() {
	if (isAlive() && !isNoAi()) {
		if (isInPowderSnow()) {
			if (_freezeConverting) {
				if (--_conversionTime < 0) {
					doFreezeConversion();
					return;
				}
			} else if (++_inPowderSnowTime >= 140) {
				startFreezeConversion(300);
			}
		} else {
			_inPowderSnowTime = -1;
			setFreezeConverting(false);
		}
	}
	AbstractSkeleton::tick();
}

void Skeleton::doFreezeConversion() {
	BlockPos pos = blockPosition();
	Level&	 level = _level;
	convertTo(level.gameData().getStaticId("minecraft:entity_type", "minecraft:stray"), true, true);
	level.levelEvent(nullptr, LEVEL_EVENT_SKELETON_CONVERTS, pos, 0);
}

void Skeleton::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractSkeleton::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_STRAY_CONVERSION, onlyNonDefault, !_freezeConverting)) writeBoolData(buf, DATA_STRAY_CONVERSION, _freezeConverting);
}

void Skeleton::save(Buffer& buf) const {
	AbstractSkeleton::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeVarInt(_freezeConverting ? _conversionTime : -1); // StrayConversionTime
}

void Skeleton::load(Buffer& buf) {
	AbstractSkeleton::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown skeleton data version");
	int conversion = buf.readVarInt();
	if (conversion != -1) {
		startFreezeConversion(conversion);
	} else {
		setFreezeConverting(false);
	}
	_dirtyData = 0;
}

// ===================== Stray =====================

void Stray::prepareArrow(Arrow& arrow) { arrow.addEffect(MobEffects::Instance(MobEffects::Slowness, 600)); }

// ===================== Bogged =====================

void Bogged::prepareArrow(Arrow& arrow) { arrow.addEffect(MobEffects::Instance(MobEffects::Poison, 100)); }

void Bogged::setSheared(bool sheared) {
	if (sheared == _sheared) return;
	_sheared = sheared;
	markData(DATA_SHEARED);
}

void Bogged::shear() {
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.bogged.shear", Level::SoundSource::Players, 1.0F, 1.0F);
	// dropFromShearingLootTable(BOGGED_SHEAR): at the top of it
	for (ItemStack& stack : rollLootTable("minecraft:shearing/bogged")) spawnAtLocation(std::move(stack), _height);
	setSheared(true);
}

bool Bogged::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (!stack.isEmpty() && stack.item == _level.gameData().getStaticId("minecraft:item", "minecraft:shears") && readyForShearing()) {
		shear();
		ItemDamage::hurtAndBreak(_level, player.inventory().getMutable(player.handSlot(hand)), 1, &player, player.handSlot(hand));
		return true;
	}
	return AbstractSkeleton::mobInteract(player, hand);
}

void Bogged::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractSkeleton::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SHEARED, onlyNonDefault, !_sheared)) writeBoolData(buf, DATA_SHEARED, _sheared);
}

void Bogged::save(Buffer& buf) const {
	AbstractSkeleton::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_sheared);
}

void Bogged::load(Buffer& buf) {
	AbstractSkeleton::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown bogged data version");
	_sheared   = buf.readBool();
	_dirtyData = 0;
}

// ===================== WitherSkeleton =====================

WitherSkeleton::WitherSkeleton(Level& level, int typeId) : AbstractSkeleton(level, typeId) { setPathfindingMalus(PathType::Lava, 8.0F); }

void WitherSkeleton::registerGoals() {
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractPiglin", true));
	AbstractSkeleton::registerGoals();
}

void WitherSkeleton::populateDefaultEquipmentSlots(DifficultyInstance&) { setItemSlot(EquipmentSlot::MainHand, itemOf(_level, "minecraft:stone_sword")); }

std::shared_ptr<SpawnGroupData> WitherSkeleton::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group = AbstractSkeleton::finalizeSpawn(difficulty, reason, group);
	if (AttributeInstance* damage = _attributes.getInstance(_ids.attackDamage)) damage->setBaseValue(4.0);
	reassessWeaponGoal();
	return group;
}

bool WitherSkeleton::doHurtTarget(Actor& target) {
	if (!AbstractSkeleton::doHurtTarget(target)) return false;
	addEffectTo(target, MobEffects::Instance(MobEffects::Wither, 200), this);
	return true;
}

bool WitherSkeleton::canHoldItem(const ItemStack& stack) {
	return !_level.gameData().isInTag("minecraft:item", "minecraft:wither_skeleton_disliked_weapons", stack.item) && AbstractSkeleton::canHoldItem(stack);
}

void WitherSkeleton::prepareArrow(Arrow& arrow) { arrow.igniteForTicks(2000); } // igniteForSeconds(100)
