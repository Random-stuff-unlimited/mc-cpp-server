#include "Commands.hpp"
#include "PacketIds.hpp"
#include "lib/UUID.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Portals.hpp"
#include "world/blockentity/BlockEntity.hpp"
#include "world/entity/Entity.hpp"

// The smaller play packets the client sends: ping, difficulty and game mode from its menus and keys, the data queries
// of F3+I, picking an entity's item, spectator teleports, the client's channels

namespace {
	// ServerboundChangeGameModePacket's GameType ids
	const char* const GAME_MODES[] = {"survival", "creative", "adventure", "spectator"};
	const char* const DIFFICULTIES[] = {"peaceful", "easy", "normal", "hard"};

	void writeNbtString(Buffer& buf, const std::string& name, const std::string& value) {
		buf.writeByte(8);
		buf.writeUShort(static_cast<uint16_t>(name.size()));
		buf.writeBytes(name);
		buf.writeUShort(static_cast<uint16_t>(value.size()));
		buf.writeBytes(value);
	}
} // namespace

// ServerboundPingRequestPacket: the same number back (Pong Response), for the client's ping graph
void handlePlayPingRequest(Packet& packet, Server& server) {
	Buffer pong;
	pong.writeLong(packet.getData().readLong());
	Packet::send(packet.getPlayer()->shared_from_this(), PacketId::Play::Clientbound::PONG_RESPONSE, pong, server);
}

// ServerboundChangeDifficultyPacket (the options screen) and ServerboundLockDifficultyPacket: for operators (level 2)
void handleChangeDifficulty(Packet& packet, Server& server) {
	int difficulty = packet.getData().readUByte();
	if (difficulty < 0 || difficulty > 3 || server.isDifficultyLocked()) return;
	Commands::run(server, *packet.getPlayer(), std::string("difficulty ") + DIFFICULTIES[difficulty]);
}

void handleLockDifficulty(Packet& packet, Server& server) {
	bool	locked = packet.getData().readBool();
	Player& player = *packet.getPlayer();
	if (!player.hasPermissions(Commands::LEVEL_GAMEMASTERS) || !locked) return;
	server.setDifficultyLocked(true);
	for (const auto& other : server.getGamePlayers()) {
		Packet to(other->shared_from_this());
		changeDifficultyPacket(to, server);
	}
}

// ServerboundChangeGameModePacket: the F3+F4 game mode switcher, as /gamemode (level 2)
void handleChangeGameMode(Packet& packet, Server& server) {
	int mode = packet.getData().readVarInt();
	if (mode < 0 || mode > 3) return;
	Commands::run(server, *packet.getPlayer(), std::string("gamemode ") + GAME_MODES[mode]);
}

// ServerboundBlockEntityTagQueryPacket (F3+I on a block): its data (what the client knows of it), for level 2
void handleBlockEntityTagQuery(Packet& packet, Server& server) {
	Buffer& data		= packet.getData();
	int		transaction = data.readVarInt();
	int32_t x, y, z;
	data.readPosition(x, y, z);
	Player& player = *packet.getPlayer();
	if (!player.hasPermissions(Commands::LEVEL_GAMEMASTERS)) return;
	Buffer answer;
	answer.writeVarInt(transaction);
	BlockEntity* entity = server.levelOf(player).getBlockEntity({x, y, z});
	if (entity) {
		std::vector<uint8_t> tag;
		entity->writeUpdateTag(tag);
		answer.writeBytes(tag);
	} else {
		answer.writeByte(0); // No tag
	}
	Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::TAG_QUERY, answer, server);
}

// ServerboundEntityTagQueryPacket (F3+I on an entity): its type, position and UUID
void handleEntityTagQuery(Packet& packet, Server& server) {
	Buffer& data		= packet.getData();
	int		transaction = data.readVarInt();
	int		id			= data.readVarInt();
	Player& player		= *packet.getPlayer();
	if (!player.hasPermissions(Commands::LEVEL_GAMEMASTERS)) return;
	Level& level = server.levelOf(player);
	Buffer answer;
	answer.writeVarInt(transaction);
	Actor* actor = level.actorById(id);
	if (!actor) {
		answer.writeByte(0);
	} else {
		answer.writeByte(10);
		writeNbtString(answer, "id", level.gameData().getStaticName("minecraft:entity_type", actor->typeId()));
		writeNbtString(answer, "UUID", actor->uuid().toString());
		answer.writeByte(9); // Pos: a list of 3 doubles
		answer.writeUShort(3);
		answer.writeBytes(std::string("Pos"));
		answer.writeByte(6);
		answer.writeInt(3);
		answer.writeDouble(actor->position().x);
		answer.writeDouble(actor->position().y);
		answer.writeDouble(actor->position().z);
		answer.writeByte(0);
	}
	Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::TAG_QUERY, answer, server);
}

// ServerboundPickItemFromEntityPacket: the entity's item (its spawn egg, the boat or minecart it is...) in hand
void handlePickItemFromEntity(Packet& packet, Server& server) {
	Buffer& data = packet.getData();
	int		id	 = data.readVarInt();
	data.readBool(); // Include data: nothing more to copy here
	Player& player = *packet.getPlayer();
	Level&	level  = server.levelOf(player);
	Actor*	actor  = level.actorById(id);
	if (!actor || actor == &player) return;
	double dx = actor->position().x - player.getX(), dy = actor->position().y - player.getY(), dz = actor->position().z - player.getZ();
	if (dx * dx + dy * dy + dz * dz > 64.0) return; // canInteractWithEntity, roughly
	const GameData& gd	 = level.gameData();
	std::string		type = gd.getStaticName("minecraft:entity_type", actor->typeId());
	int				item = gd.getStaticId("minecraft:item", type + "_spawn_egg");
	if (item < 0) item = gd.getStaticId("minecraft:item", type);
	if (item < 0 && type == "minecraft:leash_knot") item = gd.getStaticId("minecraft:item", "minecraft:lead");
	if (item <= 0) return;
	tryPickItem(packet, server, player, level, ItemStack(item, 1), player.getGameMode() == GameMode::Creative);
}

// ServerboundTeleportToEntityPacket: a spectator goes to an entity or player, in whatever dimension
void handleTeleportToEntity(Packet& packet, Server& server) {
	UUID	target = packet.getData().readUUID();
	Player& player = *packet.getPlayer();
	if (!player.isSpectator()) return;
	for (const auto& level : server.getLevels()) {
		Actor* found = nullptr;
		for (const auto& other : level->players()) {
			if (other->getUUID() == target) found = other.get();
		}
		if (!found) {
			level->entities().forEach([&](Entity& entity) {
				if (!found && entity.uuid() == target) found = &entity;
			});
		}
		if (!found) continue;
		Vec3 at = found->position();
		if (level.get() == &server.levelOf(player)) {
			Commands::run(server, server.console(), "tp " + player.getPlayerName() + " " + std::to_string(at.x) + " " + std::to_string(at.y) + " " +
														 std::to_string(at.z));
		} else {
			server.changeDimension(player, *level, at.x, at.y, at.z, player.getYaw(), player.getPitch());
		}
		return;
	}
}

