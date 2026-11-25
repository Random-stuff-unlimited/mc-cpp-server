#ifndef COMPOSTER_HPP
#define COMPOSTER_HPP

#include "world/blocks/Redstone.hpp"

#include <memory>
#include <string>

class ItemStack;
class Container;

// ComposterBlock: compostable items raise its level (by their chance, always for the first layer), at 7 it ripens for
// a second, at 8 a click takes out bone meal. Comparators read the level. Hoppers don't use it yet
class ComposterBlock : public AnalogOutputBlock {
  public:
	explicit ComposterBlock(std::shared_ptr<const BlockContext> context) : AnalogOutputBlock(context, Kind::Level), _ctx(std::move(context)) {}

	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void	  tick(Level& level, const BlockPos& pos, int state) const override;

	// ComposterBlock.COMPOSTABLES: the chance an item adds a layer, 0 if it can't be composted
	static float compostChance(const std::string& item);
	// ComposterBlock.insertItem: one item in (the composter at pos below 7); whether it added a layer
	static bool insert(Level& level, const BlockPos& pos, const std::string& item);
	static int	levelOf(Level& level, const BlockPos& pos);
	// ComposterBlock.getContainer (a WorldlyContainer for hoppers): one compostable item at a time from above below
	// level 7, the bone meal from below at level 8, nothing while it ripens
	static std::shared_ptr<Container> container(Level& level, const BlockPos& pos);

  private:
	std::shared_ptr<const BlockContext> _ctx;
};

#endif
