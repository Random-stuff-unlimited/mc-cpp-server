#include "world/PlayerTracker.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/TextComponent.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	// Player info actions (ClientboundPlayerInfoUpdatePacket.Action ordinals), sent as a bit set
	constexpr uint8_t INFO_ADD_PLAYER	   = 1 << 0;
	constexpr uint8_t INFO_UPDATE_GAMEMODE = 1 << 2;
	constexpr uint8_t INFO_UPDATE_LISTED   = 1 << 3;
	constexpr uint8_t INFO_UPDATE_LATENCY  = 1 << 4;

	int64_t toFixed(double coordinate) { return static_cast<int64_t>(std::llround(coordinate * 4096.0)); }
	uint8_t toAngle(float degrees) { return static_cast<uint8_t>(static_cast<int>(std::floor(degrees * 256.0f / 360.0f)) & 0xFF); }
	int		toChunk(double coordinate) { return static_cast<int>(std::floor(coordinate / 16.0)); }
	bool	fitsShort(int64_t delta) { return delta >= std::numeric_limits<int16_t>::min() && delta <= std::numeric_limits<int16_t>::max(); }
} // namespace

PlayerTracker::PlayerTracker(Server& server) : _server(server) {}

void PlayerTracker::join(const std::shared_ptr<Player>& player, int viewDistance) {
	Tracked& joined		= _players[player.get()];
	joined.player		= player;
	joined.viewDistance = viewDistance;
	joined.chunkX		= toChunk(player->getX());
	joined.chunkZ		= toChunk(player->getZ());
	joined.sentX		= toFixed(player->getX());
	joined.sentY		= toFixed(player->getY());
	joined.sentZ		= toFixed(player->getZ());
	joined.sentYaw		= toAngle(player->getYaw());
	joined.sentPitch	= toAngle(player->getPitch());
	addToCell(joined);
	_byEntityId[player->getPlayerID()] = player.get();
	_maxViewDistance				   = std::max(_maxViewDistance, viewDistance);

	// Tab list: the newcomer gets everyone (itself included), everyone else gets the newcomer
	std::vector<Player*> everyone;
	everyone.reserve(_players.size());
	for (auto& [key, tracked] : _players) everyone.push_back(key);
	sendPlayerInfo(player, everyone);
	Buffer newcomer;
	writePlayerInfo(newcomer, {player.get()});
	std::vector<uint8_t> frame =
			Packet::buildFrame(PacketId::Play::Clientbound::PLAYER_INFO_UPDATE, newcomer.getData(), _server.getConfig().getCompressionThreshold());
	for (auto& [key, tracked] : _players) {
		if (key != player.get()) Packet::sendFrame(tracked.player, frame, _server);
	}

	updateVisibilityAround(joined);
	broadcastMessage("multiplayer.player.joined", {player->getPlayerName()}, "yellow");
}

void PlayerTracker::leave(Player* player) {
	auto it = _players.find(player);
	if (it == _players.end()) return;
	Tracked& leaving = it->second;

	Buffer despawn;
	despawn.writeVarInt(1);
	despawn.writeVarInt(player->getPlayerID());
	sendToViewers(leaving, PacketId::Play::Clientbound::REMOVE_ENTITIES, despawn);
	for (Player* viewer : leaving.viewers) _players[viewer].visible.erase(player);
	for (Player* target : leaving.visible) _players[target].viewers.erase(player);

	Buffer info;
	info.writeVarInt(1);
	info.writeUUID(player->getUUID());
	std::vector<uint8_t> frame = Packet::buildFrame(PacketId::Play::Clientbound::PLAYER_INFO_REMOVE, info.getData(), _server.getConfig().getCompressionThreshold());
	for (auto& [key, other] : _players) {
		if (key != player) Packet::sendFrame(other.player, frame, _server);
	}
	removeFromCell(leaving);
	_byEntityId.erase(player->getPlayerID());
	_players.erase(it);
	broadcastMessage("multiplayer.player.left", {player->getPlayerName()}, "yellow");
}

void PlayerTracker::move(Player* player) {
	auto it = _players.find(player);
	if (it == _players.end() || it->second.moved) return;
	it->second.moved = true;
	_moved.push_back(player);
}

void PlayerTracker::tick(int64_t tickCount) {
	if (_moved.empty()) return;

	// Who sees whom only changes when crossing a chunk border: checked every tick
	for (Player* player : _moved) {
		auto it = _players.find(player);
		if (it == _players.end()) continue; // Left meanwhile
		Tracked& tracked = it->second;
		int		 chunkX	 = toChunk(player->getX());
		int		 chunkZ	 = toChunk(player->getZ());
		if (chunkX == tracked.chunkX && chunkZ == tracked.chunkZ) continue;
		bool cellChanged = cellKey(chunkX, chunkZ) != cellKey(tracked.chunkX, tracked.chunkZ);
		if (cellChanged) removeFromCell(tracked);
		tracked.chunkX = chunkX;
		tracked.chunkZ = chunkZ;
		if (cellChanged) addToCell(tracked);
		updateVisibilityAround(tracked);
	}
	if (tickCount % UPDATE_INTERVAL != 0) return;

	// Movements, encoded once per mover
	for (Player* player : _moved) {
		auto it = _players.find(player);
		if (it == _players.end()) continue;
		it->second.moved = false;
		encodeMovement(it->second, tickCount);
		if (!it->second.update.empty()) _updated.push_back(&it->second);
	}
	_moved.clear();

	// Then one append per viewer, with every movement it sees this tick: in a crowd, each player receives the
	// movements of all the others at once instead of one by one
	for (Tracked* mover : _updated) {
		for (Player* viewer : mover->viewers) {
			Tracked& receiver = _players.at(viewer);
			if (receiver.pending.empty()) _receivers.push_back(&receiver);
			receiver.pending.push_back(mover);
		}
	}
	std::vector<uint8_t> batch;
	for (Tracked* receiver : _receivers) {
		batch.clear();
		for (const Tracked* mover : receiver->pending) batch.insert(batch.end(), mover->update.begin(), mover->update.end());
		receiver->pending.clear();
		Packet::sendFrame(receiver->player, batch, _server);
	}
	for (Tracked* mover : _updated) mover->update.clear();
	_updated.clear();
	_receivers.clear();
}

// Movement since the last update, as vanilla's ServerEntity.sendChanges: relative moves, and the absolute position
// when the move is too long for them or every FORCE_SYNC_TICKS to correct the rounding drift
void PlayerTracker::encodeMovement(Tracked& tracked, int64_t tickCount) {
	Player* player = tracked.player.get();
	int64_t x = toFixed(player->getX()), y = toFixed(player->getY()), z = toFixed(player->getZ());
	int64_t dx = x - tracked.sentX, dy = y - tracked.sentY, dz = z - tracked.sentZ;
	uint8_t yaw = toAngle(player->getYaw()), pitch = toAngle(player->getPitch());
	bool	positionChanged = dx != 0 || dy != 0 || dz != 0;
	bool	rotationChanged = yaw != tracked.sentYaw || pitch != tracked.sentPitch;
	bool	sync = positionChanged && (!(fitsShort(dx) && fitsShort(dy) && fitsShort(dz)) || tickCount - tracked.lastSync >= FORCE_SYNC_TICKS);

	tracked.sentX	  = x;
	tracked.sentY	  = y;
	tracked.sentZ	  = z;
	tracked.sentYaw	  = yaw;
	tracked.sentPitch = pitch;
	if (sync) tracked.lastSync = tickCount;
	// Nobody sees it: nothing to encode, new viewers spawn it at the position above
	if (tracked.viewers.empty() || !(positionChanged || rotationChanged)) return;

	int		entityId  = player->getPlayerID();
	int		threshold = _server.getConfig().getCompressionThreshold();
	Buffer	data;
	if (sync) {
		data.writeVarInt(entityId);
		data.writeDouble(player->getX());
		data.writeDouble(player->getY());
		data.writeDouble(player->getZ());
		data.writeDouble(0); // Velocity
		data.writeDouble(0);
		data.writeDouble(0);
		data.writeFloat(player->getYaw());
		data.writeFloat(player->getPitch());
		data.writeBool(player->isOnGround());
		Packet::appendFrame(tracked.update, PacketId::Play::Clientbound::ENTITY_POSITION_SYNC, data.getData().data(), data.getData().size(), threshold);
	} else {
		data.writeVarInt(entityId);
		if (positionChanged) {
			data.writeShort(static_cast<int16_t>(dx));
			data.writeShort(static_cast<int16_t>(dy));
			data.writeShort(static_cast<int16_t>(dz));
		}
		if (rotationChanged) {
			data.writeUByte(yaw);
			data.writeUByte(pitch);
		}
		data.writeBool(player->isOnGround());
		int packetId = positionChanged && rotationChanged ? PacketId::Play::Clientbound::MOVE_ENTITY_POS_ROT
					 : positionChanged					 ? PacketId::Play::Clientbound::MOVE_ENTITY_POS
														 : PacketId::Play::Clientbound::MOVE_ENTITY_ROT;
		Packet::appendFrame(tracked.update, packetId, data.getData().data(), data.getData().size(), threshold);
	}

	if (rotationChanged) {
		Buffer head;
		head.writeVarInt(entityId);
		head.writeUByte(yaw);
		Packet::appendFrame(tracked.update, PacketId::Play::Clientbound::ROTATE_HEAD, head.getData().data(), head.getData().size(), threshold);
	}
}

void PlayerTracker::respawn(Player* player) {
	auto it = _players.find(player);
	if (it == _players.end()) return;
	Tracked& tracked = it->second;

	// The Respawn packet made its client forget every entity, and the others must see it spawn again
	Buffer despawn;
	despawn.writeVarInt(1);
	despawn.writeVarInt(player->getPlayerID());
	sendToViewers(tracked, PacketId::Play::Clientbound::REMOVE_ENTITIES, despawn);
	for (Player* viewer : tracked.viewers) _players[viewer].visible.erase(player);
	for (Player* target : tracked.visible) _players[target].viewers.erase(player);
	tracked.viewers.clear();
	tracked.visible.clear();

	removeFromCell(tracked);
	tracked.chunkX	  = toChunk(player->getX());
	tracked.chunkZ	  = toChunk(player->getZ());
	tracked.sentX	  = toFixed(player->getX());
	tracked.sentY	  = toFixed(player->getY());
	tracked.sentZ	  = toFixed(player->getZ());
	tracked.sentYaw	  = toAngle(player->getYaw());
	tracked.sentPitch = toAngle(player->getPitch());
	addToCell(tracked);
	updateVisibilityAround(tracked);
}

std::shared_ptr<Player> PlayerTracker::findVisible(Player* viewer, int entityId) {
	auto id = _byEntityId.find(entityId);
	if (id == _byEntityId.end()) return nullptr;
	const Tracked& target = _players.at(id->second);
	return target.viewers.count(viewer) ? target.player : nullptr;
}

void PlayerTracker::broadcast(Player* subject, int packetId, Buffer& data, bool includeSubject) {
	auto it = _players.find(subject);
	if (it == _players.end()) return;
	if (!includeSubject) {
		sendToViewers(it->second, packetId, data);
		return;
	}
	std::vector<uint8_t> frame = Packet::buildFrame(packetId, data.getData(), _server.getConfig().getCompressionThreshold());
	sendFrameToViewers(it->second, frame);
	Packet::sendFrame(it->second.player, frame, _server);
}

int64_t PlayerTracker::cellKey(int chunkX, int chunkZ) {
	return (static_cast<int64_t>(chunkZ >> CELL_SHIFT) << 32) | static_cast<uint32_t>(chunkX >> CELL_SHIFT);
}

void PlayerTracker::addToCell(const Tracked& tracked) { _cells[cellKey(tracked.chunkX, tracked.chunkZ)].push_back(tracked.player.get()); }

void PlayerTracker::removeFromCell(const Tracked& tracked) {
	auto cell = _cells.find(cellKey(tracked.chunkX, tracked.chunkZ));
	if (cell == _cells.end()) return;
	std::vector<Player*>& players = cell->second;
	auto				  it	  = std::find(players.begin(), players.end(), tracked.player.get());
	if (it != players.end()) {
		*it = players.back();
		players.pop_back();
	}
	if (players.empty()) _cells.erase(cell);
}

void PlayerTracker::updateVisibilityAround(Tracked& subject) {
	Player*				 self = subject.player.get();
	std::vector<Player*> candidates(subject.viewers.begin(), subject.viewers.end());
	candidates.insert(candidates.end(), subject.visible.begin(), subject.visible.end());
	// Nobody sees further than the largest view distance
	int radius = _maxViewDistance;
	for (int cellZ = (subject.chunkZ - radius) >> CELL_SHIFT; cellZ <= (subject.chunkZ + radius) >> CELL_SHIFT; cellZ++) {
		for (int cellX = (subject.chunkX - radius) >> CELL_SHIFT; cellX <= (subject.chunkX + radius) >> CELL_SHIFT; cellX++) {
			auto cell = _cells.find(cellKey(cellX << CELL_SHIFT, cellZ << CELL_SHIFT));
			if (cell != _cells.end()) candidates.insert(candidates.end(), cell->second.begin(), cell->second.end());
		}
	}
	std::sort(candidates.begin(), candidates.end());
	candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
	for (Player* other : candidates) {
		if (other != self) updateVisibility(subject, _players.at(other));
	}
}

bool PlayerTracker::canSee(const Tracked& viewer, const Tracked& target) const {
	return std::abs(viewer.chunkX - target.chunkX) <= viewer.viewDistance && std::abs(viewer.chunkZ - target.chunkZ) <= viewer.viewDistance;
}

// Shows or hides a and b to each other according to their distance
void PlayerTracker::updateVisibility(Tracked& a, Tracked& b) {
	for (auto [target, viewer] : {std::pair<Tracked*, Tracked*>{&a, &b}, {&b, &a}}) {
		bool visible = canSee(*viewer, *target);
		bool shown	 = target->viewers.count(viewer->player.get()) != 0;
		if (visible && !shown) {
			show(*target, *viewer);
		} else if (!visible && shown) {
			hide(*target, *viewer);
		}
	}
}

void PlayerTracker::hide(Tracked& target, Tracked& viewer) {
	target.viewers.erase(viewer.player.get());
	viewer.visible.erase(target.player.get());
	Buffer despawn;
	despawn.writeVarInt(1);
	despawn.writeVarInt(target.player->getPlayerID());
	Packet::send(viewer.player, PacketId::Play::Clientbound::REMOVE_ENTITIES, despawn, _server);
}

// Spawns target's entity on viewer's client, at the position the other viewers last received
void PlayerTracker::show(Tracked& target, Tracked& viewer) {
	target.viewers.insert(viewer.player.get());
	viewer.visible.insert(target.player.get());

	Buffer spawn;
	spawn.writeVarInt(target.player->getPlayerID());
	spawn.writeUUID(target.player->getUUID());
	spawn.writeVarInt(_server.getGameData().getStaticId("minecraft:entity_type", "minecraft:player"));
	spawn.writeDouble(target.sentX / 4096.0);
	spawn.writeDouble(target.sentY / 4096.0);
	spawn.writeDouble(target.sentZ / 4096.0);
	spawn.writeUByte(0); // Velocity (LpVec3): 0 = none
	spawn.writeUByte(target.sentPitch);
	spawn.writeUByte(target.sentYaw);
	spawn.writeUByte(target.sentYaw); // Head
	spawn.writeVarInt(0);			  // Entity-specific data
	Packet::send(viewer.player, PacketId::Play::Clientbound::ADD_ENTITY, spawn, _server);
}

// Encoded once for all the viewers
void PlayerTracker::sendToViewers(const Tracked& target, int packetId, Buffer& data) {
	if (target.viewers.empty()) return;
	sendFrameToViewers(target, Packet::buildFrame(packetId, data.getData(), _server.getConfig().getCompressionThreshold()));
}

void PlayerTracker::sendFrameToViewers(const Tracked& target, const std::vector<uint8_t>& frame) {
	for (Player* viewer : target.viewers) {
		auto it = _players.find(viewer);
		if (it != _players.end()) Packet::sendFrame(it->second.player, frame, _server);
	}
}

void PlayerTracker::broadcastMessage(const std::string& translationKey, const std::vector<std::string>& args, const std::string& color) {
	Buffer message;
	TextComponent::writeTranslatable(message, translationKey, args, color);
	message.writeBool(false); // In the chat, not above the hotbar
	std::vector<uint8_t> frame =
			Packet::buildFrame(PacketId::Play::Clientbound::SYSTEM_CHAT, message.getData(), _server.getConfig().getCompressionThreshold());
	for (auto& [key, tracked] : _players) Packet::sendFrame(tracked.player, frame, _server);
}

// Tab list entries: name, game mode, shown in the list
void PlayerTracker::writePlayerInfo(Buffer& info, const std::vector<Player*>& players) {
	info.writeUByte(INFO_ADD_PLAYER | INFO_UPDATE_GAMEMODE | INFO_UPDATE_LISTED | INFO_UPDATE_LATENCY);
	info.writeVarInt(static_cast<int32_t>(players.size()));
	for (Player* player : players) {
		info.writeUUID(player->getUUID());
		info.writeString(player->getPlayerName());
		info.writeVarInt(0); // Profile properties (skin): none in offline mode
		info.writeVarInt(static_cast<int32_t>(player->getGameMode()));
		info.writeBool(true); // Listed
		info.writeVarInt(0);  // Latency (ms)
	}
}

void PlayerTracker::sendPlayerInfo(const std::shared_ptr<Player>& to, const std::vector<Player*>& players) {
	Buffer info;
	writePlayerInfo(info, players);
	Packet::send(to, PacketId::Play::Clientbound::PLAYER_INFO_UPDATE, info, _server);
}
