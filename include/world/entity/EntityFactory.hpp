#ifndef ENTITY_FACTORY_HPP
#define ENTITY_FACTORY_HPP

#include <memory>

class Entity;
class Level;

// EntityType.create for every entity the server simulates: mobs (through the level's MobRegistry), items, primed TNT,
// projectiles... Used to load saved entities and to move one to another dimension
namespace EntityFactory {
	// An entity of this type, not in the level yet; nullptr for types the server doesn't simulate
	std::unique_ptr<Entity> create(Level& level, int typeId);
	// Entity.teleportCrossDimension's copy: the same entity (type, UUID, state) for another level, nullptr if it can't
	std::unique_ptr<Entity> copy(const Entity& entity, Level& destination);
} // namespace EntityFactory

#endif
