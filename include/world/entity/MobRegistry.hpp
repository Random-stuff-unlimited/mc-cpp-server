#ifndef MOB_REGISTRY_HPP
#define MOB_REGISTRY_HPP

#include "world/BlockPos.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GameData;
class Level;
class Mob;

// How the mobs of each entity type are made, and the goals each type gets: the per-type registration point of the
// AI. By default every type whose class extends Mob is a plain Mob with its type's size, attributes and loot.
//
// Adding AI to a type, in registerVanillaMobs (src/world/entity/ai/MobGoals.cpp):
//   registry.addGoals("minecraft:cow", [](Mob& mob) {
//       mob.goalSelector().addGoal(0, std::make_unique<FloatGoal>(mob));
//       mob.goalSelector().addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(mob, 1.0));
//   });
// (the priorities and goals of vanilla's registerGoals of that class). A type that needs its own class (its own
// entity data, a Brain, other controls) gets a factory: registry.setFactory("minecraft:villager", ...).
// The goals of a class apply to its subclasses in vanilla: register them for each type of that class
// (MobRegistry::typesOf("AbstractCow") lists them).
class MobRegistry {
  public:
	using Factory		= std::function<std::unique_ptr<Mob>(Level&, int typeId)>;
	using GoalRegistrar = std::function<void(Mob&)>;
	// EntitySpawnReason, the ones used so far
	enum class SpawnReason { SpawnItemUse, Spawner, Command, Natural, Load };

	explicit MobRegistry(const GameData& gameData) : _gameData(gameData) {}

	void setFactory(const std::string& type, Factory factory);
	// Goals added after the factory made the mob (Mob.registerGoals), in the order registered
	void addGoals(const std::string& type, GoalRegistrar registrar);
	// Entity types whose Java class is or extends this one ("Zombie" gives zombie, husk, drowned...)
	std::vector<int> typesOf(const std::string& javaClass) const;

	// EntityType.create: the mob with its goals, not in the level yet. nullptr for types that aren't mobs
	std::unique_ptr<Mob> create(Level& level, int typeId) const;
	// EntityType.spawn: created at the block (centered, raised onto what is under it when shouldOffsetY), turned at
	// random, finalizeSpawn, then added to the level. nullptr if it can't be made
	Mob* spawn(Level& level, int typeId, const BlockPos& pos, SpawnReason reason, bool shouldOffsetY = false, bool shouldOffsetYMore = false) const;

  private:
	const GameData&								 _gameData;
	std::unordered_map<int, Factory>			 _factories;
	std::unordered_map<int, std::vector<GoalRegistrar>> _goals;

	int typeId(const std::string& type) const;
};

// The vanilla mobs' factories and goals (src/world/entity/ai/MobGoals.cpp)
void registerVanillaMobs(MobRegistry& registry);

#endif
