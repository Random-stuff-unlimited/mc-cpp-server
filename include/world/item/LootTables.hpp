#ifndef LOOT_TABLES_HPP
#define LOOT_TABLES_HPP

#include "lib/json.hpp"
#include "world/BlockPos.hpp"
#include "world/item/ItemStack.hpp"

#include <filesystem>
#include <vector>

class GameData;
class JavaRandom;
class Level;

// What a block drops: its loot table from the game's data (block_loot_tables.json), evaluated like vanilla's
// LootTable (pools, rolls, weighted entries, alternatives, conditions, item functions).
// Enchantments aren't known yet: silk touch and fortune never apply
class LootTables {
  public:
	// What the table can ask about (vanilla's LootContextParams)
	struct Context {
		Level&			 level;
		BlockPos		 origin;
		int				 blockState;
		const ItemStack* tool			= nullptr; // Tool used, if any
		bool			 hasEntity		= false;   // "this" entity (the player breaking it)
		float			 explosionRadius = 0.0f;   // 0: not an explosion
		const class BlockEntity* blockEntity = nullptr; // The block's, for copy_components
	};

	void load(const std::filesystem::path& file, const GameData& gameData);
	std::vector<ItemStack> blockDrops(Context& context) const;

  private:
	const GameData*				_gameData = nullptr;
	std::vector<nlohmann::json> _byBlock; // By block id, null if it has no table

	bool  conditionsPass(const nlohmann::json& holder, Context& context) const;
	bool  condition(const nlohmann::json& condition, Context& context) const;
	bool  toolMatches(const nlohmann::json& predicate, Context& context) const;
	// LootPoolEntryContainer.expand: adds the entries that apply. Returns whether this entry applied
	bool  expand(const nlohmann::json& entry, Context& context, std::vector<const nlohmann::json*>& out) const;
	void  createItems(const nlohmann::json& entry, const nlohmann::json& pool, Context& context, std::vector<ItemStack>& out) const;
	void  applyFunction(const nlohmann::json& function, ItemStack& stack, Context& context) const;
	int	  numberInt(const nlohmann::json& number, Context& context) const;
	float numberFloat(const nlohmann::json& number, Context& context) const;
};

#endif
