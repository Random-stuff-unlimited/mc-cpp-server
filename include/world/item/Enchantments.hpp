#ifndef ENCHANTMENTS_HPP
#define ENCHANTMENTS_HPP

#include "lib/json.hpp"
#include "world/entity/Geometry.hpp"
#include "world/item/ItemStack.hpp"

#include <string>
#include <utility>
#include <vector>

class Actor;
class GameData;
class JavaRandom;
class Level;
namespace Combat {
	struct DamageSource;
}

// Enchantments (vanilla's Enchantment and EnchantmentHelper): their effects are data (enchantments.json, as in the
// game's data), run here: value effects (add, multiply, set, remove_binomial, all_of) with level based values, under
// their requirements (loot conditions). Each effect component ("minecraft:damage", "minecraft:item_damage"...) is
// asked by the game code at the place vanilla asks it
namespace Enchantments {
	// ItemEnchantments of a stack (minecraft:enchantments): enchantment registry id and level, in the stack's order
	std::vector<std::pair<int, int>> of(const GameData& gameData, const ItemStack& stack);
	// EnchantmentHelper.getItemEnchantmentLevel
	int level(const GameData& gameData, const ItemStack& stack, const std::string& enchantment);
	// The definition of an enchantment by registry id, null if unknown
	const nlohmann::json* definition(const GameData& gameData, int enchantment);

	// What the requirements of an effect can ask about (LootContextParamSets.ENCHANTED_*)
	struct Context {
		Level&						level;
		const ItemStack*			tool	 = nullptr; // The enchanted item
		Actor*						entity	 = nullptr; // "this": the enchanted entity or the one affected
		const Combat::DamageSource* damage	 = nullptr;
		Actor*						attacker = nullptr, *directAttacker = nullptr;
		Vec3						origin;
		int							enchantmentLevel = 1;
	};

	// LevelBasedValue.calculate
	float levelValue(const nlohmann::json& value, int level);
	// EnchantmentValueEffect.process
	float applyValueEffect(const nlohmann::json& effect, float value, int level, JavaRandom& random);
	// The requirements (a LootItemCondition), true when absent
	bool conditionsPass(const nlohmann::json* requirements, Context& context);
	// Runs every value effect of this component of every enchantment of the item on `value`
	// (EnchantmentHelper.runIterationOnItem + Enchantment.modify...)
	float modifyItem(const ItemStack& stack, const std::string& component, float value, Context& context);

	// EnchantmentHelper.processDurabilityChange: unbreaking spares some of the damage
	int	  processDurabilityChange(Level& level, const ItemStack& stack, int amount);
	// EnchantmentHelper.modifyDamage: sharpness, smite, bane of arthropods, impaling
	float modifyDamage(Level& level, const ItemStack& weapon, Actor& target, const Combat::DamageSource& source, float damage);
	// EnchantmentHelper.modifyKnockback
	float modifyKnockback(Level& level, const ItemStack& weapon, Actor& target, const Combat::DamageSource& source, float knockback);
	// EnchantmentHelper.getDamageProtection: protection points of the armor worn (each piece counts)
	float damageProtection(Level& level, const std::vector<ItemStack>& armor, Actor& wearer, const Combat::DamageSource& source);
	// EnchantmentHelper.doPostAttackEffects: fire aspect sets the victim on fire...
	void  doPostAttackEffects(Level& level, const ItemStack& weapon, Actor& attacker, Actor& victim, const Combat::DamageSource& source);
	// Enchantment.getMinCost / getMaxCost of a definition
	int cost_min(const nlohmann::json& definition, int level);
	int cost_max(const nlohmann::json& definition, int level);
	// Writes minecraft:enchantments (registry id, level pairs); an empty list removes the component
	void set(const GameData& gameData, ItemStack& stack, const std::vector<std::pair<int, int>>& enchantments);
	// Enchantment.areCompatible: not the same one, neither in the other's exclusive set
	bool areCompatible(const GameData& gameData, int a, int b);
	// EnchantmentHelper.selectEnchantment: enchantments for an item at this cost (the enchanting table's roll), picked
	// among the candidates (registry ids, in their order)
	std::vector<std::pair<int, int>> selectEnchantment(const GameData& gameData, JavaRandom& random, const ItemStack& stack, int cost,
													   const std::vector<int>& candidates);
	// The enchantments of a tag of the enchantment registry ("minecraft:on_mob_spawn_equipment"), in the tag's order
	std::vector<int> tagEntries(const GameData& gameData, const std::string& tag);
	// EnchantmentHelper.enchantItemFromProvider: an enchantment provider of enchantments.json ("minecraft:mob_spawn_equipment")
	void enchantItemFromProvider(const GameData& gameData, ItemStack& stack, const std::string& provider, float specialMultiplier, JavaRandom& random);

	// EnchantmentHelper.processEquipmentDropChance (looting's bonus)
	float processEquipmentDropChance(Level& level, const ItemStack& weapon, Actor& victim, const Combat::DamageSource& source, float chance);
} // namespace Enchantments

#endif
