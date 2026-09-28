#include "world/item/PotionBrewing.hpp"

#include "data/GameData.hpp"
#include "world/item/Components.hpp"

#include <algorithm>
#include <string>

PotionBrewing::PotionBrewing(const GameData& gameData) : _gameData(gameData) {
	auto item	= [&](const char* name) { return gameData.getStaticId("minecraft:item", std::string("minecraft:") + name); };
	auto potion = [&](const char* name) { return gameData.getStaticId("minecraft:potion", std::string("minecraft:") + name); };
	// PotionBrewing.Builder (every feature is enabled: nothing is left out)
	auto addContainer		= [&](const char* bottle) { _containers.push_back(item(bottle)); };
	auto addContainerRecipe = [&](const char* from, const char* ingredient, const char* to) {
		_containerMixes.push_back({item(from), item(ingredient), item(to)});
	};
	auto addMix = [&](const char* from, const char* ingredient, const char* to) {
		int f = potion(from), i = item(ingredient), t = potion(to);
		if (f >= 0 && i > 0 && t >= 0) _potionMixes.push_back({f, i, t});
	};
	auto addStartMix = [&](const char* ingredient, const char* to) {
		addMix("water", ingredient, "mundane");
		addMix("awkward", ingredient, to);
	};

	// PotionBrewing.addVanillaMixes
	addContainer("potion");
	addContainer("splash_potion");
	addContainer("lingering_potion");
	addContainerRecipe("potion", "gunpowder", "splash_potion");
	addContainerRecipe("splash_potion", "dragon_breath", "lingering_potion");
	addMix("water", "glowstone_dust", "thick");
	addMix("water", "redstone", "mundane");
	addMix("water", "nether_wart", "awkward");
	addStartMix("breeze_rod", "wind_charged");
	addStartMix("slime_block", "oozing");
	addStartMix("stone", "infested");
	addStartMix("cobweb", "weaving");
	addMix("awkward", "golden_carrot", "night_vision");
	addMix("night_vision", "redstone", "long_night_vision");
	addMix("night_vision", "fermented_spider_eye", "invisibility");
	addMix("long_night_vision", "fermented_spider_eye", "long_invisibility");
	addMix("invisibility", "redstone", "long_invisibility");
	addStartMix("magma_cream", "fire_resistance");
	addMix("fire_resistance", "redstone", "long_fire_resistance");
	addStartMix("rabbit_foot", "leaping");
	addMix("leaping", "redstone", "long_leaping");
	addMix("leaping", "glowstone_dust", "strong_leaping");
	addMix("leaping", "fermented_spider_eye", "slowness");
	addMix("long_leaping", "fermented_spider_eye", "long_slowness");
	addMix("slowness", "redstone", "long_slowness");
	addMix("slowness", "glowstone_dust", "strong_slowness");
	addMix("awkward", "turtle_helmet", "turtle_master");
	addMix("turtle_master", "redstone", "long_turtle_master");
	addMix("turtle_master", "glowstone_dust", "strong_turtle_master");
	addMix("swiftness", "fermented_spider_eye", "slowness");
	addMix("long_swiftness", "fermented_spider_eye", "long_slowness");
	addStartMix("sugar", "swiftness");
	addMix("swiftness", "redstone", "long_swiftness");
	addMix("swiftness", "glowstone_dust", "strong_swiftness");
	addMix("awkward", "pufferfish", "water_breathing");
	addMix("water_breathing", "redstone", "long_water_breathing");
	addStartMix("glistering_melon_slice", "healing");
	addMix("healing", "glowstone_dust", "strong_healing");
	addMix("healing", "fermented_spider_eye", "harming");
	addMix("strong_healing", "fermented_spider_eye", "strong_harming");
	addMix("harming", "glowstone_dust", "strong_harming");
	addMix("poison", "fermented_spider_eye", "harming");
	addMix("long_poison", "fermented_spider_eye", "harming");
	addMix("strong_poison", "fermented_spider_eye", "strong_harming");
	addStartMix("spider_eye", "poison");
	addMix("poison", "redstone", "long_poison");
	addMix("poison", "glowstone_dust", "strong_poison");
	addStartMix("ghast_tear", "regeneration");
	addMix("regeneration", "redstone", "long_regeneration");
	addMix("regeneration", "glowstone_dust", "strong_regeneration");
	addStartMix("blaze_powder", "strength");
	addMix("strength", "redstone", "long_strength");
	addMix("strength", "glowstone_dust", "strong_strength");
	addMix("water", "fermented_spider_eye", "weakness");
	addMix("weakness", "redstone", "long_weakness");
	addMix("awkward", "phantom_membrane", "slow_falling");
	addMix("slow_falling", "redstone", "long_slow_falling");
}

int PotionBrewing::potionOf(const ItemStack& stack) const {
	if (stack.isEmpty() || stack.components.empty()) return -1;
	std::optional<std::vector<uint8_t>> contents = Components::get(stack, _gameData, "minecraft:potion_contents");
	return contents ? Components::decodePotion(*contents) : -1;
}

ItemStack PotionBrewing::createItemStack(int item, int potion) const {
	ItemStack stack(item, 1);
	Components::set(stack, _gameData, "minecraft:potion_contents", Components::encodePotion(potion));
	return stack;
}

bool PotionBrewing::isContainerIngredient(const ItemStack& stack) const {
	return std::any_of(_containerMixes.begin(), _containerMixes.end(), [&](const Mix& mix) { return !stack.isEmpty() && stack.item == mix.ingredient; });
}

bool PotionBrewing::isPotionIngredient(const ItemStack& stack) const {
	return std::any_of(_potionMixes.begin(), _potionMixes.end(), [&](const Mix& mix) { return !stack.isEmpty() && stack.item == mix.ingredient; });
}

bool PotionBrewing::isBrewablePotion(int potion) const {
	return std::any_of(_potionMixes.begin(), _potionMixes.end(), [&](const Mix& mix) { return mix.to == potion; });
}

bool PotionBrewing::hasMix(const ItemStack& bottle, const ItemStack& ingredient) const {
	// isContainer
	if (bottle.isEmpty() || std::find(_containers.begin(), _containers.end(), bottle.item) == _containers.end()) return false;
	return hasContainerMix(bottle, ingredient) || hasPotionMix(bottle, ingredient);
}

bool PotionBrewing::hasContainerMix(const ItemStack& bottle, const ItemStack& ingredient) const {
	for (const Mix& mix : _containerMixes) {
		if (bottle.item == mix.from && !ingredient.isEmpty() && ingredient.item == mix.ingredient) return true;
	}
	return false;
}

bool PotionBrewing::hasPotionMix(const ItemStack& bottle, const ItemStack& ingredient) const {
	int potion = potionOf(bottle);
	if (potion < 0) return false;
	for (const Mix& mix : _potionMixes) {
		if (mix.from == potion && !ingredient.isEmpty() && ingredient.item == mix.ingredient) return true;
	}
	return false;
}

ItemStack PotionBrewing::mix(const ItemStack& ingredient, const ItemStack& bottle) const {
	if (bottle.isEmpty()) return bottle;
	int potion = potionOf(bottle);
	if (potion < 0) return bottle;
	for (const Mix& mix : _containerMixes) {
		if (bottle.item == mix.from && !ingredient.isEmpty() && ingredient.item == mix.ingredient) return createItemStack(mix.to, potion);
	}
	for (const Mix& mix : _potionMixes) {
		if (mix.from == potion && !ingredient.isEmpty() && ingredient.item == mix.ingredient) return createItemStack(bottle.item, mix.to);
	}
	return bottle;
}
