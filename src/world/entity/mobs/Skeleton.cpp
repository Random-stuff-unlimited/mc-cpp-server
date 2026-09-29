#include "world/entity/mobs/Skeleton.hpp"

#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/ItemDamage.hpp"

namespace {
	constexpr int EVENT_HEAD_BREAK = 49; // LivingEntity.EVENT_ITEM_BREAK for the head
} // namespace

void Skeleton::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<RangedAttackGoal>(*this, 1.0, 40, 15.0F));
	_goalSelector.addGoal(3, std::make_unique<RestrictSunGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<FleeSunGoal>(*this, 1.0));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractVillager", false));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "IronGolem", true));
}

void Skeleton::populateDefaultEquipmentSlots(DifficultyInstance& difficulty) {
	Monster::populateDefaultEquipmentSlots(difficulty);
	// AbstractSkeleton: the bow, always
	int bow = _level.gameData().getStaticId("minecraft:item", "minecraft:bow");
	ItemStack stack;
	stack.item	= bow;
	stack.count = 1;
	setItemSlot(EquipmentSlot::MainHand, stack);
}

void Skeleton::aiStep() {
	if (isAlive()) {
		// Skeleton burns in daylight unless it wears a helmet, which wears out instead (like the zombie)
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
	}
	Monster::aiStep();
}