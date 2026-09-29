#ifndef ITEM_USE_ON_HPP
#define ITEM_USE_ON_HPP

#include "world/BlockBehavior.hpp"
#include "world/item/ItemUse.hpp"

class Level;
class Player;

namespace ItemUse {
	// Item.useOn for the items that do something on a block other than placing one (flint and steel, fire charges,
	// ender eyes...), after the block's own use. Pass: the item's placement or Item.use comes next
	Result useOn(Level& level, Player& player, int hand, const BlockHit& hit);
} // namespace ItemUse

#endif
