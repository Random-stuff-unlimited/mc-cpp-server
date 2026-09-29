#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/mobs/Creeper.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/entity/mobs/Skeleton.hpp"
#include "world/entity/mobs/Spider.hpp"
#include "world/entity/mobs/Zombie.hpp"

// The vanilla mob classes (src/world/entity/mobs): each type gets its class, which registers its own goals
// (registerGoals, with vanilla's priorities). Types without a class of their own are plain Mobs (Monsters for the
// hostile ones, see MobRegistry::create) without goals: they stand still, fall, take damage and die like vanilla's.
// Brain-based mobs (villagers, piglins, axolotls, frogs...) will tick their Brain in customServerAiStep.
namespace {
	template <typename T> MobRegistry::Factory factory() {
		return [](Level& level, int typeId) -> std::unique_ptr<Mob> { return std::make_unique<T>(level, typeId); };
	}
} // namespace

void registerVanillaMobs(MobRegistry& registry) {
	registry.setFactory("minecraft:zombie", factory<Zombie>());
	registry.setFactory("minecraft:husk", factory<Husk>());
	registry.setFactory("minecraft:creeper", factory<Creeper>());
	registry.setFactory("minecraft:spider", factory<Spider>());
	registry.setFactory("minecraft:cave_spider", factory<CaveSpider>());
	registry.setFactory("minecraft:skeleton", factory<Skeleton>());
	registry.setFactory("minecraft:stray", factory<Stray>());
	registry.setFactory("minecraft:cow", factory<Cow>());
	registry.setFactory("minecraft:pig", factory<Pig>());
	registry.setFactory("minecraft:chicken", factory<Chicken>());
	registry.setFactory("minecraft:sheep", factory<Sheep>());
}
