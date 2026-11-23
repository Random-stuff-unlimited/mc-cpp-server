#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/mobs/Bat.hpp"
#include "world/entity/mobs/Blaze.hpp"
#include "world/entity/mobs/Camelids.hpp"
#include "world/entity/mobs/Creeper.hpp"
#include "world/entity/mobs/Critters.hpp"
#include "world/entity/mobs/Dolphin.hpp"
#include "world/entity/mobs/Drowned.hpp"
#include "world/entity/mobs/Enderman.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/entity/mobs/Felines.hpp"
#include "world/entity/mobs/Fox.hpp"
#include "world/entity/mobs/Ghast.hpp"
#include "world/entity/mobs/Guardian.hpp"
#include "world/entity/mobs/Hoglin.hpp"
#include "world/entity/mobs/Horses.hpp"
#include "world/entity/mobs/Illagers.hpp"
#include "world/entity/mobs/IronGolem.hpp"
#include "world/entity/mobs/Panda.hpp"
#include "world/entity/mobs/Phantom.hpp"
#include "world/entity/mobs/PolarBear.hpp"
#include "world/entity/mobs/Rabbit.hpp"
#include "world/entity/mobs/Skeleton.hpp"
#include "world/entity/mobs/Silverfish.hpp"
#include "world/entity/mobs/Slime.hpp"
#include "world/entity/mobs/SnowGolem.hpp"
#include "world/entity/mobs/Spider.hpp"
#include "world/entity/mobs/Strider.hpp"
#include "world/entity/mobs/Turtle.hpp"
#include "world/entity/mobs/WaterAnimals.hpp"
#include "world/entity/mobs/Witch.hpp"
#include "world/entity/mobs/Wolf.hpp"
#include "world/entity/mobs/Zombie.hpp"
#include "world/entity/mobs/ZombifiedPiglin.hpp"

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
	registry.setFactory("minecraft:bat", factory<Bat>());
	registry.setFactory("minecraft:blaze", factory<Blaze>());
	registry.setFactory("minecraft:husk", factory<Husk>());
	registry.setFactory("minecraft:zombie_villager", factory<ZombieVillager>());
	registry.setFactory("minecraft:drowned", factory<Drowned>());
	registry.setFactory("minecraft:creeper", factory<Creeper>());
	registry.setFactory("minecraft:spider", factory<Spider>());
	registry.setFactory("minecraft:cave_spider", factory<CaveSpider>());
	registry.setFactory("minecraft:enderman", factory<Enderman>());
	registry.setFactory("minecraft:hoglin", factory<Hoglin>());
	registry.setFactory("minecraft:zoglin", factory<Zoglin>());
	registry.setFactory("minecraft:giant", factory<Giant>());
	registry.setFactory("minecraft:iron_golem", factory<IronGolem>());
	registry.setFactory("minecraft:skeleton", factory<Skeleton>());
	registry.setFactory("minecraft:stray", factory<Stray>());
	registry.setFactory("minecraft:bogged", factory<Bogged>());
	registry.setFactory("minecraft:wither_skeleton", factory<WitherSkeleton>());
	registry.setFactory("minecraft:zombified_piglin", factory<ZombifiedPiglin>());
	registry.setFactory("minecraft:cow", factory<Cow>());
	registry.setFactory("minecraft:mooshroom", factory<Mooshroom>());
	registry.setFactory("minecraft:pig", factory<Pig>());
	registry.setFactory("minecraft:chicken", factory<Chicken>());
	registry.setFactory("minecraft:sheep", factory<Sheep>());
	registry.setFactory("minecraft:polar_bear", factory<PolarBear>());
	registry.setFactory("minecraft:rabbit", factory<Rabbit>());
	registry.setFactory("minecraft:slime", factory<Slime>());
	registry.setFactory("minecraft:snow_golem", factory<SnowGolem>());
	registry.setFactory("minecraft:silverfish", factory<Silverfish>());
	registry.setFactory("minecraft:magma_cube", factory<MagmaCube>());
	registry.setFactory("minecraft:squid", factory<Squid>());
	registry.setFactory("minecraft:glow_squid", factory<GlowSquid>());
	registry.setFactory("minecraft:cod", factory<Cod>());
	registry.setFactory("minecraft:salmon", factory<Salmon>());
	registry.setFactory("minecraft:tropical_fish", factory<TropicalFish>());
	registry.setFactory("minecraft:pufferfish", factory<Pufferfish>());
	registry.setFactory("minecraft:dolphin", factory<Dolphin>());
	registry.setFactory("minecraft:ocelot", factory<Ocelot>());
	registry.setFactory("minecraft:cat", factory<Cat>());
	registry.setFactory("minecraft:strider", factory<Strider>());
	registry.setFactory("minecraft:fox", factory<Fox>());
	registry.setFactory("minecraft:panda", factory<Panda>());
	registry.setFactory("minecraft:horse", factory<Horse>());
	registry.setFactory("minecraft:donkey", factory<Donkey>());
	registry.setFactory("minecraft:mule", factory<Mule>());
	registry.setFactory("minecraft:skeleton_horse", factory<UndeadHorse>());
	registry.setFactory("minecraft:zombie_horse", factory<UndeadHorse>());
	registry.setFactory("minecraft:ghast", factory<Ghast>());
	registry.setFactory("minecraft:turtle", factory<Turtle>());
	registry.setFactory("minecraft:guardian", factory<Guardian>());
	registry.setFactory("minecraft:elder_guardian", factory<ElderGuardian>());
	registry.setFactory("minecraft:endermite", factory<Endermite>());
	registry.setFactory("minecraft:phantom", factory<Phantom>());
	registry.setFactory("minecraft:witch", factory<Witch>());
	registry.setFactory("minecraft:wolf", factory<Wolf>());
	registry.setFactory("minecraft:vindicator", factory<Vindicator>());
	registry.setFactory("minecraft:pillager", factory<Pillager>());
	registry.setFactory("minecraft:evoker", factory<Evoker>());
	registry.setFactory("minecraft:illusioner", factory<Illusioner>());
	registry.setFactory("minecraft:vex", factory<Vex>());
	registry.setFactory("minecraft:ravager", factory<Ravager>());
	registry.setFactory("minecraft:llama", factory<Llama>());
	registry.setFactory("minecraft:trader_llama", factory<TraderLlama>());
	registry.setFactory("minecraft:camel", factory<Camel>());
	registry.setFactory("minecraft:goat", factory<Goat>());
	registry.setFactory("minecraft:armadillo", factory<Armadillo>());
	registry.setFactory("minecraft:sniffer", factory<Sniffer>());
}
