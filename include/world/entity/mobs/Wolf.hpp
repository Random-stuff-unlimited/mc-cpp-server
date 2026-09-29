#ifndef WOLF_HPP
#define WOLF_HPP

#include "world/entity/Mob.hpp"

// Wolf: wild wolves hunt small animals (and skeletons), and a player can tame one with bones (a third of the time).
// A tamed wolf follows its owner, attacks what attacks it or its owner, is healed by feeding it meat, and wears a
// collar the client renders
class Wolf : public Mob {
  public:
	Wolf(Level& level, int typeId);

	bool	isTamed() const { return _tamed; }
	Actor*	owner() const override;
	Actor*	getOwnerEntity() override { return owner(); }
	int		collarColor() const { return _collarColor; }

	void registerGoals() override;
	bool mobInteract(Player& player, int hand) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;
	bool wantsToPickUp(const ItemStack& stack) override;

  private:
	bool		_tamed		 = false;
	EntityRef	_owner;
	int			_collarColor = 14; // DyeColor.RED, the default collar
	bool		_isMeat(const ItemStack& stack) const;
	void		tame(Player& player);
	void		usePlayerItem(Player& player, int hand);
};

#endif