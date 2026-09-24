#include "world/PlayerTracker.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/TextComponent.hpp"
#include "network/server.hpp"
#include "player.hpp"

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
	std::lock_guard<std::mutex> lock(_mutex);

	Tracked& joined		 = _players[player.get()];
	joined.player		 = player;
	joined.viewDistance	 = viewDistance;
	joined.chunkX		 = toChunk(player->getX());
	joined.chunkZ		 = toChunk(player->getZ());
	joined.sentX		 = toFixed(player->getX());
	joined.sentY		 = toFixed(player->getY());
	joined.sentZ		 = toFixed(player->getZ());
	joined.sentYaw		 = toAngle(player->getYaw());
	joined.sentPitch	 = toAngle(player->getPitch());

	// Tab list: the newcomer gets everyone (itself included), everyone else gets the newcomer
	std::vector<Player*> everyone;
	for (auto& [key, tracked] : _players) everyone.push_back(key);
	sendPlayerInfo(player, everyone);
	for (auto& [key, tracked] : _players) {
		if (key != player.get()) sendPlayerInfo(tracked.player, {player.get()});
	}

	for (auto& [key, other] : _players) {
		if (key != player.get()) updateVisibility(joined, other);
	}
	sendMessageLocked("multiplayer.player.joined", {player->getPlayerName()}, "yellow");
}

void PlayerTracker::leave(Player* player) {
	std::lock_guard<std::mutex> lock(_mutex);
	auto						it = _players.find(player);
	if (it == _players.end()) return;

	Buffer despawn;
	despawn.writeVarInt(1);
	despawn.writeVarInt(player->getPlayerID());
	sendToViewers(it->second, PacketId::Play::Clientbound::REMOVE_ENTITIES, despawn);

	Buffer info;
	info.writeVarInt(1);
	info.writeUUID(player->getUUID());
	for (auto& [key, other] : _players) {
		other.viewers.erase(player);
		if (key != player) Packet::send(other.player, PacketId::Play::Clientbound::PLAYER_INFO_REMOVE, info, _server);
	}
	_players.erase(it);
	sendMessageLocked("multiplayer.player.left", {player->getPlayerName()}, "yellow");
}

void PlayerTracker::move(Player* player, bool positionChanged, bool rotationChanged) {
	std::lock_guard<std::mutex> lock(_mutex);
	auto						it = _players.find(player);
	if (it == _players.end()) return;
	Tracked& moved = it->second;

	// Players that already see the mover get the movement first; visibility is updated after, so new viewers
	// spawn it directly at its new position
	int		entityId = player->getPlayerID();
	int64_t x = toFixed(player->getX()), y = toFixed(player->getY()), z = toFixed(player->getZ());
	int64_t dx = x - moved.sentX, dy = y - moved.sentY, dz = z - moved.sentZ;
	uint8_t yaw = toAngle(player->getYaw()), pitch = toAngle(player->getPitch());
	positionChanged = positionChanged && (dx != 0 || dy != 0 || dz != 0);
	rotationChanged = rotationChanged && (yaw != moved.sentYaw || pitch != moved.sentPitch);

	if (positionChanged && !(fitsShort(dx) && fitsShort(dy) && fitsShort(dz))) {
		// More than 8 blocks: absolute position
		Buffer sync;
		sync.writeVarInt(entityId);
		sync.writeDouble(player->getX());
		sync.writeDouble(player->getY());
		sync.writeDouble(player->getZ());
		sync.writeDouble(0); // Velocity
		sync.writeDouble(0);
		sync.writeDouble(0);
		sync.writeFloat(player->getYaw());
		sync.writeFloat(player->getPitch());
		sync.writeBool(player->isOnGround());
		sendToViewers(moved, PacketId::Play::Clientbound::ENTITY_POSITION_SYNC, sync);
	} else if (positionChanged || rotationChanged) {
		Buffer move;
		move.writeVarInt(entityId);
		if (positionChanged) {
			move.writeShort(static_cast<int16_t>(dx));
			move.writeShort(static_cast<int16_t>(dy));
			move.writeShort(static_cast<int16_t>(dz));
		}
		if (rotationChanged) {
			move.writeUByte(yaw);
			move.writeUByte(pitch);
		}
		move.writeBool(player->isOnGround());
		int packetId = positionChanged && rotationChanged ? PacketId::Play::Clientbound::MOVE_ENTITY_POS_ROT
					 : positionChanged					 ? PacketId::Play::Clientbound::MOVE_ENTITY_POS
														 : PacketId::Play::Clientbound::MOVE_ENTITY_ROT;
		sendToViewers(moved, packetId, move);
	}

	if (rotationChanged) {
		Buffer head;
		head.writeVarInt(entityId);
		head.writeUByte(yaw);
		sendToViewers(moved, PacketId::Play::Clientbound::ROTATE_HEAD, head);
	}
	if (positionChanged) {
		moved.sentX = x;
		moved.sentY = y;
		moved.sentZ = z;
	}
	if (rotationChanged) {
		moved.sentYaw	= yaw;
		moved.sentPitch = pitch;
	}

	// Who sees whom only changes when crossing a chunk border
	int chunkX = toChunk(player->getX());
	int chunkZ = toChunk(player->getZ());
	if (chunkX != moved.chunkX || chunkZ != moved.chunkZ) {
		moved.chunkX = chunkX;
		moved.chunkZ = chunkZ;
		for (auto& [key, other] : _players) {
			if (key != player) updateVisibility(moved, other);
		}
	}
}

void PlayerTracker::respawn(Player* player) {
	std::lock_guard<std::mutex> lock(_mutex);
	auto						it = _players.find(player);
	if (it == _players.end()) return;
	Tracked& tracked = it->second;

	Buffer despawn;
	despawn.writeVarInt(1);
	despawn.writeVarInt(player->getPlayerID());
	sendToViewers(tracked, PacketId::Play::Clientbound::REMOVE_ENTITIES, despawn);
	for (auto& [key, other] : _players) {
		if (key != player) other.viewers.erase(player);
	}
	tracked.viewers.clear();

	tracked.chunkX	  = toChunk(player->getX());
	tracked.chunkZ	  = toChunk(player->getZ());
	tracked.sentX	  = toFixed(player->getX());
	tracked.sentY	  = toFixed(player->getY());
	tracked.sentZ	  = toFixed(player->getZ());
	tracked.sentYaw	  = toAngle(player->getYaw());
	tracked.sentPitch = toAngle(player->getPitch());
	for (auto& [key, other] : _players) {
		if (key != player) updateVisibility(tracked, other);
	}
}

std::shared_ptr<Player> PlayerTracker::findVisible(Player* viewer, int entityId) {
	std::lock_guard<std::mutex> lock(_mutex);
	for (auto& [key, tracked] : _players) {
		if (key->getPlayerID() == entityId && tracked.viewers.count(viewer)) return tracked.player;
	}
	return nullptr;
}

void PlayerTracker::broadcast(Player* subject, int packetId, Buffer& data, bool includeSubject) {
	std::lock_guard<std::mutex> lock(_mutex);
	auto						it = _players.find(subject);
	if (it == _players.end()) return;
	sendToViewers(it->second, packetId, data);
	if (includeSubject) Packet::send(it->second.player, packetId, data, _server);
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
			target->viewers.erase(viewer->player.get());
			Buffer despawn;
			despawn.writeVarInt(1);
			despawn.writeVarInt(target->player->getPlayerID());
			Packet::send(viewer->player, PacketId::Play::Clientbound::REMOVE_ENTITIES, despawn, _server);
		}
	}
}

// Spawns target's entity on viewer's client, at the position the other viewers last received
void PlayerTracker::show(Tracked& target, Tracked& viewer) {
	target.viewers.insert(viewer.player.get());

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

void PlayerTracker::sendToViewers(const Tracked& target, int packetId, Buffer& data) {
	if (target.viewers.empty()) return;
	std::vector<uint8_t> frame = Packet::buildFrame(packetId, data.getData(), _server.getConfig().getCompressionThreshold());
	for (Player* viewer : target.viewers) {
		auto it = _players.find(viewer);
		if (it != _players.end()) Packet::sendFrame(it->second.player, frame, _server);
	}
}

void PlayerTracker::broadcastMessage(const std::string& translationKey, const std::vector<std::string>& args, const std::string& color) {
	std::lock_guard<std::mutex> lock(_mutex);
	sendMessageLocked(translationKey, args, color);
}

void PlayerTracker::sendMessageLocked(const std::string& translationKey, const std::vector<std::string>& args, const std::string& color) {
	Buffer message;
	TextComponent::writeTranslatable(message, translationKey, args, color);
	message.writeBool(false); // In the chat, not above the hotbar
	std::vector<uint8_t> frame =
			Packet::buildFrame(PacketId::Play::Clientbound::SYSTEM_CHAT, message.getData(), _server.getConfig().getCompressionThreshold());
	for (auto& [key, tracked] : _players) Packet::sendFrame(tracked.player, frame, _server);
}

// Tab list entries: name, game mode, shown in the list
void PlayerTracker::sendPlayerInfo(const std::shared_ptr<Player>& to, const std::vector<Player*>& players) {
	Buffer info;
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
	Packet::send(to, PacketId::Play::Clientbound::PLAYER_INFO_UPDATE, info, _server);
}
