#ifndef SKELETON_HPP
#define SKELETON_HPP

#include "world/entity/mobs/Monster.hpp"

#include <memory>

class Arrow;
class MeleeAttackGoal;
class RangedBowAttackGoal;

// AbstractSkeleton: shoots arrows with its bow (RangedBowAttackGoal: strafes around its target, draws for a second),
// or fights in melee without one (reassessWeaponGoal, whenever what it holds changes). Burns in daylight unless it
// wears a helmet, which wears out instead; flees the sun and wolves
class AbstractSkeleton : public Monster {
  public:
	AbstractSkeleton(Level& level, int typeId);

	void							registerGoals() override;
	void							populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	// AbstractSkeleton.finalizeSpawn: the bow (and maybe armor), enchantments, loot pick up, Halloween pumpkin
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	// AbstractSkeleton.reassessWeaponGoal: the bow goal with a bow in hand, the melee goal otherwise
	void							reassessWeaponGoal();
	void							performRangedAttack(Actor& target, float power) override;
	void							setItemSlot(EquipmentSlot slot, ItemStack stack) override;
	void							load(Buffer& buf) override;

  protected:
	std::shared_ptr<RangedBowAttackGoal> _bowGoal;
	std::shared_ptr<MeleeAttackGoal>	 _meleeGoal;

	void		 aiStep() override;
	// The ticks between two shots: hard difficulty, and the others
	virtual int	 hardAttackInterval() const { return 20; }
	virtual int	 attackInterval() const { return 40; }
	// AbstractSkeleton.getArrow: ProjectileUtil.getMobArrow, then what the type adds (slowness, poison, fire)
	virtual void prepareArrow(Arrow&) {}
};

// Skeleton: turns into a stray after 7 seconds in powder snow (15 seconds of shaking)
class Skeleton : public AbstractSkeleton {
  public:
	static constexpr int DATA_STRAY_CONVERSION = 16;

	using AbstractSkeleton::AbstractSkeleton;
	bool isFreezeConverting() const { return _freezeConverting; }
	void startFreezeConversion(int ticks);
	void tick() override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _freezeConverting = false;
	int	 _inPowderSnowTime = 0, _conversionTime = 0;

	void setFreezeConverting(bool converting);
	void doFreezeConversion();
	bool isInPowderSnow();
};

// Stray: a skeleton of the frozen lands, its arrows slow down what they hit
class Stray : public AbstractSkeleton {
  public:
	using AbstractSkeleton::AbstractSkeleton;

  protected:
	void prepareArrow(Arrow& arrow) override;
};

// Bogged: a skeleton of the swamps, slower to shoot, its arrows poison; shears take its mushrooms
class Bogged : public AbstractSkeleton {
  public:
	static constexpr int DATA_SHEARED = 16;

	using AbstractSkeleton::AbstractSkeleton;
	bool isSheared() const { return _sheared; }
	bool readyForShearing() const { return !_sheared && isAlive(); }
	void shear();
	bool mobInteract(Player& player, int hand) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	int	 hardAttackInterval() const override { return 50; }
	int	 attackInterval() const override { return 70; }
	void prepareArrow(Arrow& arrow) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _sheared = false;
	void setSheared(bool sheared);
};

// WitherSkeleton: a skeleton of the nether fortresses with a stone sword, whose hits wither; hunts piglins too
class WitherSkeleton : public AbstractSkeleton {
  public:
	WitherSkeleton(Level& level, int typeId);
	void							registerGoals() override;
	void							populateDefaultEquipmentSlots(DifficultyInstance& difficulty) override;
	void							populateDefaultEquipmentEnchantments(DifficultyInstance&) override {}
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	bool							doHurtTarget(Actor& target) override;
	bool							canBeAffected(MobEffects::Effect effect) const override { return effect != MobEffects::Wither && AbstractSkeleton::canBeAffected(effect); }
	bool							canHoldItem(const ItemStack& stack) override;

  protected:
	void prepareArrow(Arrow& arrow) override;
};

#endif
