#include "BlockPos.hpp"
#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "item/ItemStack.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/blocks/Containers.hpp"
#include "world/inventory/Menu.hpp"
#include "world/item/Components.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

// Block breaking and placing. The client changes the block right away (prediction) and sends a sequence number;
// the server applies the change through Level (players get it at the end of the tick), then acknowledges the
// sequence after it. When it refuses, it sends the real block first so the client undoes its prediction.

namespace {
	enum PlayerActionStatus { START_DIGGING = 0, CANCEL_DIGGING = 1, FINISH_DIGGING = 2, DROP_ALL_ITEMS = 3, DROP_ITEM = 4, SWAP_HANDS = 6 };
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

	void sendBlockUpdate(const std::shared_ptr<Player>& player, Server& server, int x, int y, int z, int state) {
		Buffer buf;
		buf.writePosition(x, y, z);
		buf.writeVarInt(state);
		Packet::send(player, PacketId::Play::Clientbound::BLOCK_UPDATE, buf, server);
	}


	// Vanilla allows 4.5 blocks in survival, 5 in creative, plus a margin for latency
	bool inReach(const Player& player, int x, int y, int z) {
		double range = (player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 1.0;
		double dx	 = x + 0.5 - player.getX();
		double dy	 = y + 0.5 - (player.getY() + 1.62); // Eye height
		double dz	 = z + 0.5 - player.getZ();
		return dx * dx + dy * dy + dz * dz <= (range + 0.87) * (range + 0.87); // + half the block's diagonal
	}

	// Player.hasCorrectToolForDrops
	bool hasCorrectToolForDrops(Player& player, const GameData& gameData, int state) {
		return !gameData.getStateProperties(state).requiresCorrectTool || gameData.isCorrectToolForDrops(player.getItemInHand(0), state);
	}

	// DoublePlantBlock.preventDropFromBottomPart and BedBlock.playerWillDestroy: breaking the top of a door or tall plant
	// (or the foot of a bed) without drops removes the other half without drops too
	void preventDropFromOtherPart(Player& player, Level& level, const GameData& gameData, const BlockPos& pos, int state) {
		const BlockRegistry& blocks = level.blocks();
		int					 block	= blocks.blockOf(state);
		BlockPos			 other;
		if (gameData.isInstanceOf(block, "DoorBlock") || gameData.isInstanceOf(block, "DoublePlantBlock")) {
			if (blocks.get(state, blocks.property("half")) != blocks.value("upper")) return;
			other = pos.below();
			int otherState = level.getBlockState(other);
			if (blocks.blockOf(otherState) != block || blocks.get(otherState, blocks.property("half")) != blocks.value("lower")) return;
		} else if (gameData.isInstanceOf(block, "BedBlock") && player.getGameMode() == GameMode::Creative) {
			if (blocks.get(state, blocks.property("part")) != blocks.value("foot")) return;
			std::string facing = blocks.valueName(blocks.get(state, blocks.property("facing")));
			int			direction = facing == "north" ? 0 : facing == "south" ? 1 : facing == "west" ? 2 : 3;
			const int	offsets[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
			other = {pos.x + offsets[direction][0], pos.y, pos.z + offsets[direction][1]};
			int otherState = level.getBlockState(other);
			if (blocks.blockOf(otherState) != block || blocks.get(otherState, blocks.property("part")) != blocks.value("head")) return;
		} else {
			return;
		}
		int otherState = level.getBlockState(other);
		level.setBlock(other, level.fluidLegacyBlock(otherState), Level::UPDATE_ALL | Level::UPDATE_SUPPRESS_DROPS);
		level.levelEvent(&player, LEVEL_EVENT_BLOCK_BREAK, other, otherState);
	}

	// Returns false if the block can't be broken (then nothing changed). Like vanilla's ServerPlayerGameMode.destroyBlock
	bool breakBlock(Player& player, Server& server, int x, int y, int z) {
		Level&			level	 = server.getLevel();
		const GameData& gameData = server.getGameData();
		BlockPos		pos{x, y, z};
		if (!level.hasChunkAt(pos)) return false;
		int state = level.getBlockState(pos);
		if (level.blocks().isAir(state)) return false;
		bool creative = player.getGameMode() == GameMode::Creative;
		// Item.canDestroyBlock: a tool that can't break blocks in creative (swords, the mace, the trident)
		const GameData::ItemProperties* held = gameData.getItemProperties(player.getStackInHand(0).item);
		if (creative && held && !held->canDestroyBlocksInCreative) return false;
		if (!creative && gameData.getDestroyTime(state) < 0) return false; // Bedrock...

		// Block.playerWillDestroy: particles and sound for the others (the breaker's client played them already)
		bool canHarvest = hasCorrectToolForDrops(player, gameData, state);
		level.levelEvent(&player, LEVEL_EVENT_BLOCK_BREAK, pos, state);
		if (creative || !canHarvest) preventDropFromOtherPart(player, level, gameData, pos, state);

		ItemStack tool	  = player.getStackInHand(0);
		bool	  removed = level.removeBlock(pos, false);
		// Block.playerDestroy: what it drops, with the tool used (the other half of a door follows through updateShape)
		if (removed && !creative && canHarvest) {
			level.dropResources(state, pos, &player, &tool);
			// IceBlock.playerDestroy: water stays, over something solid or liquid (silk touch isn't known yet)
			if (gameData.isInstanceOf(level.blocks().blockOf(state), "IceBlock") && !level.isUltraWarm()) {
				const GameData::StateProperties& below = gameData.getStateProperties(level.getBlockState(pos.below()));
				if (below.blocksMotion || below.liquid) level.setBlock(pos, gameData.getDefaultBlockState("minecraft:water"), Level::UPDATE_ALL);
			}
		}
		return removed;
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
	Level&	 level	  = server.getLevel();

	// Actions that aren't about a block: no acknowledgment
	if (status == DROP_ITEM || status == DROP_ALL_ITEMS) {
		if (player.getGameMode() == GameMode::Spectator) return;
		// ServerPlayer.drop: the client removed it already
		int		  slot	= player.handSlot(0);
		ItemStack held	= player.inventory().get(slot);
		if (held.isEmpty()) return;
		int		  count = status == DROP_ALL_ITEMS ? held.count : 1;
		ItemStack thrown = held.copyWithCount(count);
		held.shrink(count);
		player.inventory().set(slot, held.isEmpty() ? ItemStack() : held);
		// The client knows (setRemoteSlot of the open menu's slot for it)
		Menu& menu = Menus::current(player, level);
		for (const Slot& menuSlot : menu.slots()) {
			if (dynamic_cast<PlayerContainer*>(menuSlot.container) && PlayerInventory::windowSlot(menuSlot.containerSlot) == slot) {
				menu.setRemoteSlot(menuSlot.index, player.inventory().get(slot));
			}
		}
		level.dropFromPlayer(player, std::move(thrown), true);
		return;
	}
	if (status == SWAP_HANDS) {
		if (player.getGameMode() == GameMode::Spectator) return;
		ItemStack offhand = player.inventory().get(player.handSlot(1));
		player.inventory().set(player.handSlot(1), player.inventory().get(player.handSlot(0)));
		player.inventory().set(player.handSlot(0), std::move(offhand));
		return;
	}

	bool canBuild = player.getGameMode() == GameMode::Survival || player.getGameMode() == GameMode::Creative;
	bool refused  = false;

	if (status == CANCEL_DIGGING) {
		player.stopDigging();
	} else if (status == START_DIGGING) {
		int state = level.getBlockState({x, y, z});
		if (!canBuild || !inReach(player, x, y, z) || !level.hasChunkAt({x, y, z})) {
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

	if (refused && level.hasChunkAt({x, y, z})) sendBlockUpdate(self, server, x, y, z, level.getBlockState({x, y, z})); // Undo the prediction
	player.acknowledgeBlockChanges(sequence);
}

void handleUseItemOnPacket(Packet& packet, Server& server) {
	Buffer& data = packet.getData();
	int		hand = data.readVarInt();
	int32_t x, y, z;
	data.readPosition(x, y, z);
	int	  face	  = data.readVarInt();
	float cursorX = data.readFloat(); // Where the face was hit, in the block
	float cursorY = data.readFloat();
	float cursorZ = data.readFloat();
	data.readBool(); // Inside block
	data.readBool(); // World border hit
	int sequence = data.readVarInt();

	Player&			player	 = *packet.getPlayer();
	auto			self	 = player.shared_from_this();
	const GameData& gameData = server.getGameData();
	Level&			level	 = server.getLevel();
	BlockPos		hit{x, y, z};
	if (face < 0 || face > 5 || player.getGameMode() == GameMode::Spectator || !level.hasChunkAt(hit) || !inReach(player, x, y, z)) {
		player.acknowledgeBlockChanges(sequence);
		return;
	}

	// ServerPlayerGameMode.useItemOn: the block first (levers, doors...), unless sneaking with something in a hand
	int	 item	  = player.getItemInHand(hand);
	bool sneaking = player.isShiftKeyDown() && (player.getItemInHand(0) != 0 || player.getItemInHand(1) != 0);
	if (!sneaking && hand == 0) {
		int state = level.getBlockState(hit);
		if (level.behavior(state).useWithoutItem(level, hit, state, player)) {
			player.acknowledgeBlockChanges(sequence);
			return;
		}
	}

	// BlockItem.place, only in survival and creative (adventure mode can't build)
	int placed = gameData.getPlacedBlockState(item);
	if (placed < 0 || player.getGameMode() == GameMode::Adventure) {
		player.acknowledgeBlockChanges(sequence);
		return;
	}
	int placedBlock = level.blocks().blockOf(placed);
	// canBeReplaced: a replaceable block (tall grass, snow layer...), not of the kind being placed
	auto replaceable = [&](const BlockPos& pos) {
		if (!level.hasChunkAt(pos) || level.isOutsideBuildHeight(pos.y)) return false;
		int state = level.getBlockState(pos);
		return gameData.getStateProperties(state).replaceable && level.blocks().blockOf(state) != placedBlock;
	};
	PlaceContext context{};
	context.clickedFace	   = static_cast<Direction>(face);
	context.replaceClicked = replaceable(hit);
	context.clickedPos	   = context.replaceClicked ? hit : hit.relative(context.clickedFace);
	context.clickX		   = x + cursorX;
	context.clickY		   = y + cursorY;
	context.clickZ		   = z + cursorZ;
	context.yaw			   = player.getYaw();
	context.pitch		   = player.getPitch();
	context.secondaryUse   = player.isShiftKeyDown();
	context.item		   = item;
	context.block		   = placedBlock;
	BlockPos target		   = context.clickedPos;

	std::vector<BlockChange> parts;
	bool					 fits = context.replaceClicked || replaceable(target);
	if (fits) {
		int state = level.behaviors().placer(placedBlock).getStateForPlacement(level, context);
		if (state == GENERIC_PLACEMENT) {
			// No placement rule ported for this block: axis from the face, facing and second half from the player
			int oriented = gameData.withProperty(placed, "axis", FACE_AXES[face]);
			parts		 = placementParts(gameData, player, target.x, target.y, target.z, oriented >= 0 ? oriented : placed);
		} else if (state >= 0) {
			parts = {{target.x, target.y, target.z, state}};
		}
		fits = !parts.empty();
		for (size_t i = 0; i < parts.size() && fits; i++) {
			BlockPos pos{parts[i].x, parts[i].y, parts[i].z};
			fits = parts[i].state >= 0 && (i == 0 || replaceable(pos)) && level.behavior(parts[i].state).canSurvive(level, pos, parts[i].state);
		}
	}
	if (!fits) {
		// Undo the client's prediction
		sendBlockUpdate(self, server, target.x, target.y, target.z, level.hasChunkAt(target) ? level.getBlockState(target) : 0);
		player.acknowledgeBlockChanges(sequence);
		return;
	}
	// Like vanilla: the block itself with UPDATE_ALL_IMMEDIATE (BlockItem.place), its other half with UPDATE_ALL
	for (size_t i = 0; i < parts.size(); i++) {
		level.setBlock({parts[i].x, parts[i].y, parts[i].z}, parts[i].state, i == 0 ? Level::UPDATE_ALL_IMMEDIATE : Level::UPDATE_ALL);
	}
	int state = level.getBlockState(target);
	if (state == parts[0].state) level.behavior(state).setPlacedBy(level, target, state);
	// One item used, except with infinite materials
	if (player.getGameMode() != GameMode::Creative) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
	player.acknowledgeBlockChanges(sequence);
}

void handleSetCarriedItemPacket(Packet& packet, Server& server) {
	int slot = packet.getData().readShort();
	if (slot >= 0 && slot <= 8) packet.getPlayer()->setSelectedSlot(slot);
	(void)server;
}

// Creative inventory: the client tells the server what it put in each slot (ServerGamePacketListenerImpl.handleSetCreativeModeSlot)
void handleSetCreativeModeSlotPacket(Packet& packet, Server& server) {
	Buffer&	  data	 = packet.getData();
	int		  slot	 = data.readShort();
	ItemStack stack	 = ItemStack::readLast(data);
	Player&	  player = *packet.getPlayer();
	if (player.getGameMode() != GameMode::Creative) return;
	const GameData::ItemProperties* item	 = server.getGameData().getItemProperties(stack.item);
	bool							validSize = stack.isEmpty() || (item && stack.count <= item->maxStackSize);
	if (slot >= 1 && slot <= 45 && validSize) {
		// The client has it already
		Menu& menu = Menus::inventory(player, server.getLevel());
		menu.slots()[slot].set(stack);
		menu.setRemoteSlot(slot, stack);
		menu.broadcastChanges();
	} else if (slot < 0 && validSize && !stack.isEmpty()) {
		server.getLevel().dropFromPlayer(player, std::move(stack), true);
	}
}

namespace {
	// ItemStack.isEnchanted: a non-empty minecraft:enchantments (its encoding starts with the entry count)
	bool isEnchanted(const ItemStack& stack, const GameData& gd) {
		std::optional<std::vector<uint8_t>> enchantments = Components::get(stack, gd, "minecraft:enchantments");
		return enchantments && !enchantments->empty() && (*enchantments)[0] != 0;
	}

	// Inventory.getSuitableHotbarSlot: the first empty hotbar slot from the selected one, else the first not enchanted
	int suitableHotbarSlot(const Player& player, const GameData& gd) {
		const PlayerInventory& inv		= player.inventory();
		int					   selected = player.getSelectedSlot();
		for (int i = 0; i < 9; i++) {
			int slot = (selected + i) % 9;
			if (inv.get(PlayerInventory::HOTBAR + slot).isEmpty()) return slot;
		}
		for (int i = 0; i < 9; i++) {
			int slot = (selected + i) % 9;
			if (!isEnchanted(inv.get(PlayerInventory::HOTBAR + slot), gd)) return slot;
		}
		return selected;
	}
} // namespace

// ServerGamePacketListenerImpl.handlePickItemFromBlock + tryPickItem
void handlePickItemFromBlock(Packet& packet, Server& server) {
	Buffer&	 data = packet.getData();
	BlockPos pos;
	data.readPosition(pos.x, pos.y, pos.z);
	bool includeData = data.readBool();

	Level&			level  = server.getLevel();
	const GameData& gd	   = level.gameData();
	Player&			player = *packet.getPlayer();
	bool			infinite = player.getGameMode() == GameMode::Creative; // hasInfiniteMaterials

	// Player.canInteractWithBlock(pos, 1.0): the block's box within the interaction range + 1
	double range = (infinite ? 5.0 : 4.5) + 1.0;
	double eyeY	 = player.getY() + 1.62;
	double dx	 = std::max({pos.x - player.getX(), 0.0, player.getX() - (pos.x + 1.0)});
	double dy	 = std::max({pos.y - eyeY, 0.0, eyeY - (pos.y + 1.0)});
	double dz	 = std::max({pos.z - player.getZ(), 0.0, player.getZ() - (pos.z + 1.0)});
	if (dx * dx + dy * dy + dz * dz >= range * range) return;

	// Block.getCloneItemStack: the block's item (same name), empty if it has none
	int				   state = level.getBlockState(pos);
	const std::string& name	 = gd.getStaticName("minecraft:block", gd.getBlockOfState(state));
	int				   item	 = gd.getStaticId("minecraft:item", name);
	if (item <= 0) return;
	ItemStack picked(item, 1);
	if (infinite && includeData) {
		// addBlockDataToItem: the components its block entity gives (contents, name)
		if (BlockEntity* entity = level.getBlockEntity(pos)) ContainerItems::collect(*entity, picked, gd);
	}

	PlayerInventory& inv = player.inventory();
	// Inventory.findSlotMatchingItem: hotbar first, then the main inventory (vanilla indices 0-35)
	int found = -1;
	for (int i = 0; i < 36 && found < 0; i++) {
		const ItemStack& stack = inv.get(PlayerInventory::windowSlot(i));
		if (!stack.isEmpty() && stack.sameItemSameComponents(picked)) found = i;
	}
	if (found >= 0 && found < 9) {
		player.setSelectedSlot(found);
	} else if (found >= 0) {
		// Inventory.pickSlot: swapped with a suitable hotbar slot, which becomes the selected one
		player.setSelectedSlot(suitableHotbarSlot(player, gd));
		int		  hand	 = player.handSlot(0);
		int		  other	 = PlayerInventory::windowSlot(found);
		ItemStack inHand = inv.get(hand);
		inv.set(hand, inv.get(other));
		inv.set(other, std::move(inHand));
	} else if (infinite) {
		// Inventory.addAndPickItem: into a suitable hotbar slot, what was there moves to the first empty slot
		player.setSelectedSlot(suitableHotbarSlot(player, gd));
		int hand = player.handSlot(0);
		if (!inv.get(hand).isEmpty()) {
			for (int i = 0; i < 36; i++) {
				int slot = PlayerInventory::windowSlot(i);
				if (inv.get(slot).isEmpty()) {
					inv.set(slot, inv.get(hand));
					break;
				}
			}
		}
		inv.set(hand, std::move(picked));
	}
	setHeldItemPacket(packet, server);
	Menus::inventory(player, level).broadcastChanges();
}
