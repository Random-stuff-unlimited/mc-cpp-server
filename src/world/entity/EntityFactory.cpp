#include "world/entity/EntityFactory.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
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
