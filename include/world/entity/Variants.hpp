#ifndef VARIANTS_HPP
#define VARIANTS_HPP

#include "world/BlockPos.hpp"

#include <string>

class Level;

// Mob variants (vanilla's VariantUtils, PriorityProvider and SpawnConditions): cows, pigs and chickens are warm, cold
// or temperate by their biome; cats, frogs and wolves too. A variant is its id in its synced registry
// (minecraft:cow_variant...), as the entity data sends it
namespace Variants {
	// VariantUtils.selectVariantToSpawn: among the variants whose conditions hold here, those of the highest priority,
	// one at random (the level's random). -1 if none
	int			selectToSpawn(Level& level, const std::string& registry, const BlockPos& pos);
	// A variant's id by name ("minecraft:temperate"), -1 if unknown
	int			id(Level& level, const std::string& registry, const std::string& name);
	// Its name, "" if unknown
	std::string name(Level& level, const std::string& registry, int id);
} // namespace Variants

#endif
