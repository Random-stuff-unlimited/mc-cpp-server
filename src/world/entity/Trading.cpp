#include "world/entity/Trading.hpp"

#include "lib/JavaRandom.hpp"
#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/item/Components.hpp"
#include "world/item/Enchantments.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace {
	int maxStackOf(const GameData& gameData, int item) {
		const GameData::ItemProperties* props = gameData.getItemProperties(item);
		return props ? props->maxStackSize : 64;
	}

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

	const char* const COLORS[] = {"white", "orange", "magenta", "light_blue", "yellow", "lime",	"pink", "gray",
								  "light_gray", "cyan", "purple", "blue",	   "brown",	 "green", "red",  "black"};
	const char* const ARROW_POTIONS[] = {"minecraft:swiftness",	 "minecraft:slowness",		 "minecraft:strength",	 "minecraft:poison",
										 "minecraft:healing",	 "minecraft:harming",		 "minecraft:regeneration", "minecraft:fire_resistance",
										 "minecraft:night_vision", "minecraft:invisibility", "minecraft:water_breathing", "minecraft:leaping",
										 "minecraft:weakness",	 "minecraft:slow_falling",	 "minecraft:turtle_master"};

	// VillagerTrades' ItemListings, as data:
	//   'E' EmeraldForItems(item, cost a, maxUses, xp): a of the item for an emerald
	//   'I' ItemsForEmeralds(item, emeralds a, count b, maxUses, xp, multiplier)
	//   'X' ItemsAndEmeraldsToItems(item, count a, emeralds b, result item2 x c, maxUses, xp)
	//   'N' EnchantedItemForEmeralds(item, base emeralds a, maxUses, xp, multiplier)
	//   'B' EnchantBookForEmeralds(xp)
	//   'T' TippedArrowForItemsAndEmeralds (5 arrows and 2 emeralds for 5 tipped arrows)
	// "{color}" in an item name: a color drawn at random (vanilla lists one listing per color)
	struct Listing {
		char		kind;
		const char* item;
		int			a = 0, b = 0, maxUses = 12, xp = 1;
		float		mult  = 0.05F;
		const char* item2 = nullptr;
		int			c	  = 0;
	};
	using Levels = std::vector<std::vector<Listing>>;

	const std::map<std::string, Levels>& professionTrades() {
		static const std::map<std::string, Levels> TRADES = {
				{"minecraft:farmer",
				 {{{'E', "wheat", 20, 0, 16, 2}, {'E', "potato", 26, 0, 16, 2}, {'E', "carrot", 22, 0, 16, 2}, {'E', "beetroot", 15, 0, 16, 2},
				   {'I', "bread", 1, 6, 16, 1}},
				  {{'E', "pumpkin", 6, 0, 12, 10}, {'I', "pumpkin_pie", 1, 4, 12, 5}, {'I', "apple", 1, 4, 16, 5}},
				  {{'I', "cookie", 3, 18, 12, 10}, {'E', "melon", 4, 0, 12, 20}},
				  {{'I', "cake", 1, 1, 12, 15}, {'I', "suspicious_stew", 1, 1, 12, 15}},
				  {{'I', "golden_carrot", 3, 3, 12, 30}, {'I', "glistering_melon_slice", 4, 3, 12, 30}}}},
				{"minecraft:fisherman",
				 {{{'E', "string", 20, 0, 16, 2}, {'E', "coal", 10, 0, 16, 2}, {'X', "cod", 6, 1, 16, 1, 0.05F, "cooked_cod", 6},
				   {'I', "cod_bucket", 3, 1, 16, 1}},
				  {{'E', "cod", 15, 0, 16, 10}, {'X', "salmon", 6, 1, 16, 5, 0.05F, "cooked_salmon", 6}, {'I', "campfire", 2, 1, 12, 5}},
				  {{'E', "salmon", 13, 0, 16, 20}, {'N', "fishing_rod", 3, 0, 3, 10, 0.2F}},
				  {{'E', "tropical_fish", 6, 0, 12, 30}},
				  {{'E', "pufferfish", 4, 0, 12, 30}, {'E', "oak_boat", 1, 0, 12, 30}}}},
				{"minecraft:shepherd",
				 {{{'E', "white_wool", 18, 0, 16, 2}, {'E', "brown_wool", 18, 0, 16, 2}, {'E', "black_wool", 18, 0, 16, 2}, {'E', "gray_wool", 18, 0, 16, 2},
				   {'I', "shears", 2, 1, 12, 1}},
				  {{'E', "white_dye", 12, 0, 16, 10}, {'E', "gray_dye", 12, 0, 16, 10}, {'E', "black_dye", 12, 0, 16, 10}, {'E', "light_blue_dye", 12, 0, 16, 10},
				   {'E', "lime_dye", 12, 0, 16, 10}, {'I', "{color}_wool", 1, 1, 16, 5}, {'I', "{color}_carpet", 1, 4, 16, 5}},
				  {{'E', "yellow_dye", 12, 0, 16, 20}, {'E', "light_gray_dye", 12, 0, 16, 20}, {'E', "orange_dye", 12, 0, 16, 20}, {'E', "red_dye", 12, 0, 16, 20},
				   {'E', "pink_dye", 12, 0, 16, 20}, {'I', "{color}_bed", 3, 1, 12, 10}},
				  {{'E', "brown_dye", 12, 0, 16, 30}, {'E', "purple_dye", 12, 0, 16, 30}, {'E', "blue_dye", 12, 0, 16, 30}, {'E', "green_dye", 12, 0, 16, 30},
				   {'E', "magenta_dye", 12, 0, 16, 30}, {'E', "cyan_dye", 12, 0, 16, 30}, {'I', "{color}_banner", 3, 1, 12, 15}},
				  {{'I', "painting", 2, 3, 12, 30}}}},
				{"minecraft:fletcher",
				 {{{'E', "stick", 32, 0, 16, 2}, {'I', "arrow", 1, 16, 12, 1}, {'X', "gravel", 10, 1, 12, 1, 0.05F, "flint", 10}},
				  {{'E', "flint", 26, 0, 12, 10}, {'I', "bow", 2, 1, 12, 5}},
				  {{'E', "string", 14, 0, 16, 20}, {'I', "crossbow", 3, 1, 12, 10}},
				  {{'E', "feather", 24, 0, 16, 30}, {'N', "bow", 2, 0, 3, 15}},
				  {{'E', "tripwire_hook", 8, 0, 12, 30}, {'N', "crossbow", 3, 0, 3, 15}, {'T', "arrow", 0, 0, 12, 30}}}},
				{"minecraft:librarian",
				 {{{'E', "paper", 24, 0, 16, 2}, {'B', "book", 0, 0, 12, 1}, {'I', "bookshelf", 9, 1, 12, 1}},
				  {{'E', "book", 4, 0, 12, 10}, {'B', "book", 0, 0, 12, 5}, {'I', "lantern", 1, 1, 12, 5}},
				  {{'E', "ink_sac", 5, 0, 12, 20}, {'B', "book", 0, 0, 12, 10}, {'I', "glass", 1, 4, 12, 10}},
				  {{'E', "writable_book", 2, 0, 12, 30}, {'B', "book", 0, 0, 12, 15}, {'I', "clock", 5, 1, 12, 15}, {'I', "compass", 4, 1, 12, 15}},
				  {{'I', "name_tag", 20, 1, 12, 30}}}},
				{"minecraft:cartographer",
				 {{{'E', "paper", 24, 0, 16, 2}, {'I', "map", 7, 1, 12, 1}},
				  {{'E', "glass_pane", 11, 0, 16, 10}},
				  {{'E', "compass", 1, 0, 12, 20}},
				  {{'I', "item_frame", 7, 1, 12, 15}, {'I', "{color}_banner", 3, 1, 12, 15}},
				  {{'I', "globe_banner_pattern", 8, 1, 12, 30}}}},
				{"minecraft:cleric",
				 {{{'E', "rotten_flesh", 32, 0, 16, 2}, {'I', "redstone", 1, 2, 12, 1}},
				  {{'E', "gold_ingot", 3, 0, 12, 10}, {'I', "lapis_lazuli", 1, 1, 12, 5}},
				  {{'E', "rabbit_foot", 2, 0, 12, 20}, {'I', "glowstone", 4, 1, 12, 10}},
				  {{'E', "turtle_scute", 4, 0, 12, 30}, {'E', "glass_bottle", 9, 0, 12, 30}, {'I', "ender_pearl", 5, 1, 12, 15}},
				  {{'E', "nether_wart", 22, 0, 12, 30}, {'I', "experience_bottle", 3, 1, 12, 30}}}},
				{"minecraft:armorer",
				 {{{'E', "coal", 15, 0, 16, 2}, {'I', "iron_leggings", 7, 1, 12, 1, 0.2F}, {'I', "iron_boots", 4, 1, 12, 1, 0.2F},
				   {'I', "iron_helmet", 5, 1, 12, 1, 0.2F}, {'I', "iron_chestplate", 9, 1, 12, 1, 0.2F}},
				  {{'E', "iron_ingot", 4, 0, 12, 10}, {'I', "bell", 36, 1, 12, 5, 0.2F}, {'I', "chainmail_boots", 1, 1, 12, 5, 0.2F},
				   {'I', "chainmail_leggings", 3, 1, 12, 5, 0.2F}},
				  {{'E', "lava_bucket", 1, 0, 12, 20}, {'E', "diamond", 1, 0, 12, 20}, {'I', "chainmail_helmet", 1, 1, 12, 10, 0.2F},
				   {'I', "chainmail_chestplate", 4, 1, 12, 10, 0.2F}, {'I', "shield", 5, 1, 12, 10, 0.2F}},
				  {{'N', "diamond_leggings", 14, 0, 3, 15, 0.2F}, {'N', "diamond_boots", 8, 0, 3, 15, 0.2F}},
				  {{'N', "diamond_helmet", 8, 0, 3, 30, 0.2F}, {'N', "diamond_chestplate", 16, 0, 3, 30, 0.2F}}}},
				{"minecraft:weaponsmith",
				 {{{'E', "coal", 15, 0, 16, 2}, {'I', "iron_axe", 3, 1, 12, 1, 0.2F}, {'N', "iron_sword", 2, 0, 3, 1}},
				  {{'E', "iron_ingot", 4, 0, 12, 10}, {'I', "bell", 36, 1, 12, 5, 0.2F}},
				  {{'E', "flint", 24, 0, 12, 20}},
				  {{'E', "diamond", 1, 0, 12, 30}, {'N', "diamond_axe", 12, 0, 3, 15, 0.2F}},
				  {{'N', "diamond_sword", 8, 0, 3, 30, 0.2F}}}},
				{"minecraft:toolsmith",
				 {{{'E', "coal", 15, 0, 16, 2}, {'I', "stone_axe", 1, 1, 12, 1, 0.2F}, {'I', "stone_shovel", 1, 1, 12, 1, 0.2F},
				   {'I', "stone_pickaxe", 1, 1, 12, 1, 0.2F}, {'I', "stone_hoe", 1, 1, 12, 1, 0.2F}},
				  {{'E', "iron_ingot", 4, 0, 12, 10}, {'I', "bell", 36, 1, 12, 5, 0.2F}},
				  {{'E', "flint", 30, 0, 12, 20}, {'N', "iron_axe", 1, 0, 3, 10, 0.2F}, {'N', "iron_shovel", 2, 0, 3, 10, 0.2F},
				   {'N', "iron_pickaxe", 3, 0, 3, 10, 0.2F}, {'I', "diamond_hoe", 4, 1, 3, 10, 0.2F}},
				  {{'E', "diamond", 1, 0, 12, 30}, {'N', "diamond_axe", 12, 0, 3, 15, 0.2F}, {'N', "diamond_shovel", 5, 0, 3, 15, 0.2F}},
				  {{'N', "diamond_pickaxe", 13, 0, 3, 30, 0.2F}}}},
				{"minecraft:butcher",
				 {{{'E', "chicken", 14, 0, 16, 2}, {'E', "porkchop", 7, 0, 16, 2}, {'E', "rabbit", 4, 0, 16, 2}, {'I', "rabbit_stew", 1, 1, 12, 1}},
				  {{'E', "coal", 15, 0, 16, 2}, {'I', "cooked_porkchop", 1, 5, 16, 5}, {'I', "cooked_chicken", 1, 8, 16, 5}},
				  {{'E', "mutton", 7, 0, 16, 20}, {'E', "beef", 10, 0, 16, 20}},
				  {{'E', "dried_kelp_block", 10, 0, 12, 30}},
				  {{'E', "sweet_berries", 10, 0, 12, 30}}}},
				{"minecraft:leatherworker",
				 {{{'E', "leather", 6, 0, 16, 2}, {'I', "leather_leggings", 3, 1, 12, 1, 0.2F}, {'I', "leather_chestplate", 7, 1, 12, 1, 0.2F}},
				  {{'E', "flint", 26, 0, 12, 10}, {'I', "leather_helmet", 5, 1, 12, 5, 0.2F}, {'I', "leather_boots", 4, 1, 12, 5, 0.2F}},
				  {{'E', "rabbit_hide", 9, 0, 12, 20}, {'I', "leather_chestplate", 7, 1, 12, 10, 0.2F}},
				  {{'E', "turtle_scute", 4, 0, 12, 30}, {'I', "leather_horse_armor", 6, 1, 12, 15, 0.2F}},
				  {{'I', "saddle", 6, 1, 12, 30, 0.2F}, {'I', "leather_helmet", 5, 1, 12, 30, 0.2F}}}},
				{"minecraft:mason",
				 {{{'E', "clay_ball", 10, 0, 16, 2}, {'I', "brick", 1, 10, 16, 1}},
				  {{'E', "stone", 20, 0, 16, 10}, {'I', "chiseled_stone_bricks", 1, 4, 16, 5}},
				  {{'E', "granite", 16, 0, 16, 20}, {'E', "andesite", 16, 0, 16, 20}, {'E', "diorite", 16, 0, 16, 20}, {'I', "dripstone_block", 1, 4, 16, 10},
				   {'I', "polished_andesite", 1, 4, 16, 10}, {'I', "polished_diorite", 1, 4, 16, 10}, {'I', "polished_granite", 1, 4, 16, 10}},
				  {{'E', "quartz", 12, 0, 12, 30}, {'I', "{color}_terracotta", 1, 1, 12, 15}, {'I', "{color}_glazed_terracotta", 1, 1, 12, 15}},
				  {{'I', "quartz_pillar", 1, 1, 12, 30}, {'I', "quartz_block", 1, 1, 12, 30}}}},
		};
		return TRADES;
	}

	// WanderingTrader's: the common ones (five of them) and the rare ones (one)
	const std::vector<Listing>& wanderingCommon() {
		static const std::vector<Listing> LISTINGS = {
				{'I', "sea_pickle", 2, 1, 5}, {'I', "slime_ball", 4, 1, 5}, {'I', "glowstone", 2, 1, 5}, {'I', "nautilus_shell", 5, 1, 5},
				{'I', "fern", 1, 1, 12}, {'I', "sugar_cane", 1, 1, 8}, {'I', "pumpkin", 1, 1, 4}, {'I', "kelp", 3, 1, 12}, {'I', "cactus", 3, 1, 8},
				{'I', "dandelion", 1, 1, 12}, {'I', "poppy", 1, 1, 12}, {'I', "blue_orchid", 1, 1, 8}, {'I', "allium", 1, 1, 12},
				{'I', "azure_bluet", 1, 1, 12}, {'I', "red_tulip", 1, 1, 12}, {'I', "orange_tulip", 1, 1, 12}, {'I', "white_tulip", 1, 1, 12},
				{'I', "pink_tulip", 1, 1, 12}, {'I', "oxeye_daisy", 1, 1, 12}, {'I', "cornflower", 1, 1, 12}, {'I', "lily_of_the_valley", 1, 1, 7},
				{'I', "wheat_seeds", 1, 1, 12}, {'I', "beetroot_seeds", 1, 1, 12}, {'I', "pumpkin_seeds", 1, 1, 12}, {'I', "melon_seeds", 1, 1, 12},
				{'I', "acacia_sapling", 5, 1, 8}, {'I', "birch_sapling", 5, 1, 8}, {'I', "dark_oak_sapling", 5, 1, 8}, {'I', "jungle_sapling", 5, 1, 8},
				{'I', "oak_sapling", 5, 1, 8}, {'I', "spruce_sapling", 5, 1, 8}, {'I', "cherry_sapling", 5, 1, 8}, {'I', "pale_oak_sapling", 5, 1, 8},
				{'I', "mangrove_propagule", 5, 1, 8}, {'I', "{color}_dye", 1, 3, 12}, {'I', "brain_coral_block", 3, 1, 8},
				{'I', "bubble_coral_block", 3, 1, 8}, {'I', "fire_coral_block", 3, 1, 8}, {'I', "horn_coral_block", 3, 1, 8},
				{'I', "tube_coral_block", 3, 1, 8}, {'I', "vine", 1, 1, 12}, {'I', "brown_mushroom", 1, 1, 12}, {'I', "red_mushroom", 1, 1, 12},
				{'I', "lily_pad", 1, 2, 5}, {'I', "small_dripleaf", 1, 2, 5}, {'I', "sand", 1, 8, 8}, {'I', "red_sand", 1, 4, 6},
				{'I', "pointed_dripstone", 1, 2, 5}, {'I', "rooted_dirt", 1, 2, 5}, {'I', "moss_block", 1, 2, 5}};
		return LISTINGS;
	}
	const std::vector<Listing>& wanderingRare() {
		static const std::vector<Listing> LISTINGS = {{'I', "tropical_fish_bucket", 5, 1, 4}, {'I', "pufferfish_bucket", 5, 1, 4},
													  {'I', "packed_ice", 3, 1, 6},			 {'I', "blue_ice", 6, 1, 6},
													  {'I', "gunpowder", 1, 1, 8},			 {'I', "podzol", 3, 3, 6}};
		return LISTINGS;
	}

	std::string itemName(const char* name, JavaRandom& random) {
		std::string item = name;
		size_t		at	 = item.find("{color}");
		if (at != std::string::npos) item.replace(at, 7, COLORS[random.nextInt(16)]);
		return "minecraft:" + item;
	}

	int itemId(Level& level, const std::string& name) { return level.gameData().getStaticId("minecraft:item", name); }

	// ItemListing.getOffer
	std::optional<MerchantOffer> makeOffer(Level& level, JavaRandom& random, const Listing& listing) {
		const GameData& data	= level.gameData();
		int				emerald = itemId(level, "minecraft:emerald");
		MerchantOffer	offer;
		offer.maxUses		  = listing.maxUses;
		offer.xp			  = listing.xp;
		offer.priceMultiplier = listing.mult;
		std::string item	  = itemName(listing.item, random);
		int			id		  = itemId(level, item);
		if (id < 0) return std::nullopt;
		switch (listing.kind) {
			case 'E':
				offer.costA	 = {id, listing.a};
				offer.result = ItemStack(emerald, 1);
				break;
			case 'I':
				offer.costA	 = {emerald, listing.a};
				offer.result = ItemStack(id, listing.b);
				break;
			case 'X': {
				int result = itemId(level, "minecraft:" + std::string(listing.item2));
				if (result < 0) return std::nullopt;
				offer.costA	 = {id, listing.a};
				offer.costB	 = ItemCost{emerald, listing.b};
				offer.result = ItemStack(result, listing.c);
				break;
			}
			case 'N': {
				// EnchantedItemForEmeralds: enchanted at a level of 5 to 19 (#on_traded_equipment), priced with it
				int		  enchantLevel = 5 + random.nextInt(15);
				ItemStack stack(id, 1);
				std::vector<std::pair<int, int>> enchantments =
						Enchantments::selectEnchantment(data, random, stack, enchantLevel, Enchantments::tagEntries(data, "minecraft:on_traded_equipment"));
				Enchantments::set(data, stack, enchantments);
				offer.costA	 = {emerald, std::min(listing.a + enchantLevel, 64)};
				offer.result = std::move(stack);
				break;
			}
			case 'B': {
				// EnchantBookForEmeralds: one #tradeable enchantment at a random level, its price by level (doubled for the
				// #double_trade_price ones)
				std::vector<int> tradeable = Enchantments::tagEntries(data, "minecraft:tradeable");
				if (tradeable.empty()) return std::nullopt;
				int					  enchantment = tradeable[random.nextInt(static_cast<int>(tradeable.size()))];
				const nlohmann::json* definition  = Enchantments::definition(data, enchantment);
				int					  maxLevel	  = definition ? definition->value("max_level", 1) : 1;
				int					  bookLevel	  = Mth::nextInt(random, 1, maxLevel);
				int					  cost		  = 2 + random.nextInt(5 + bookLevel * 10) + 3 * bookLevel;
				if (data.isInTag("minecraft:enchantment", "minecraft:double_trade_price", enchantment)) cost *= 2;
				ItemStack book(itemId(level, "minecraft:enchanted_book"), 1);
				Enchantments::set(data, book, {{enchantment, bookLevel}});
				offer.costA			  = {emerald, std::min(cost, 64)};
				offer.costB			  = ItemCost{itemId(level, "minecraft:book"), 1};
				offer.result		  = std::move(book);
				offer.priceMultiplier = 0.2F;
				break;
			}
			case 'T': {
				// TippedArrowForItemsAndEmeralds: 5 arrows and 2 emeralds for 5 arrows of a random potion
				ItemStack arrows(itemId(level, "minecraft:tipped_arrow"), 5);
				const char* potion = ARROW_POTIONS[random.nextInt(static_cast<int>(sizeof(ARROW_POTIONS) / sizeof(*ARROW_POTIONS)))];
				Components::set(arrows, data, "minecraft:potion_contents", Components::encodePotion(data.getStaticId("minecraft:potion", potion)));
				offer.costA	 = {emerald, 2};
				offer.costB	 = ItemCost{id, 5};
				offer.result = std::move(arrows);
				break;
			}
			default: return std::nullopt;
		}
		return offer;
	}

	// AbstractVillager.addOffersFromItemListings: `count` of the listings, at random, none twice
	void addFromListings(Level& level, JavaRandom& random, const std::vector<Listing>& listings, int count, std::vector<MerchantOffer>& offers) {
		std::vector<int> indexes;
		for (int i = 0; i < static_cast<int>(listings.size()); i++) indexes.push_back(i);
		for (int added = 0; added < count && !indexes.empty();) {
			int pick = random.nextInt(static_cast<int>(indexes.size()));
			int index = indexes[pick];
			indexes.erase(indexes.begin() + pick);
			if (std::optional<MerchantOffer> offer = makeOffer(level, random, listings[index])) {
				offers.push_back(std::move(*offer));
				added++;
			}
		}
	}
} // namespace

// ===================== ItemCost and MerchantOffer =====================

void ItemCost::write(Buffer& buf) const {
	buf.writeVarInt(item);
	buf.writeVarInt(count);
	buf.writeVarInt(0); // DataComponentExactPredicate: none
}

int MerchantOffer::adjustedCostA(const GameData& gameData) const {
	if (costA.isEmpty()) return 0;
	int base  = costA.count;
	int extra = std::max(0, static_cast<int>(std::floor(base * demand * priceMultiplier)));
	return std::max(1, std::min(base + extra + specialPriceDiff, maxStackOf(gameData, costA.item)));
}

bool MerchantOffer::satisfiedBy(const ItemStack& a, const ItemStack& b, const GameData& gameData) const {
	if (!costA.test(a) || a.count < adjustedCostA(gameData)) return false;
	if (!costB) return b.isEmpty();
	return costB->test(b) && b.count >= costB->count;
}

bool MerchantOffer::take(ItemStack& a, ItemStack& b, const GameData& gameData) const {
	if (!satisfiedBy(a, b, gameData)) return false;
	a.shrink(adjustedCostA(gameData));
	if (a.count <= 0) a = ItemStack{};
	if (costB && !b.isEmpty()) {
		b.shrink(costB->count);
		if (b.count <= 0) b = ItemStack{};
	}
	return true;
}

void MerchantOffer::write(Buffer& buf, const GameData& gameData) const {
	// MerchantOffer.STREAM_CODEC: the base cost (the client applies the price changes), result, second cost, stock
	costA.write(buf);
	result.write(buf);
	buf.writeBool(costB.has_value());
	if (costB) costB->write(buf);
	buf.writeBool(isOutOfStock());
	buf.writeInt(uses);
	buf.writeInt(maxUses);
	buf.writeInt(xp);
	buf.writeInt(specialPriceDiff);
	buf.writeFloat(priceMultiplier);
	buf.writeInt(demand);
	(void)gameData;
}

void MerchantOffer::save(Buffer& buf) const {
	buf.writeVarInt(costA.item);
	buf.writeVarInt(costA.count);
	buf.writeBool(costB.has_value());
	if (costB) {
		buf.writeVarInt(costB->item);
		buf.writeVarInt(costB->count);
	}
	writeStack(buf, result);
	buf.writeInt(uses);
	buf.writeInt(maxUses);
	buf.writeBool(rewardExp);
	buf.writeInt(specialPriceDiff);
	buf.writeInt(demand);
	buf.writeFloat(priceMultiplier);
	buf.writeInt(xp);
}

MerchantOffer MerchantOffer::load(Buffer& buf) {
	MerchantOffer offer;
	offer.costA.item  = buf.readVarInt();
	offer.costA.count = buf.readVarInt();
	if (buf.readBool()) {
		ItemCost b;
		b.item		= buf.readVarInt();
		b.count		= buf.readVarInt();
		offer.costB = b;
	}
	offer.result		   = readStack(buf);
	offer.uses			   = buf.readInt();
	offer.maxUses		   = buf.readInt();
	offer.rewardExp		   = buf.readBool();
	offer.specialPriceDiff = buf.readInt();
	offer.demand		   = buf.readInt();
	offer.priceMultiplier  = buf.readFloat();
	offer.xp			   = buf.readInt();
	return offer;
}

// ===================== VillagerTrades =====================

namespace VillagerTrades {
	void addOffers(Level& level, JavaRandom& random, const std::string& profession, int villagerLevel, std::vector<MerchantOffer>& offers) {
		auto it = professionTrades().find(profession);
		if (it == professionTrades().end() || villagerLevel < 1 || villagerLevel > static_cast<int>(it->second.size())) return;
		addFromListings(level, random, it->second[villagerLevel - 1], 2, offers);
	}

	void addWanderingTraderOffers(Level& level, JavaRandom& random, std::vector<MerchantOffer>& offers) {
		addFromListings(level, random, wanderingCommon(), 5, offers);
		addFromListings(level, random, wanderingRare(), 1, offers);
	}

	int xpForLevel(int villagerLevel) {
		static const int THRESHOLDS[] = {0, 10, 70, 150, 250};
		return villagerLevel >= 1 && villagerLevel <= 5 ? THRESHOLDS[villagerLevel - 1] : 250;
	}
} // namespace VillagerTrades
