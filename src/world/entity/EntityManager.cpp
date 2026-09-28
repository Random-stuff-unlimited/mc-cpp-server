#include "world/entity/EntityManager.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Chunk.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"

#include <algorithm>
#include <cmath>

namespace {
	constexpr float	 PLAYER_WIDTH  = 0.6f;
	constexpr float	 PLAYER_HEIGHT = 1.8f;
	constexpr double MIN_MOVE_SQR  = 7.6293945E-6f; // Smaller moves aren't sent

	int64_t chunkKeyOf(const Vec3& position) { return Chunk::key(Mth::floor(position.x) >> 4, Mth::floor(position.z) >> 4); }
	int64_t encode(double coordinate) { return std::llround(coordinate * 4096.0); } // VecDeltaCodec
	bool	fitsShort(int64_t value) { return value >= -32768 && value <= 32767; }
} // namespace

EntityManager::~EntityManager() = default;

Entity* EntityManager::add(std::unique_ptr<Entity> entity) {
	auto tracked		  = std::make_unique<Tracked>();
	tracked->sentBase	  = entity->position();
	tracked->sentMovement = entity->deltaMovement();
	tracked->wasOnGround  = entity->onGround();
	tracked->entity		  = std::move(entity);
	tracked->chunk		  = chunkKeyOf(tracked->entity->position());
	index(*tracked);
	_byId[tracked->entity->id()] = tracked.get();
	for (const auto& player : _level.server().getGamePlayers()) updateViewer(*tracked, *player);
	Entity* added = tracked->entity.get();
	_entities.push_back(std::move(tracked));
	return added;
}

void EntityManager::index(Tracked& tracked) { _byChunk[tracked.chunk].push_back(&tracked); }

void EntityManager::unindex(Tracked& tracked) {
	auto it = _byChunk.find(tracked.chunk);
	if (it == _byChunk.end()) return;
	auto& list = it->second;
	list.erase(std::remove(list.begin(), list.end(), &tracked), list.end());
	if (list.empty()) _byChunk.erase(it);
}

std::vector<ItemEntity*> EntityManager::itemsIn(const AABB& box) {
	std::vector<ItemEntity*> items;
	for (int x = Mth::floor(box.minX) >> 4; x <= Mth::floor(box.maxX) >> 4; x++) {
		for (int z = Mth::floor(box.minZ) >> 4; z <= Mth::floor(box.maxZ) >> 4; z++) {
			auto it = _byChunk.find(Chunk::key(x, z));
			if (it == _byChunk.end()) continue;
			for (Tracked* tracked : it->second) {
				auto* item = dynamic_cast<ItemEntity*>(tracked->entity.get());
				if (item && !item->isRemoved() && item->boundingBox().intersects(box)) items.push_back(item);
			}
		}
	}
	return items;
}

std::vector<Entity*> EntityManager::entitiesIn(const AABB& box) {
	std::vector<Entity*> found;
	for (int x = Mth::floor(box.minX) >> 4; x <= Mth::floor(box.maxX) >> 4; x++) {
		for (int z = Mth::floor(box.minZ) >> 4; z <= Mth::floor(box.maxZ) >> 4; z++) {
			auto it = _byChunk.find(Chunk::key(x, z));
			if (it == _byChunk.end()) continue;
			for (Tracked* tracked : it->second) {
				if (!tracked->entity->isRemoved() && tracked->entity->boundingBox().intersects(box)) found.push_back(tracked->entity.get());
			}
		}
	}
	return found;
}

// ----- Tick -----

void EntityManager::tick() {
	// Players first: they pick up the items they touch (Player.aiStep)
	for (const auto& player : _level.server().getGamePlayers()) {
		if (!player->isDisconnected() && !player->combat().dead && player->getGameMode() != GameMode::Spectator) pickUpItems(*player);
	}
	for (size_t i = 0; i < _entities.size(); i++) {
		Tracked& tracked = *_entities[i];
		Entity&	 entity	 = *tracked.entity;
		if (entity.isRemoved()) continue;
		// Entities only tick in ticking chunks; the others wait
		if (!_level.shouldTickBlocksAt(entity.blockPosition())) continue;
		entity.tickCountIncrement();
		entity.tick();
	}
}

void EntityManager::pickUpItems(Player& player) {
	double half = PLAYER_WIDTH / 2.0;
	AABB   box{player.getX() - half, player.getY(), player.getZ() - half, player.getX() + half, player.getY() + PLAYER_HEIGHT, player.getZ() + half};
	for (ItemEntity* item : itemsIn(box.inflate(1.0, 0.5, 1.0))) {
		int taken = item->playerTouch(player);
		if (taken <= 0) continue;
		// Player.take: the pickup animation for everyone who sees the item
		Buffer take;
		take.writeVarInt(item->id());
		take.writeVarInt(player.getPlayerID());
		take.writeVarInt(taken);
		Tracked& tracked = *_byId.at(item->id());
		sendToViewers(tracked, PacketId::Play::Clientbound::TAKE_ITEM_ENTITY, take);
		if (!tracked.viewers.count(&player)) Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::TAKE_ITEM_ENTITY, take, _level.server());
	}
}

// ----- Tracking -----

void EntityManager::updateViewer(Tracked& tracked, Player& player) {
	const Entity&  entity	= *tracked.entity;
	ChunkStreamer* streamer = player.getChunkStreamer();
	bool		   visible	= false;
	if (!entity.isRemoved() && !player.isDisconnected() && streamer) {
		double dx	   = player.getX() - entity.position().x;
		double dz	   = player.getZ() - entity.position().z;
		double range   = std::min(tracked.trackingRange, streamer->viewDistance() * 16);
		int	   chunkX  = Mth::floor(entity.position().x) >> 4;
		int	   chunkZ  = Mth::floor(entity.position().z) >> 4;
		visible		   = dx * dx + dz * dz <= range * range && streamer->hasChunk(chunkX, chunkZ);
	}
	bool shown = tracked.viewers.count(&player) != 0;
	if (visible && !shown) {
		tracked.viewers.insert(&player);
		spawnFor(tracked, player);
	} else if (!visible && shown) {
		tracked.viewers.erase(&player);
		despawnFor(tracked, player);
	}
}

void EntityManager::spawnFor(Tracked& tracked, Player& player) {
	const Entity& entity = *tracked.entity;
	auto		  self	 = player.shared_from_this();
	Buffer		  spawn;
	spawn.writeVarInt(entity.id());
	spawn.writeUUID(entity.uuid());
	spawn.writeVarInt(entity.typeId());
	spawn.writeDouble(tracked.sentBase.x);
	spawn.writeDouble(tracked.sentBase.y);
	spawn.writeDouble(tracked.sentBase.z);
	spawn.writeLpVec3(tracked.sentMovement.x, tracked.sentMovement.y, tracked.sentMovement.z);
	spawn.writeUByte(0); // Pitch
	spawn.writeUByte(0); // Yaw
	spawn.writeUByte(0); // Head yaw
	spawn.writeVarInt(0); // Entity-specific data
	Packet::send(self, PacketId::Play::Clientbound::ADD_ENTITY, spawn, _level.server());

	Buffer data;
	data.writeVarInt(entity.id());
	entity.writeEntityData(data);
	data.writeUByte(0xFF);
	Packet::send(self, PacketId::Play::Clientbound::SET_ENTITY_DATA, data, _level.server());
}

void EntityManager::despawnFor(Tracked& tracked, Player& player) {
	Buffer remove;
	remove.writeVarInt(1);
	remove.writeVarInt(tracked.entity->id());
	Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::REMOVE_ENTITIES, remove, _level.server());
}

void EntityManager::sendToViewers(const Tracked& tracked, int packetId, Buffer& data) {
	if (tracked.viewers.empty()) return;
	std::vector<uint8_t> frame = Packet::buildFrame(packetId, data.getData(), _level.server().getConfig().getCompressionThreshold());
	for (Player* viewer : tracked.viewers) Packet::sendFrame(viewer->shared_from_this(), frame, _level.server());
}

void EntityManager::forgetPlayer(Player* player) {
	for (const auto& tracked : _entities) tracked->viewers.erase(player);
	_playerChunks.erase(player);
}

// Players that changed chunk see the entities around them again (vanilla updates the players that moved)
void EntityManager::updatePlayerChunks() {
	for (const auto& player : _level.server().getGamePlayers()) {
		int64_t chunk = chunkKeyOf({player->getX(), 0, player->getZ()});
		auto	it	  = _playerChunks.find(player.get());
		if (it != _playerChunks.end() && it->second == chunk) continue;
		_playerChunks[player.get()] = chunk;
		int centerX = Mth::floor(player->getX()) >> 4, centerZ = Mth::floor(player->getZ()) >> 4;
		int radius	= 7; // Tracking ranges reach 6 chunks for items; + the chunk the player left
		for (int x = centerX - radius; x <= centerX + radius; x++) {
			for (int z = centerZ - radius; z <= centerZ + radius; z++) {
				auto entities = _byChunk.find(Chunk::key(x, z));
				if (entities == _byChunk.end()) continue;
				for (Tracked* tracked : entities->second) updateViewer(*tracked, *player);
			}
		}
		// Entities it still sees from far away (it moved fast)
		for (const auto& tracked : _entities) {
			if (tracked->viewers.count(player.get())) updateViewer(*tracked, *player);
		}
	}
	// Forget the players that left
	for (auto it = _playerChunks.begin(); it != _playerChunks.end();) {
		bool present = std::any_of(_level.server().getGamePlayers().begin(), _level.server().getGamePlayers().end(),
								   [&](const auto& p) { return p.get() == it->first; });
		if (present) {
			++it;
			continue;
		}
		for (const auto& tracked : _entities) tracked->viewers.erase(it->first);
		it = _playerChunks.erase(it);
	}
}

void EntityManager::sendChanges() {
	updatePlayerChunks();
	for (size_t i = 0; i < _entities.size(); i++) {
		Tracked& tracked = *_entities[i];
		Entity&	 entity	 = *tracked.entity;
		if (entity.isRemoved()) continue;
		int64_t chunk = chunkKeyOf(entity.position());
		if (chunk != tracked.chunk) {
			unindex(tracked);
			tracked.chunk = chunk;
			index(tracked);
			for (const auto& player : _level.server().getGamePlayers()) updateViewer(tracked, *player);
		}
		sendUpdates(tracked);
	}

	// Removed entities: gone for everyone who saw them
	for (auto it = _entities.begin(); it != _entities.end();) {
		Tracked& tracked = **it;
		if (!tracked.entity->isRemoved()) {
			++it;
			continue;
		}
		for (Player* viewer : tracked.viewers) despawnFor(tracked, *viewer);
		unindex(tracked);
		_byId.erase(tracked.entity->id());
		it = _entities.erase(it);
	}
}

// ServerEntity.sendChanges, for entities that don't rotate or ride
void EntityManager::sendUpdates(Tracked& tracked) {
	Entity& entity = *tracked.entity;
	if (tracked.serverTick % tracked.updateInterval == 0 || entity.hasImpulse || entity.entityDataDirty) {
		tracked.teleportDelay++;
		const Vec3& position = entity.position();
		Vec3		delta	 = position - tracked.sentBase;
		bool		moved	 = delta.lengthSqr() >= MIN_MOVE_SQR;
		bool		send	 = moved || tracked.serverTick % 60 == 0;
		int64_t		dx = encode(position.x) - encode(tracked.sentBase.x), dy = encode(position.y) - encode(tracked.sentBase.y),
				dz	 = encode(position.z) - encode(tracked.sentBase.z);
		bool		sentPosition = false;

		// The movement first
		const Vec3& movement = entity.deltaMovement();
		double		changed	 = (movement - tracked.sentMovement).lengthSqr();
		if (changed > 1.0E-7 || (changed > 0.0 && movement.lengthSqr() == 0.0)) {
			tracked.sentMovement = movement;
			Buffer motion;
			motion.writeVarInt(entity.id());
			motion.writeLpVec3(movement.x, movement.y, movement.z);
			sendToViewers(tracked, PacketId::Play::Clientbound::SET_ENTITY_MOTION, motion);
		}
		if (!fitsShort(dx) || !fitsShort(dy) || !fitsShort(dz) || tracked.teleportDelay > 400 || tracked.wasOnGround != entity.onGround()) {
			tracked.wasOnGround	  = entity.onGround();
			tracked.teleportDelay = 0;
			Buffer sync;
			sync.writeVarInt(entity.id());
			sync.writeDouble(position.x);
			sync.writeDouble(position.y);
			sync.writeDouble(position.z);
			sync.writeDouble(movement.x);
			sync.writeDouble(movement.y);
			sync.writeDouble(movement.z);
			sync.writeFloat(0); // Yaw
			sync.writeFloat(0); // Pitch
			sync.writeBool(entity.onGround());
			sendToViewers(tracked, PacketId::Play::Clientbound::ENTITY_POSITION_SYNC, sync);
			sentPosition = true;
		} else if (send) {
			Buffer move;
			move.writeVarInt(entity.id());
			move.writeShort(static_cast<int16_t>(dx));
			move.writeShort(static_cast<int16_t>(dy));
			move.writeShort(static_cast<int16_t>(dz));
			move.writeBool(entity.onGround());
			sendToViewers(tracked, PacketId::Play::Clientbound::MOVE_ENTITY_POS, move);
			sentPosition = true;
		}
		if (entity.entityDataDirty) {
			Buffer data;
			data.writeVarInt(entity.id());
			entity.writeEntityData(data);
			data.writeUByte(0xFF);
			sendToViewers(tracked, PacketId::Play::Clientbound::SET_ENTITY_DATA, data);
			entity.entityDataDirty = false;
		}
		if (sentPosition) tracked.sentBase = position;
		entity.hasImpulse = false;
	}
	tracked.serverTick++;
}
