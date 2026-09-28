#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"

// The AI of each vanilla mob type, registered here once goals are ported (see MobRegistry for the pattern):
// vanilla's registerGoals of each class, with the same priorities and in the same order. For instance, Cow:
//   registry.addGoals("minecraft:cow", [](Mob& mob) {
//       mob.goalSelector().addGoal(0, std::make_unique<FloatGoal>(mob));
//       mob.goalSelector().addGoal(1, std::make_unique<PanicGoal>(mob, 2.0));
//       ...
//   });
// Brain-based mobs (villagers, piglins, axolotls, frogs...) get a factory making their own Mob subclass instead,
// which ticks its Brain in customServerAiStep.
// No goal is registered yet: every mob stands still, falls, takes damage and dies like vanilla's without its AI.
void registerVanillaMobs(MobRegistry& registry) { (void)registry; }
