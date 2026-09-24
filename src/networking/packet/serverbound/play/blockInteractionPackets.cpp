#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/World.hpp"

#include <cmath>
#include <string>
#include <vector>

// Block breaking and placing. The client changes the block right away (prediction) and sends a sequence number;
// the server applies the change, tells every player that has the chunk, then acknowledges the sequence. When it
// refuses, it sends the real block first so the client undoes its prediction.

namespace {
	enum PlayerActionStatus { START_DIGGING = 0, CANCEL_DIGGING = 1, FINISH_DIGGING = 2 };
	constexpr int LEVEL_EVENT_BLOCK_BREAK = 2001; // Particles and sound of a broken block

	// Direction ids: down, up, north, south, west, east
	constexpr int FACE_OFFSETS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
	constexpr const char* FACE_AXES[6] = {"y", "y", "z", "z", "x", "x"};

	// Horizontal directions in yaw order: 0° = south, 90° = west...
	constexpr const char* HORIZONTAL[4]		 = {"south", "west", "north", "east"};
	constexpr int		  HORIZONTAL_XZ[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};

	struct BlockChange {
		int x, y, z, state;
	};

	int horizontalFromYaw(float yaw) { return static_cast<int>(std::floor(yaw / 90.0 + 0.5)) & 3; }

	int horizontalFromName(const std::string& facing) {
		for (int i = 0; i < 4; i++) {
			if (facing == HORIZONTAL[i]) return i;
		}
		return -1;
	}

	// Positions and states a block takes when placed at (x, y, z): two for doors, tall plants and beds
	std::vector<BlockChange> placementParts(const GameData& gameData, const Player& player, int x, int y, int z, int state) {
		int direction = horizontalFromYaw(player.getYaw());
		switch (gameData.getBlockProperties(state).shape) {
		case GameData::Shape::DoubleHeight: {
			int facing = gameData.withProperty(state, "facing", HORIZONTAL[direction]); // Doors face the player's direction
			int lower  = gameData.withProperty(facing >= 0 ? facing : state, "half", "lower");
			return {{x, y, z, lower}, {x, y + 1, z, gameData.withProperty(lower, "half", "upper")}};
		}
		case GameData::Shape::DoubleLength: {
			// Head one block further in the direction the player looks
			int foot = gameData.withProperty(gameData.withProperty(state, "facing", HORIZONTAL[direction]), "part", "foot");
			return {{x, y, z, foot},
					{x + HORIZONTAL_XZ[direction][0], y, z + HORIZONTAL_XZ[direction][1], gameData.withProperty(foot, "part", "head")}};
		}
		default:
			return {{x, y, z, state}};
		}
	}

	// Where the other part of a two-position block is, if `state` is one
	bool otherPart(const GameData& gameData, int state, int& x, int& y, int& z) {
		switch (gameData.getBlockProperties(state).shape) {
		case GameData::Shape::DoubleHeight:
			y += gameData.getProperty(state, "half") == "lower" ? 1 : -1;
			return true;
		case GameData::Shape::DoubleLength: {
			int direction = horizontalFromName(gameData.getProperty(state, "facing"));
			if (direction < 0) return false;
			int sign = gameData.getProperty(state, "part") == "foot" ? 1 : -1;
			x += sign * HORIZONTAL_XZ[direction][0];
			z += sign * HORIZONTAL_XZ[direction][1];
			return true;
		}
		default:
			return false;
		}
	}

	void sendBlockUpdate(const std::shared_ptr<Player>& player, Server& server, int x, int y, int z, int state) {
		Buffer buf;
		buf.writePosition(x, y, z);
		buf.writeVarInt(state);
		Packet::send(player, PacketId::Play::Clientbound::BLOCK_UPDATE, buf, server);
	}

	void acknowledge(const std::shared_ptr<Player>& player, Server& server, int sequence) {
		Buffer buf;
		buf.writeVarInt(sequence);
		Packet::send(player, PacketId::Play::Clientbound::BLOCK_CHANGED_ACK, buf, server);
	}

	void broadcastBlockUpdate(Server& server, int x, int y, int z, int state) {
		Buffer buf;
		buf.writePosition(x, y, z);
		buf.writeVarInt(state);
		server.broadcastToChunk(x >> 4, z >> 4, PacketId::Play::Clientbound::BLOCK_UPDATE, buf);
	}

	// Changes a block and tells everyone who has it: the block, then the light around it
	void changeBlock(Server& server, int x, int y, int z, uint32_t state) {
		std::vector<World::LightUpdate> lightUpdates;
		server.getWorld().setBlock(x, y, z, state, &lightUpdates);
		broadcastBlockUpdate(server, x, y, z, static_cast<int>(state));
		for (World::LightUpdate& update : lightUpdates) {
			Buffer light(update.packet);
			server.broadcastToChunk(update.chunkX, update.chunkZ, PacketId::Play::Clientbound::LIGHT_UPDATE, light);
		}
	}

	// Vanilla allows 4.5 blocks in survival, 5 in creative, plus a margin for latency
	bool inReach(const Player& player, int x, int y, int z) {
		double range = (player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 1.0;
		double dx	 = x + 0.5 - player.getX();
		double dy	 = y + 0.5 - (player.getY() + 1.62); // Eye height
		double dz	 = z + 0.5 - player.getZ();
		return dx * dx + dy * dy + dz * dz <= (range + 0.87) * (range + 0.87); // + half the block's diagonal
	}

	// Returns false if the block can't be broken (then nothing changed)
	bool breakBlock(Player& player, Server& server, int x, int y, int z) {
		World& world = server.getWorld();
		int	   state = world.getBlock(x, y, z);
		if (state < 0 || world.isAir(state)) return false;
		if (player.getGameMode() != GameMode::Creative && server.getGameData().getDestroyTime(state) < 0) return false; // Bedrock...

		changeBlock(server, x, y, z, world.airState());

		// Breaking one half of a door, tall plant or bed removes the other one
		const GameData& gameData = server.getGameData();
		int				otherX = x, otherY = y, otherZ = z;
		if (otherPart(gameData, state, otherX, otherY, otherZ)) {
			int other = world.getBlock(otherX, otherY, otherZ);
			if (other >= 0 && gameData.getBlockOfState(other) == gameData.getBlockOfState(state)) {
				changeBlock(server, otherX, otherY, otherZ, world.airState());
			}
		}

		// Other players see the break effect; the breaker's client already played it
		Buffer effect;
		effect.writeInt(LEVEL_EVENT_BLOCK_BREAK);
		effect.writePosition(x, y, z);
		effect.writeInt(state);
		effect.writeBool(false);
		server.broadcastToChunk(x >> 4, z >> 4, PacketId::Play::Clientbound::LEVEL_EVENT, effect, &player);
		return true;
	}
} // namespace

void handlePlayerActionPacket(Packet& packet, Server& server) {
	Buffer&	 data	= packet.getData();
	int		 status = data.readVarInt();
	int32_t	 x, y, z;
	data.readPosition(x, y, z);
	data.readByte(); // Face
	int		 sequence = data.readVarInt();
	Player&	 player	  = *packet.getPlayer();
	auto	 self	  = player.shared_from_this();

	bool canBuild = player.getGameMode() == GameMode::Survival || player.getGameMode() == GameMode::Creative;
	bool refused  = false;

	if (status == CANCEL_DIGGING) {
		player.stopDigging();
	} else if (status == START_DIGGING) {
		int state = server.getWorld().getBlock(x, y, z);
		if (!canBuild || !inReach(player, x, y, z) || state < 0) {
			refused = true;
		} else if (player.getGameMode() == GameMode::Creative || server.getGameData().getDestroyTime(state) == 0) {
			// Instant break: always in creative, and for blocks that take no time (flowers, torches...)
			refused = !breakBlock(player, server, x, y, z);
		} else {
			player.startDigging(x, y, z); // Broken on FINISH_DIGGING
		}
	} else if (status == FINISH_DIGGING) {
		// TODO: check the digging time against the block's hardness and the tool (anti-cheat)
		refused = !canBuild || !player.isDigging(x, y, z) || !inReach(player, x, y, z) || !breakBlock(player, server, x, y, z);
		player.stopDigging();
	}
	// Other actions (drop item, swap hands...) aren't handled yet

	if (refused) {
		// Restore the real block on the client
		int state = server.getWorld().getBlock(x, y, z);
		if (state >= 0) sendBlockUpdate(self, server, x, y, z, state);
	}
	acknowledge(self, server, sequence);
}

void handleUseItemOnPacket(Packet& packet, Server& server) {
	Buffer& data = packet.getData();
	int		hand = data.readVarInt();
	int32_t x, y, z;
	data.readPosition(x, y, z);
	int face = data.readVarInt();
	data.readFloat(); // Cursor position on the face
	data.readFloat();
	data.readFloat();
	data.readBool(); // Inside block
	data.readBool(); // World border hit
	int sequence = data.readVarInt();

	Player&			player	 = *packet.getPlayer();
	auto			self	 = player.shared_from_this();
	const GameData& gameData = server.getGameData();
	World&			world	 = server.getWorld();

	int placed = gameData.getPlacedBlockState(player.getItemInHand(hand));
	if (placed < 0 || face < 0 || face > 5 || player.getGameMode() == GameMode::Spectator || player.getGameMode() == GameMode::Adventure) {
		// Not a block (or not allowed to build): nothing to place. Using items and blocks (doors, chests...) isn't handled yet
		acknowledge(self, server, sequence);
		return;
	}

	// Clicking a replaceable block (tall grass, snow layer...) places into it, otherwise next to the clicked face
	auto replaceable = [&](int state) {
		return state >= 0 && (world.isAir(state) || gameData.isInTag("minecraft:block", "minecraft:replaceable", gameData.getBlockOfState(state)));
	};
	int targetX = x, targetY = y, targetZ = z;
	if (!replaceable(world.getBlock(x, y, z))) {
		targetX += FACE_OFFSETS[face][0];
		targetY += FACE_OFFSETS[face][1];
		targetZ += FACE_OFFSETS[face][2];
	}

	int oriented = gameData.withProperty(placed, "axis", FACE_AXES[face]); // Logs, pillars...
	if (oriented >= 0) placed = oriented;

	// Every position the block needs must be free, or nothing is placed
	std::vector<BlockChange> parts = placementParts(gameData, player, targetX, targetY, targetZ, placed);
	bool					 fits  = inReach(player, targetX, targetY, targetZ);
	for (const BlockChange& part : parts) fits = fits && part.state >= 0 && replaceable(world.getBlock(part.x, part.y, part.z));

	for (const BlockChange& part : parts) {
		if (fits) {
			changeBlock(server, part.x, part.y, part.z, static_cast<uint32_t>(part.state));
		} else if (int actual = world.getBlock(part.x, part.y, part.z); actual >= 0) {
			sendBlockUpdate(self, server, part.x, part.y, part.z, actual); // Undo the client's prediction
		}
	}
	// TODO: consume the item in survival once the inventory is fully tracked
	acknowledge(self, server, sequence);
}

void handleSetCarriedItemPacket(Packet& packet, Server& server) {
	int slot = packet.getData().readShort();
	if (slot >= 0 && slot <= 8) packet.getPlayer()->setSelectedSlot(slot);
	(void)server;
}

// Creative inventory: the client tells the server what it put in each slot
void handleSetCreativeModeSlotPacket(Packet& packet, Server& server) {
	Buffer& data  = packet.getData();
	int		slot  = data.readShort();
	int		count = data.readVarInt();
	// Item stack: count, then item id and component changes (ignored) when not empty
	int item = count > 0 ? data.readVarInt() : -1;
	if (packet.getPlayer()->getGameMode() == GameMode::Creative) packet.getPlayer()->setInventorySlot(slot, item);
	(void)server;
}
