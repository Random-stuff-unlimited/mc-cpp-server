#include "world/entity/LightningBolt.hpp"
#include "world/entity/AreaEffectCloud.hpp"
#include "world/entity/LeashKnot.hpp"
#include "world/entity/Vehicles.hpp"
#include "world/entity/EndCrystal.hpp"
#include "world/entity/EntityFactory.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/entity/Arrow.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/PrimedTnt.hpp"

namespace EntityFactory {

	std::unique_ptr<Entity> create(Level& level, int typeId) {
		if (std::unique_ptr<Mob> mob = level.mobs().create(level, typeId)) return mob;
		const std::string& name = level.gameData().getStaticName("minecraft:entity_type", typeId);
		if (name == "minecraft:item") return std::make_unique<ItemEntity>(level, Vec3{}, ItemStack{}, Vec3{});
		if (name == "minecraft:tnt") return std::make_unique<PrimedTnt>(level, Vec3{}, nullptr);
		if (name == "minecraft:arrow") return std::make_unique<Arrow>(level);
		if (name == "minecraft:trident") return std::make_unique<ThrownTrident>(level);
		if (name == "minecraft:end_crystal") return std::make_unique<EndCrystal>(level);
		if (name == "minecraft:leash_knot") return std::make_unique<LeashKnot>(level);
		if (name == "minecraft:lightning_bolt") return std::make_unique<LightningBolt>(level);
		if (name == "minecraft:area_effect_cloud") return std::make_unique<AreaEffectCloud>(level);
		if (const GameData::EntityTypeInfo* info = level.gameData().getEntityType(typeId); info && info->is("AbstractBoat")) {
			return std::make_unique<Boat>(level, typeId);
		}
		if (const GameData::EntityTypeInfo* info = level.gameData().getEntityType(typeId); info && info->is("AbstractMinecart")) {
			return std::make_unique<Minecart>(level, typeId);
		}
		return nullptr;
	}

	std::unique_ptr<Entity> copy(const Entity& entity, Level& destination) {
		std::unique_ptr<Entity> copied = create(destination, entity.typeId());
		if (!copied) return nullptr;
		Buffer data;
		entity.save(data);
		Buffer in(data.getData());
		copied->load(in);
		copied->setUuid(entity.uuid());
		return copied;
	}

} // namespace EntityFactory
