#include "world/entity/EntityManager.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Chunk.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Level.hpp"
#include "data/GameData.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"
#include "logger.hpp"

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
	tracked->lastYRot	  = Mth::packDegrees(entity->yRot());
	tracked->lastXRot	  = Mth::packDegrees(entity->xRot());
	tracked->lastYHeadRot = Mth::packDegrees(entity->yHeadRot());
	tracked->living		  = dynamic_cast<LivingEntity*>(entity.get());
	// EntityType.clientTrackingRange, updateInterval, trackDeltas
	if (const GameData::EntityTypeInfo* type = _level.gameData().getEntityType(entity->typeId())) {
		tracked->trackingRange	= type->trackingRange * 16;
		tracked->updateInterval = std::max(1, type->updateInterval);
		tracked->trackDelta		= type->trackDeltas;
		_trackingChunks			= std::max(_trackingChunks, type->trackingRange + 1);
	}
	tracked->entity		  = std::move(entity);
	tracked->chunk		  = chunkKeyOf(tracked->entity->position());
	index(*tracked);
	_byId[tracked->entity->id()] = tracked.get();
	for (const auto& player : _level.players()) updateViewer(*tracked, *player);
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

Entity* EntityManager::byId(int id) {
	auto it = _byId.find(id);
	return it == _byId.end() ? nullptr : it->second->entity.get();
}

void EntityManager::broadcast(const Entity& entity, int packetId, Buffer& data) {
	auto it = _byId.find(entity.id());
	if (it != _byId.end()) sendToViewers(*it->second, packetId, data);
}

// ----- Saving -----

void EntityManager::queueLoad(std::vector<uint8_t> savedEntities) { _pendingLoads.push_back(std::move(savedEntities)); }

void EntityManager::processPendingLoads() {
	if (_pendingLoads.empty()) return;
	std::vector<std::vector<uint8_t>> loads = std::move(_pendingLoads);
	_pendingLoads.clear();
	const GameData& data = _level.gameData();
	for (std::vector<uint8_t>& blob : loads) {
		try {
			Buffer in(blob);
			int	   count = in.readVarInt();
			for (int i = 0; i < count; i++) {
				std::string			 type	= in.readString();
				std::vector<uint8_t> bytes	= in.readBytes(static_cast<size_t>(in.readVarInt()));
				int					 typeId = data.getStaticId("minecraft:entity_type", type);
				// EntityType.create: unknown types (removed in this version) and unreadable data are dropped
				std::unique_ptr<Mob> mob = typeId >= 0 ? _level.mobs().create(_level, typeId) : nullptr;
				if (!mob) continue;
				try {
					Buffer own(bytes);
					mob->load(own);
				} catch (const std::exception& e) {
					g_logger->logGameInfo(WARN, "Dropping unreadable " + type + ": " + e.what(), "World");
					continue;
				}
				add(std::move(mob));
			}
		} catch (const std::exception& e) {
			g_logger->logGameInfo(WARN, std::string("Unreadable entities in a chunk: ") + e.what(), "World");
		}
	}
}

std::vector<uint8_t> EntityManager::encodeChunk(int64_t key) const {
	auto it = _byChunk.find(key);
	if (it == _byChunk.end()) return {};
	int count = 0;
	for (const Tracked* tracked : it->second) count += tracked->entity->shouldBeSaved() && !tracked->entity->isRemoved();
	if (count == 0) return {};
	Buffer			out;
	const GameData& data = _level.gameData();
	out.writeVarInt(count);
	for (const Tracked* tracked : it->second) {
		const Entity& entity = *tracked->entity;
		if (!entity.shouldBeSaved() || entity.isRemoved()) continue;
		Buffer own;
		entity.save(own);
		out.writeString(data.getStaticName("minecraft:entity_type", entity.typeId()));
		out.writeVarInt(static_cast<int32_t>(own.getData().size()));
		out.writeBytes(own.getData());
	}
	return std::move(out.getData());
}

void EntityManager::unloadChunk(int64_t key) {
	auto it = _byChunk.find(key);
	if (it == _byChunk.end()) return;
	for (Tracked* tracked : it->second) {
		if (tracked->entity->shouldBeSaved()) tracked->entity->discard(); // Saved with the chunk; gone at the end of the tick
	}
}

// ----- Tick -----

void EntityManager::tick() {
	processPendingLoads();
	// Players first: they pick up the items they touch (Player.aiStep)
	for (const auto& player : _level.players()) {
		if (!player->isDisconnected() && !player->combat().dead && player->getGameMode() != GameMode::Spectator) pickUpItems(*player);
	}
	for (size_t i = 0; i < _entities.size(); i++) {
		Tracked& tracked = *_entities[i];
		Entity&	 entity	 = *tracked.entity;
		if (entity.isRemoved()) continue;
		entity.checkDespawn();
		if (entity.isRemoved()) continue;
		// Entities only tick in ticking chunks; the others wait
		if (!_level.shouldTickBlocksAt(entity.blockPosition())) continue;
		// ServerLevel.tickNonPassenger
		entity.setOldPosAndRot();
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
	spawn.writeUByte(static_cast<uint8_t>(tracked.lastXRot));
	spawn.writeUByte(static_cast<uint8_t>(tracked.lastYRot));
	spawn.writeUByte(static_cast<uint8_t>(tracked.lastYHeadRot));
	spawn.writeVarInt(entity.entityData()); // Entity-specific data (an experience orb's value, 0 otherwise)
	Packet::send(self, PacketId::Play::Clientbound::ADD_ENTITY, spawn, _level.server());

	// ServerEntity.sendPairingData: the values that aren't the client's defaults, then the attributes it is told about
	Buffer data;
	data.writeVarInt(entity.id());
	size_t header = data.getData().size();
	entity.writeEntityData(data);
	if (data.getData().size() > header) {
		data.writeUByte(0xFF);
		Packet::send(self, PacketId::Play::Clientbound::SET_ENTITY_DATA, data, _level.server());
	}
	if (tracked.living) {
		bool any = false;
		tracked.living->attributes().forEachSyncable([&](const AttributeInstance&) { any = true; });
		if (any) {
			Buffer attributes;
			attributes.writeVarInt(entity.id());
			tracked.living->attributes().writeSyncable(attributes, false);
			Packet::send(self, PacketId::Play::Clientbound::UPDATE_ATTRIBUTES, attributes, _level.server());
		}
		// Its equipment (armor, what it holds)
		Buffer equipment;
		if (tracked.living->writeEquipment(equipment)) Packet::send(self, PacketId::Play::Clientbound::SET_EQUIPMENT, equipment, _level.server());
	}
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
	for (const auto& player : _level.players()) {
		int64_t chunk = chunkKeyOf({player->getX(), 0, player->getZ()});
		auto	it	  = _playerChunks.find(player.get());
		if (it != _playerChunks.end() && it->second == chunk) continue;
		_playerChunks[player.get()] = chunk;
		int centerX = Mth::floor(player->getX()) >> 4, centerZ = Mth::floor(player->getZ()) >> 4;
		int radius	= _trackingChunks; // The largest tracking range seen, + the chunk the player left
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
		bool present = std::any_of(_level.players().begin(), _level.players().end(),
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
	processPendingLoads();
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
			for (const auto& player : _level.players()) updateViewer(tracked, *player);
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

// ServerEntity.sendChanges (no passengers or vehicles)
void EntityManager::sendUpdates(Tracked& tracked) {
	Entity& entity = *tracked.entity;
	if (tracked.serverTick % tracked.updateInterval == 0 || entity.hasImpulse || entity.entityDataDirty) {
		int8_t		yRot	 = Mth::packDegrees(entity.yRot());
		int8_t		xRot	 = Mth::packDegrees(entity.xRot());
		bool		rotated	 = yRot != tracked.lastYRot || xRot != tracked.lastXRot;
		tracked.teleportDelay++;
		const Vec3& position = entity.position();
		Vec3		delta	 = position - tracked.sentBase;
		bool		moved	 = delta.lengthSqr() >= MIN_MOVE_SQR;
		bool		send	 = moved || tracked.serverTick % 60 == 0;
		int64_t		dx = encode(position.x) - encode(tracked.sentBase.x), dy = encode(position.y) - encode(tracked.sentBase.y),
				dz	 = encode(position.z) - encode(tracked.sentBase.z);
		bool		sentPosition = false, sentRotation = false;

		// The position packet to send, if any
		Buffer move;
		int	   movePacket = -1;
		if (!fitsShort(dx) || !fitsShort(dy) || !fitsShort(dz) || tracked.teleportDelay > 400 || tracked.wasOnGround != entity.onGround()) {
			tracked.wasOnGround	  = entity.onGround();
			tracked.teleportDelay = 0;
			const Vec3& movement  = entity.deltaMovement();
			move.writeVarInt(entity.id());
			for (double v : {position.x, position.y, position.z, movement.x, movement.y, movement.z}) move.writeDouble(v);
			move.writeFloat(entity.yRot());
			move.writeFloat(entity.xRot());
			move.writeBool(entity.onGround());
			movePacket	 = PacketId::Play::Clientbound::ENTITY_POSITION_SYNC;
			sentPosition = sentRotation = true;
		} else if (!send || !rotated) {
			if (send) {
				move.writeVarInt(entity.id());
				move.writeShort(static_cast<int16_t>(dx));
				move.writeShort(static_cast<int16_t>(dy));
				move.writeShort(static_cast<int16_t>(dz));
				move.writeBool(entity.onGround());
				movePacket	 = PacketId::Play::Clientbound::MOVE_ENTITY_POS;
				sentPosition = true;
			} else if (rotated) {
				move.writeVarInt(entity.id());
				move.writeByte(yRot);
				move.writeByte(xRot);
				move.writeBool(entity.onGround());
				movePacket	 = PacketId::Play::Clientbound::MOVE_ENTITY_ROT;
				sentRotation = true;
			}
		} else {
			move.writeVarInt(entity.id());
			move.writeShort(static_cast<int16_t>(dx));
			move.writeShort(static_cast<int16_t>(dy));
			move.writeShort(static_cast<int16_t>(dz));
			move.writeByte(yRot);
			move.writeByte(xRot);
			move.writeBool(entity.onGround());
			movePacket	 = PacketId::Play::Clientbound::MOVE_ENTITY_POS_ROT;
			sentPosition = sentRotation = true;
		}

		// The movement, before the position
		if (entity.hasImpulse || tracked.trackDelta) {
			const Vec3& movement = entity.deltaMovement();
			double		changed	 = (movement - tracked.sentMovement).lengthSqr();
			if (changed > 1.0E-7 || (changed > 0.0 && movement.lengthSqr() == 0.0)) {
				tracked.sentMovement = movement;
				Buffer motion;
				motion.writeVarInt(entity.id());
				motion.writeLpVec3(movement.x, movement.y, movement.z);
				sendToViewers(tracked, PacketId::Play::Clientbound::SET_ENTITY_MOTION, motion);
			}
		}
		if (movePacket >= 0) sendToViewers(tracked, movePacket, move);
		sendDirtyData(tracked);
		if (sentPosition) tracked.sentBase = position;
		if (sentRotation) {
			tracked.lastYRot = yRot;
			tracked.lastXRot = xRot;
		}
		int8_t head = Mth::packDegrees(entity.yHeadRot());
		if (head != tracked.lastYHeadRot) {
			Buffer rotate;
			rotate.writeVarInt(entity.id());
			rotate.writeByte(head);
			sendToViewers(tracked, PacketId::Play::Clientbound::ROTATE_HEAD, rotate);
			tracked.lastYHeadRot = head;
		}
		entity.hasImpulse = false;
	}
	tracked.serverTick++;
	if (entity.hurtMarked) {
		// Knockback: everyone sees the new movement at once
		entity.hurtMarked	 = false;
		const Vec3& movement = entity.deltaMovement();
		Buffer		motion;
		motion.writeVarInt(entity.id());
		motion.writeLpVec3(movement.x, movement.y, movement.z);
		sendToViewers(tracked, PacketId::Play::Clientbound::SET_ENTITY_MOTION, motion);
	}
}

// ServerEntity.sendDirtyEntityData: the changed entity data, then the changed attributes
void EntityManager::sendDirtyData(Tracked& tracked) {
	Entity& entity = *tracked.entity;
	if (entity.entityDataDirty) {
		Buffer data;
		data.writeVarInt(entity.id());
		entity.writeDirtyEntityData(data);
		data.writeUByte(0xFF);
		sendToViewers(tracked, PacketId::Play::Clientbound::SET_ENTITY_DATA, data);
		entity.entityDataDirty = false;
		entity.clearDirtyEntityData();
	}
	if (tracked.living && tracked.living->attributes().hasDirtySyncable()) {
		Buffer attributes;
		attributes.writeVarInt(entity.id());
		tracked.living->attributes().writeSyncable(attributes, true);
		sendToViewers(tracked, PacketId::Play::Clientbound::UPDATE_ATTRIBUTES, attributes);
	}
	if (tracked.living) tracked.living->attributes().clearDirty();
}
