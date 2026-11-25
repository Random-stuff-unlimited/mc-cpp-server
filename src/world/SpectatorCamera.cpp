#include "world/SpectatorCamera.hpp"

#include "network/PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Level.hpp"
#include "world/entity/Entity.hpp"

namespace SpectatorCamera {
	namespace {
		// Where the camera is, the player goes (absSnapTo): the others see it there, its chunks follow
		void snapTo(Server& server, Player& player, const Actor& target) {
			const Vec3& at = target.position();
			player.setPosition(at.x, at.y, at.z);
			const Entity* entity = const_cast<Actor&>(target).asEntity();
			player.setRotation(target.yRot(), entity ? entity->xRot() : player.getPitch());
			server.getPlayerTracker().move(&player);
			if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->onPlayerMove(at.x, at.z);
		}
	} // namespace

	void setCamera(Server& server, Player& player, Actor* target) {
		if (target == &player) target = nullptr;
		int id = target ? target->id() : -1;
		if (player.cameraId() == id) return;
		player.setCameraId(id);
		Buffer camera;
		camera.writeVarInt(target ? target->id() : player.id());
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SET_CAMERA, camera, server);
		if (!target) return;
		snapTo(server, player, *target);
		// connection.teleport: the client's own player goes there too
		Buffer teleport;
		teleport.writeVarInt(player.nextTeleportId());
		teleport.writeDouble(player.getX());
		teleport.writeDouble(player.getY());
		teleport.writeDouble(player.getZ());
		teleport.writeDouble(0);
		teleport.writeDouble(0);
		teleport.writeDouble(0);
		teleport.writeFloat(player.getYaw());
		teleport.writeFloat(player.getPitch());
		teleport.writeInt(0);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::PLAYER_POSITION, teleport, server);
	}

	void tick(Server& server, Player& player) {
		if (player.cameraId() == -1) return;
		Level*	level  = player.level();
		Actor*	target = level ? level->actorById(player.cameraId()) : nullptr;
		Entity* entity = target ? target->asEntity() : nullptr;
		bool	gone   = !target || !target->isAlive() || (entity && entity->isRemoved());
		// wantsToStopRiding: sneaking
		if (gone || player.getGameMode() != GameMode::Spectator || player.isShiftKeyDown()) {
			setCamera(server, player, nullptr);
			return;
		}
		snapTo(server, player, *target);
	}
} // namespace SpectatorCamera
