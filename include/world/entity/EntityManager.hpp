#ifndef ENTITY_MANAGER_HPP
#define ENTITY_MANAGER_HPP

#include "world/entity/Entity.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ItemEntity;
class Level;
class Player;

// The entities of the level (players aside), on the game thread: they tick in the order they were added (vanilla's
// entity tick list), players pick up items, and each player is sent the entities near it (vanilla's ChunkMap
// TrackedEntity and ServerEntity: spawn, then movement every updateInterval ticks or on impulse)
class EntityManager {
  public:
	explicit EntityManager(Level& level) : _level(level) {}
	~EntityManager();

	Entity* add(std::unique_ptr<Entity> entity);
	// Entity phase of the tick: players touch items, then every entity ticks (only in ticking chunks)
	void tick();
	// End of the tick: spawns, movements and removals to the players
	void sendChanges();
	// The player left the game: it can't receive anything anymore
	void forgetPlayer(Player* player);

	std::vector<ItemEntity*> itemsIn(const AABB& box);
	// Every entity whose box touches this one (not removed)
	std::vector<Entity*>	 entitiesIn(const AABB& box);
	size_t					 count() const { return _entities.size(); }
	template <typename F> void forEach(F f) {
		for (const auto& tracked : _entities) f(*tracked->entity);
	}

  private:
	// What each player last received about an entity (vanilla's ServerEntity)
	struct Tracked {
		std::unique_ptr<Entity>		entity;
		std::unordered_set<Player*> viewers;
		int64_t						chunk;		 // Chunk::key where it was last indexed
		Vec3						sentBase;	 // Position the relative moves start from
		Vec3						sentMovement;
		int							serverTick	  = 0;
		int							teleportDelay = 0;
		bool						wasOnGround	  = false;
		int							trackingRange = 96; // clientTrackingRange * 16
		int							updateInterval = 20;
	};

	Level&											   _level;
	std::vector<std::unique_ptr<Tracked>>			   _entities;
	std::unordered_map<int64_t, std::vector<Tracked*>> _byChunk;
	std::unordered_map<int, Tracked*>				   _byId;
	std::unordered_map<Player*, int64_t>			   _playerChunks; // Chunk of each player at the last check

	void index(Tracked& tracked);
	void unindex(Tracked& tracked);
	// TrackedEntity.updatePlayer: shows or hides the entity for this player
	void updateViewer(Tracked& tracked, Player& player);
	void spawnFor(Tracked& tracked, Player& player);
	void despawnFor(Tracked& tracked, Player& player);
	void sendUpdates(Tracked& tracked);
	void sendToViewers(const Tracked& tracked, int packetId, Buffer& data);
	void pickUpItems(Player& player);
	void updatePlayerChunks();
};

#endif
