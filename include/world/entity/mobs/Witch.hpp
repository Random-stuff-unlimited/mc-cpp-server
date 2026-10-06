#ifndef WITCH_HPP
#define WITCH_HPP

#include "world/entity/mobs/Monster.hpp"

// Witch: throws splash potions at its target (slowness from afar, poison, weakness up close, else harming), and
// drinks one when it needs it (water breathing, fire resistance, healing, swiftness to catch up). Magic barely hurts
// it. The raids' part of Raider (banners, waves, healing the raiders) isn't ported
class Witch : public Monster {
  public:
	static constexpr int DATA_IS_CELEBRATING = 16, DATA_USING_ITEM = 17;

	using Monster::Monster;
	bool isDrinkingPotion() const { return _usingItem; }
	void registerGoals() override;
	void performRangedAttack(Actor& target, float power) override;

  protected:
	void  aiStep() override;
	float damageAfterMagicAbsorb(const Combat::DamageSource& source, float amount, int damageType) const override;
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _usingItem = false;
	int	 _usingTime = 0;

	void	  setUsingItem(bool using_);
	ItemStack potionStack(const char* item, const char* potion) const;
};

#endif
