#ifndef TRADING_HPP
#define TRADING_HPP

#include "world/item/ItemStack.hpp"

#include <optional>
#include <string>
#include <vector>

class Buffer;
class GameData;
class JavaRandom;
class Level;

// ItemCost: what a trade asks for (an item and a count; any components)
struct ItemCost {
	int item  = -1;
	int count = 0;

	bool isEmpty() const { return item < 0 || count <= 0; }
	bool test(const ItemStack& stack) const { return !isEmpty() && !stack.isEmpty() && stack.item == item; }
	void write(Buffer& buf) const;
};

// MerchantOffer: one trade, its use count, and what changes its price (the demand, a special price)
struct MerchantOffer {
	ItemCost				costA;
	std::optional<ItemCost> costB;
	ItemStack				result;
	int						uses = 0, maxUses = 12;
	bool					rewardExp		 = true;
	int						specialPriceDiff = 0, demand = 0;
	float					priceMultiplier	 = 0.05F;
	int						xp				 = 1;

	bool isOutOfStock() const { return uses >= maxUses; }
	// MerchantOffer.getCostA: the base count raised by the demand, plus the special price, between 1 and a stack
	int	 adjustedCostA(const GameData& gameData) const;
	// MerchantOffer.satisfiedBy: the two payment slots pay for it
	bool satisfiedBy(const ItemStack& a, const ItemStack& b, const GameData& gameData) const;
	// MerchantOffer.take: the payments are used up; false if they don't pay
	bool take(ItemStack& a, ItemStack& b, const GameData& gameData) const;
	// MerchantOffer.updateDemand, on restocking
	void updateDemand() { demand = demand + uses - (maxUses - uses); }
	void resetUses() { uses = 0; }
	void write(Buffer& buf, const GameData& gameData) const;
	void save(Buffer& buf) const;
	static MerchantOffer load(Buffer& buf);
};

// VillagerTrades: the offers of each profession by level, and the wandering trader's
namespace VillagerTrades {
	// Two new offers of a profession ("minecraft:farmer") at a level (1 to 5), not among those it has
	void addOffers(Level& level, JavaRandom& random, const std::string& profession, int villagerLevel, std::vector<MerchantOffer>& offers);
	// The wandering trader's: five common, one rare
	void addWanderingTraderOffers(Level& level, JavaRandom& random, std::vector<MerchantOffer>& offers);
	// VillagerData.NEXT_LEVEL_XP_THRESHOLDS: the experience a level needs (0, 10, 70, 150, 250)
	int	 xpForLevel(int villagerLevel);
} // namespace VillagerTrades

#endif
