#include "world/inventory/BeaconMenu.hpp"
#include "world/item/Books.hpp"
#include "world/blocks/Signs.hpp"
#include "world/entity/LeashKnot.hpp"
#include "world/entity/Vehicles.hpp"
#include "world/entity/EndCrystal.hpp"
#include "world/inventory/MerchantMenu.hpp"
#include "world/entity/mobs/Horses.hpp"
#include "world/entity/Fireballs.hpp"
#include "PacketIds.hpp"
#include "Commands.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Combat.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/inventory/AnvilMenu.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"
#include "world/Survival.hpp"

#include <algorithm>

namespace {
	// The four Move packets share this: position and rotation are set, then the movement, exhaustion, pressure
	// plates and fall damage run
	constexpr uint8_t MOVE_FLAG_ON_GROUND = 0x01;

	// A line of chat: with a leading '/', a command; otherwise a message for everyone
	void handleChatLine(Server& server, Player& player, const std::string& line) {
		if (!line.empty() && line[0] == '/') {
			Commands::run(server, player, line.substr(1));
		} else {
			Commands::broadcastChat(server, player, line);
		}
	}

	void handleMove(Packet* packet, Server& server, bool withPosition, bool withRotation) {
		Buffer& data	  = packet->getData();
		Player* player	  = packet->getPlayer();
		double	x = player->getX(), y = player->getY(), z = player->getZ();
		double	previousX = x, previousY = y, previousZ = z;
		// Riding: its position is its vehicle's, only its look counts (handleMovePlayer, isPassenger)
		bool riding = player->vehicleId() != -1;
		if (withPosition) {
			x = data.readDouble();
			y = data.readDouble(); // Feet
			z = data.readDouble();
			if (riding) {
				x = previousX, y = previousY, z = previousZ;
				withPosition = false;
			} else {
				player->setPosition(x, y, z);
			}
		}
		if (withRotation) {
			float yaw = data.readFloat();
			player->setRotation(yaw, data.readFloat());
		}
		bool onGround = data.readUByte() & MOVE_FLAG_ON_GROUND;
		if (riding) {
			server.getPlayerTracker().move(player);
			return;
		}
		// Jump and movement exhaustion (handleMovePlayer: jumpFromGround, setOnGroundWithMovement, checkMovementStatistics)
		if (withPosition && !player->combat().dead) {
			Survival::onMove(server.levelOf(*player), *player, x - previousX, y - previousY, z - previousZ, onGround);
		}
		player->setOnGround(onGround);

		if (withPosition) {
			if (ChunkStreamer* streamer = player->getChunkStreamer()) streamer->onPlayerMove(x, z);
		}
		server.getPlayerTracker().move(player);
		if (withPosition) Combat::onMove(server, *player, previousY);
		// applyEffectsFromBlocks: pressure plates under the player
		if (withPosition && player->getGameMode() != GameMode::Spectator && !player->combat().dead) {
			double half = Player::BB_WIDTH / 2.0;
			server.levelOf(*player).checkInsideBlocks({x - half, y, z - half, x + half, y + Player::BB_HEIGHT, z + half}, player);
		}
	}

	enum InteractType { INTERACT = 0, ATTACK = 1, INTERACT_AT = 2 };
	enum PlayerCommandAction { START_SPRINTING = 1, STOP_SPRINTING = 2, START_RIDING_JUMP = 3, OPEN_INVENTORY = 5 };
	constexpr int CLIENT_COMMAND_RESPAWN = 0;
	constexpr int ANIMATE_SWING_MAIN_HAND = 0, ANIMATE_SWING_OFF_HAND = 3;
} // namespace

// Game thread: the Play state. Everything the client can send once it is in the world
void handlePlayState(Packet* packet, Server& server) {
	Player* player = packet->getPlayer();
	int32_t id	   = static_cast<int32_t>(packet->getId());
	switch (id) {
	case PacketId::Play::Serverbound::ACCEPT_TELEPORTATION:
		handleConfirmTeleportationPacket(*packet, server);
		gameEventPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::MOVE_PLAYER_POS:
		handleMove(packet, server, true, false);
		break;
	case PacketId::Play::Serverbound::MOVE_PLAYER_POS_ROT:
		handleMove(packet, server, true, true);
		break;
	case PacketId::Play::Serverbound::MOVE_PLAYER_ROT:
		handleMove(packet, server, false, true);
		break;
	case PacketId::Play::Serverbound::MOVE_PLAYER_STATUS_ONLY:
		handleMove(packet, server, false, false);
		break;
	case PacketId::Play::Serverbound::CHUNK_BATCH_RECEIVED:
		if (ChunkStreamer* streamer = player->getChunkStreamer()) streamer->onBatchReceived(packet->getData().readFloat());
		break;
	case PacketId::Play::Serverbound::KEEP_ALIVE:
		player->onKeepAliveResponse(packet->getData().readLong());
		break;
	case PacketId::Play::Serverbound::PLAYER_ACTION:
		handlePlayerActionPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::USE_ITEM_ON:
		handleUseItemOnPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::USE_ITEM:
		handleUseItemPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::PLAYER_ABILITIES: {
		// handlePlayerAbilities: the client starts or stops flying, if it may fly
		bool mayFly = player->getGameMode() == GameMode::Creative || player->getGameMode() == GameMode::Spectator;
		player->survival().flying = (packet->getData().readUByte() & 0x02) && mayFly;
		break;
	}
	case PacketId::Play::Serverbound::SET_CARRIED_ITEM:
		handleSetCarriedItemPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::SET_CREATIVE_MODE_SLOT:
		handleSetCreativeModeSlotPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::PICK_ITEM_FROM_BLOCK:
		handlePickItemFromBlock(*packet, server);
		break;
	case PacketId::Play::Serverbound::INTERACT: {
		int entityId = packet->getData().readVarInt();
		int type	 = packet->getData().readVarInt();
		if (type == INTERACT) {
			// ServerGamePacketListenerImpl.handleInteract: Player.interactOn a mob in reach (Mob.interact / mobInteract)
			int		hand   = packet->getData().readVarInt();
			Entity* entity = server.levelOf(*player).entities().byId(entityId);
			Mob*	mob	   = dynamic_cast<Mob*>(entity);
			bool	used   = false;
			if (player->getGameMode() != GameMode::Spectator && entity && entity->isAlive()) {
				used = mob ? (mob->handleLeashInteraction(*player, hand) || mob->mobInteract(*player, hand)) : entity->interact(*player, hand); // Boats, minecarts
			}
			if (used) {
				Buffer animation;
				animation.writeVarInt(player->getPlayerID());
				animation.writeUByte(hand == 0 ? ANIMATE_SWING_MAIN_HAND : ANIMATE_SWING_OFF_HAND);
				server.getPlayerTracker().broadcast(player, PacketId::Play::Clientbound::ANIMATE, animation, false);
			}
		} else if (type == ATTACK) {
			if (auto target = server.getPlayerTracker().findVisible(player, entityId)) {
				Combat::attack(server, *player, *target);
			} else if (auto* living = dynamic_cast<LivingEntity*>(server.levelOf(*player).entities().byId(entityId))) {
				Combat::attack(server, *player, *living);
			} else if (auto* vehicle = dynamic_cast<VehicleEntity*>(server.levelOf(*player).entities().byId(entityId)); vehicle && player->getGameMode() != GameMode::Spectator) {
				// Player.attack on a boat or minecart: the hit's damage (the hand's 1, plus the weapon's)
				const GameData::ItemProperties* weapon = server.getGameData().getItemProperties(player->getStackInHand(0).item);
				float damage = 1.0F + (weapon ? weapon->attackDamage : 0.0F);
				vehicle->hurtServer({"minecraft:player_attack", player, nullptr, std::nullopt}, damage);
			} else if (auto* knot = dynamic_cast<LeashKnot*>(server.levelOf(*player).entities().byId(entityId)); knot && player->getGameMode() != GameMode::Spectator) {
				knot->hurtServer({"minecraft:player_attack", player, nullptr, std::nullopt}, 1.0F);
			} else if (auto* crystal = dynamic_cast<EndCrystal*>(server.levelOf(*player).entities().byId(entityId)); crystal && player->getGameMode() != GameMode::Spectator) {
				crystal->hurtServer({"minecraft:player_attack", player, nullptr, std::nullopt}, 1.0F);
			} else if (auto* fireball = dynamic_cast<LargeFireball*>(server.levelOf(*player).entities().byId(entityId))) {
				// Player.attack on a redirectable projectile: sent back along the player's look (Projectile.deflect, AIM_DEFLECT)
				if (player->getGameMode() != GameMode::Spectator && !fireball->isRemoved()) {
					const float toRadians = static_cast<float>(M_PI / 180.0);
					float		yaw = player->getYaw() * toRadians, pitch = player->getPitch() * toRadians;
					fireball->deflectAim(*player, {-Mth::sin(yaw) * Mth::cos(pitch), -Mth::sin(pitch), Mth::cos(yaw) * Mth::cos(pitch)});
					Level& level = server.levelOf(*player);
					level.playSoundAt(nullptr, player->getX(), player->getY(), player->getZ(), "minecraft:entity.player.attack.nodamage",
									  Level::SoundSource::Players, 1.0F, 1.0F);
				}
			}
		}
		break;
	}
	case PacketId::Play::Serverbound::SWING: {
		Buffer animation;
		animation.writeVarInt(player->getPlayerID());
		animation.writeUByte(packet->getData().readVarInt() == 0 ? ANIMATE_SWING_MAIN_HAND : ANIMATE_SWING_OFF_HAND);
		server.getPlayerTracker().broadcast(player, PacketId::Play::Clientbound::ANIMATE, animation, false);
		break;
	}
	case PacketId::Play::Serverbound::CONTAINER_CLICK:
		handleContainerClickPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::CONTAINER_CLOSE:
		handleContainerClosePacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::CONTAINER_BUTTON_CLICK:
		handleContainerButtonClickPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::CONTAINER_SLOT_STATE_CHANGED:
		handleContainerSlotStateChangedPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::RENAME_ITEM: {
		// The anvil's name field: its result and cost follow
		packet->getData().readVarInt(); // Container id
		std::string name = packet->getData().readString(50);
		if (auto* anvil = dynamic_cast<AnvilMenu*>(&Menus::current(*player, server.levelOf(*player)))) anvil->renameItem(name);
		break;
	}
	case PacketId::Play::Serverbound::SELECT_TRADE: {
		// The offer clicked in the merchant's list
		int index = packet->getData().readVarInt();
		if (auto* merchant = dynamic_cast<MerchantMenu*>(&Menus::current(*player, server.levelOf(*player)))) merchant->selectTrade(index);
		break;
	}
	case PacketId::Play::Serverbound::PLACE_RECIPE:
		handlePlaceRecipePacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::RECIPE_BOOK_CHANGE_SETTINGS:
		handleRecipeBookChangeSettingsPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::RECIPE_BOOK_SEEN_RECIPE:
		break; // Highlights aren't kept: every recipe is known from the start
	case PacketId::Play::Serverbound::PLAYER_INPUT: {
		// The keys held: forward, backward, left, right, jump, sneak (0x20), sprint
		uint8_t keys = packet->getData().readUByte();
		player->setShiftKeyDown(keys & 0x20);
		player->setForwardKeyDown(keys & 0x01);
		// Sneaking gets off what it rides (ServerPlayer.rideTick)
		if ((keys & 0x20) && player->vehicleId() != -1) {
			if (Entity* vehicle = server.levelOf(*player).entities().byId(player->vehicleId())) {
				vehicle->ejectRider();
			} else {
				player->setVehicleId(-1);
			}
		}
		break;
	}
	case PacketId::Play::Serverbound::PADDLE_BOAT: {
		// The paddles the rider moves (for the others to see)
		bool left = packet->getData().readBool(), right = packet->getData().readBool();
		if (auto* boat = dynamic_cast<Boat*>(server.levelOf(*player).entities().byId(player->vehicleId())); boat && boat->rider() == player) {
			boat->setPaddleState(left, right);
		}
		break;
	}
	case PacketId::Play::Serverbound::MOVE_VEHICLE: {
		// The rider's client moves the vehicle it controls
		Buffer& data = packet->getData();
		double	x = data.readDouble(), y = data.readDouble(), z = data.readDouble();
		float	yaw = data.readFloat(), pitch = data.readFloat();
		bool	onGround = data.readBool();
		if (player->vehicleId() == -1 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) break;
		Entity* vehicle = server.levelOf(*player).entities().byId(player->vehicleId());
		if (vehicle && vehicle->rider() == player && vehicle->isControlledByRider()) vehicle->moveFromRider({x, y, z}, yaw, pitch, onGround);
		break;
	}
	case PacketId::Play::Serverbound::PLAYER_COMMAND: {
		packet->getData().readVarInt(); // Entity id (always the player itself)
		int action = packet->getData().readVarInt();
		if (action == START_SPRINTING) player->setSprinting(true);
		if (action == STOP_SPRINTING) player->setSprinting(false);
		if (action == OPEN_INVENTORY && player->vehicleId() != -1) {
			// The inventory key while riding a horse: its inventory
			if (auto* horse = dynamic_cast<AbstractHorse*>(server.levelOf(*player).entities().byId(player->vehicleId()))) horse->openInventory(*player);
		}
		if (action == START_RIDING_JUMP && player->vehicleId() != -1) {
			int boost = packet->getData().readVarInt();
			auto* horse = dynamic_cast<AbstractHorse*>(server.levelOf(*player).entities().byId(player->vehicleId()));
			if (horse && horse->isSaddled() && boost > 0) horse->handleStartJump();
		}
		break;
	}
	case PacketId::Play::Serverbound::CLIENT_COMMAND:
		if (packet->getData().readVarInt() == CLIENT_COMMAND_RESPAWN) {
			// Back from the end credits: respawned in the overworld with everything kept, else after a death
			if (player->wonGame()) {
				player->setWonGame(false);
				Combat::respawn(server, *player, true);
			} else {
				Combat::respawn(server, *player, false);
			}
		}
		break;
	case PacketId::Play::Serverbound::CLIENT_INFORMATION:
		// Sent again when the player changes its settings (language...)
		handleClientInformationPacket(*packet, server);
		break;
	case PacketId::Play::Serverbound::CHAT: {
		// ServerboundChat: message, then its timestamp, salt and signature, which aren't verified
		handleChatLine(server, *player, packet->getData().readString(256));
		break;
	}
	case PacketId::Play::Serverbound::COMMAND_SUGGESTION: {
		int			transaction = packet->getData().readVarInt();
		std::string text		= packet->getData().readString(32500);
		Commands::suggest(server, *player, transaction, text);
		break;
	}
	case PacketId::Play::Serverbound::CHAT_COMMAND:
		// ServerboundChatCommand: the command alone, without the '/'
		Commands::run(server, *player, packet->getData().readString(32767));
		break;
	case PacketId::Play::Serverbound::SET_BEACON: {
		// ServerboundSetBeaconPacket: the optional primary and secondary powers (mob_effect registry ids)
		Buffer& data = packet->getData();
		auto	power = [&]() -> BeaconBlockEntity::Power {
			   if (!data.readBool()) return std::nullopt;
			   int id = data.readVarInt();
			   if (id < 0 || id >= static_cast<int>(MobEffects::Count)) return std::nullopt;
			   return static_cast<MobEffects::Effect>(id);
		};
		BeaconBlockEntity::Power primary   = power();
		BeaconBlockEntity::Power secondary = power();
		if (auto* menu = dynamic_cast<BeaconMenu*>(&Menus::current(*player, server.levelOf(*player)))) menu->updateEffects(primary, secondary);
		break;
	}
	case PacketId::Play::Serverbound::EDIT_BOOK: {
		// ServerboundEditBookPacket: slot, pages, optional title (signing)
		Buffer& data  = packet->getData();
		int		slot  = data.readVarInt();
		int		count = data.readVarInt();
		if (count < 0 || count > Books::MAX_PAGES) break;
		std::vector<std::string> pages;
		for (int i = 0; i < count; i++) pages.push_back(data.readString(Books::PAGE_LENGTH));
		std::optional<std::string> title;
		if (data.readBool()) title = data.readString(Books::TITLE_LENGTH);
		Books::handleEdit(server.levelOf(*player), *player, slot, pages, title);
		break;
	}
	case PacketId::Play::Serverbound::SIGN_UPDATE: {
		// ServerboundSignUpdatePacket: position, front side, the 4 lines
		Buffer& data = packet->getData();
		int32_t x, y, z;
		data.readPosition(x, y, z);
		bool					   front = data.readBool();
		std::array<std::string, 4> lines;
		for (std::string& line : lines) line = data.readString(384);
		Signs::handleUpdate(server.levelOf(*player), *player, {x, y, z}, front, lines);
		break;
	}
	case PacketId::Play::Serverbound::CHAT_COMMAND_SIGNED: {
		// ServerboundChatCommandSigned: the command, then its signature (ignored)
		Commands::run(server, *player, packet->getData().readString(32767));
		break;
	}
	case PacketId::Play::Serverbound::CHAT_ACK:
		packet->getData().readVarInt(); // Message count: no chat tracking yet
		break;
	case PacketId::Play::Serverbound::CHAT_SESSION_UPDATE:
		break; // The client's session: no signed chat
	case PacketId::Play::Serverbound::PLAYER_LOADED:
		g_logger->logNetwork(DEBUG, "Player fully loaded in game", "Play");
		break;
	default:
		break; // Not handled yet (see docs/PACKETS_MISSING.md)
	}
}