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
class LivingEntity;
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
	// The same without building a list: f(Entity&) for each
	template <typename F> void forEachIn(const AABB& box, F f) {
		for (int x = Mth::floor(box.minX) >> 4; x <= Mth::floor(box.maxX) >> 4; x++) {
			for (int z = Mth::floor(box.minZ) >> 4; z <= Mth::floor(box.maxZ) >> 4; z++) {
				auto it = _byChunk.find(chunkKey(x, z));
				if (it == _byChunk.end()) continue;
				for (Tracked* tracked : it->second) {
					Entity& entity = *tracked->entity;
					if (!entity.isRemoved() && entity.boundingBox().intersects(box)) f(entity);
				}
			}
		}
	}
	size_t					 count() const { return _entities.size(); }
	template <typename F> void forEach(F f) {
		for (const auto& tracked : _entities) f(*tracked->entity);
	}
	// nullptr if no entity has this id (players aside)
	Entity* byId(int id);
	// To every player that sees the entity (ChunkMap.broadcast)
	void	broadcast(const Entity& entity, int packetId, Buffer& data);

	// ----- Saving with the chunks -----

	// The saved entities of a chunk that joined the level, added at the start of the next entity phase (a chunk can
	// join in the middle of an entity's move)
	void				 queueLoad(std::vector<uint8_t> savedEntities);
	void				 processPendingLoads();
	// What is saved of the entities in this chunk (Chunk::key): empty if none. Format: VarInt count, then per entity
	// its type name (String), VarInt length and its own data (Entity::save)
	std::vector<uint8_t> encodeChunk(int64_t chunkKey) const;
	// The chunk left the level: its saved entities go with it
	void				 unloadChunk(int64_t chunkKey);

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
		bool						trackDelta	   = true;
		int8_t						lastYRot = 0, lastXRot = 0, lastYHeadRot = 0; // Packed degrees
		LivingEntity*				living		   = nullptr; // The entity, if it is one (attributes to send)
	};

	Level&											   _level;
	std::vector<std::unique_ptr<Tracked>>			   _entities;
	std::unordered_map<int64_t, std::vector<Tracked*>> _byChunk;
	std::unordered_map<int, Tracked*>				   _byId;
	std::unordered_map<Player*, int64_t>			   _playerChunks; // Chunk of each player at the last check
	std::vector<std::vector<uint8_t>>				   _pendingLoads;
	int												   _trackingChunks = 7; // Chunks around a player where entities can be seen, + 1

	static int64_t chunkKey(int x, int z) { return (static_cast<int64_t>(z) << 32) | static_cast<uint32_t>(x); } // Chunk::key
	void		   sendDirtyData(Tracked& tracked);
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
