#include "world/entity/mobs/Witch.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Throwables.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/Components.hpp"

#include <cmath>

namespace {
	constexpr const char* SPEED_MODIFIER_DRINKING = "minecraft:drinking";
	constexpr int		  EVENT_WITCH_PARTICLES	  = 15;

	float healthOf(Actor& actor) {
		if (Player* player = actor.asPlayer()) return player->combat().health;
		if (LivingEntity* living = actor.asLiving()) return living->health();
		return 0.0F;
	}
	bool hasEffect(Actor& actor, MobEffects::Effect effect) {
		if (Player* player = actor.asPlayer()) return player->hasEffect(effect);
		if (LivingEntity* living = actor.asLiving()) return living->hasEffect(effect);
		return false;
	}
} // namespace

void Witch::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<RangedAttackGoal>(*this, 1.0, 60, 10.0F));
	_goalSelector.addGoal(2, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(3, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Raider"}));
	// NearestAttackableWitchTargetGoal: always allowed without a raid to heal
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false));
}

void Witch::setUsingItem(bool using_) {
	if (using_ == _usingItem) return;
	_usingItem = using_;
	markData(DATA_USING_ITEM);
}

ItemStack Witch::potionStack(const char* item, const char* potion) const {
	// PotionContents.createItemStack
	const GameData& data = _level.gameData();
	ItemStack		stack(data.getStaticId("minecraft:item", item), 1);
	Components::set(stack, data, "minecraft:potion_contents", Components::encodePotion(data.getStaticId("minecraft:potion", potion)));
	return stack;
}

void Witch::aiStep() {
	if (isAlive()) {
		if (_usingItem) {
			if (_usingTime-- <= 0) {
				// Drunk: its effects, the bottle gone, its speed back
				setUsingItem(false);
				ItemStack		drunk = getMainHandItem();
				const GameData& data  = _level.gameData();
				setItemSlot(EquipmentSlot::MainHand, ItemStack{});
				if (!drunk.isEmpty() && drunk.item == data.getStaticId("minecraft:item", "minecraft:potion")) {
					if (std::optional<std::vector<uint8_t>> contents = Components::get(drunk, data, "minecraft:potion_contents")) {
						int potion = Components::decodePotion(*contents);
						// forEachEffect(this::addEffect): added like any effect, the instant ones too
						if (potion >= 0) {
							for (const MobEffects::Instance& effect : MobEffects::potionEffects(data.getStaticName("minecraft:potion", potion))) addEffect(effect);
						}
					}
				}
				if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) speed->removeModifier(SPEED_MODIFIER_DRINKING);
			}
		} else {
			const char* potion = nullptr;
			bool		burnt  = isOnFire() || _level.gameData().isInTag("minecraft:damage_type", "minecraft:is_fire",
																	   _level.gameData().getSyncedId("minecraft:damage_type", getLastDamageType()));
			Actor*		target = nullptr;
			if (_random.nextFloat() < 0.15F && _eyeInWater && !hasEffect(MobEffects::WaterBreathing)) {
				potion = "minecraft:water_breathing";
			} else if (_random.nextFloat() < 0.15F && burnt && !hasEffect(MobEffects::FireResistance)) {
				potion = "minecraft:fire_resistance";
			} else if (_random.nextFloat() < 0.05F && _health < maxHealth()) {
				potion = "minecraft:healing";
			} else if (_random.nextFloat() < 0.5F && (target = getTarget()) != nullptr && !hasEffect(MobEffects::Speed) &&
					   target->distanceToSqr(*this) > 121.0) {
				potion = "minecraft:swiftness";
			}
			if (potion) {
				setItemSlot(EquipmentSlot::MainHand, potionStack("minecraft:potion", potion));
				_usingTime = useDuration(getMainHandItem());
				setUsingItem(true);
				playSound("minecraft:entity.witch.drink", 1.0F, 0.8F + _random.nextFloat() * 0.4F);
				if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) {
					speed->removeModifier(SPEED_MODIFIER_DRINKING);
					speed->addModifier({SPEED_MODIFIER_DRINKING, -0.25, AttributeModifier::Operation::AddValue}, false);
				}
			}
		}
		if (_random.nextFloat() < 7.5E-4F) broadcastEntityEvent(EVENT_WITCH_PARTICLES);
	}
	Monster::aiStep();
}

float Witch::damageAfterMagicAbsorb(const Combat::DamageSource& source, float amount, int damageType) const {
	amount = Monster::damageAfterMagicAbsorb(source, amount, damageType);
	if (source.causing == static_cast<const Actor*>(this)) amount = 0.0F;
	if (_level.gameData().isInTag("minecraft:damage_type", "minecraft:witch_resistant_to", damageType)) amount *= 0.15F;
	return amount;
}

void Witch::performRangedAttack(Actor& target, float) {
	if (_usingItem) return;
	Vec3 movement{};
	if (Entity* entity = target.asEntity()) movement = entity->deltaMovement();
	double dx		= target.position().x + movement.x - _position.x;
	double dy		= target.eyeY() - 1.1F - _position.y;
	double dz		= target.position().z + movement.z - _position.z;
	double distance = std::sqrt(dx * dx + dz * dz);
	const char* potion = "minecraft:harming";
	const GameData::EntityTypeInfo* type = _level.gameData().getEntityType(target.typeId());
	if (type && type->is("Raider")) {
		potion = healthOf(target) <= 4.0F ? "minecraft:healing" : "minecraft:regeneration";
		setTarget(nullptr);
	} else if (distance >= 8.0 && !::hasEffect(target, MobEffects::Slowness)) {
		potion = "minecraft:slowness";
	} else if (healthOf(target) >= 8.0F && !::hasEffect(target, MobEffects::Poison)) {
		potion = "minecraft:poison";
	} else if (distance <= 3.0 && !::hasEffect(target, MobEffects::Weakness) && _random.nextFloat() < 0.25F) {
		potion = "minecraft:weakness";
	}
	auto thrown = std::make_unique<ThrownSplashPotion>(_level, this, potionStack("minecraft:splash_potion", potion));
	thrown->shoot(dx, dy + distance * 0.2, dz, 0.75F, 8.0F);
	_level.entities().add(std::move(thrown));
	playSound("minecraft:entity.witch.throw", 1.0F, 0.8F + _random.nextFloat() * 0.4F);
}

void Witch::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_USING_ITEM, onlyNonDefault, !_usingItem)) writeBoolData(buf, DATA_USING_ITEM, _usingItem);
}
