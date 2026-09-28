#include "world/blockentity/ContainerEntities.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Shapes.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/inventory/Menu.hpp"

#include <algorithm>
#include <cmath>

namespace {
	int step(Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; }
	// A property of the block at the entity's position
	int propertyValue(Level& level, const BlockPos& pos, const char* property) {
		return level.blocks().get(level.getBlockState(pos), level.blocks().property(property));
	}
	Direction facingAt(Level& level, const BlockPos& pos) {
		int value = propertyValue(level, pos, "facing");
		for (Direction direction : Directions::ALL) {
			if (level.blocks().valueName(value) == std::string(std::array<const char*, 6>{"down", "up", "north", "south", "west", "east"}[static_cast<int>(direction)])) {
				return direction;
			}
		}
		return Direction::North;
	}
	float openPitch(Level& level) { return level.random().nextFloat() * 0.1F + 0.9F; }

	// The player's open menu shows this container (or a double chest with it)
	bool menuShows(Player& player, const Container* container) {
		if (!player.openMenuSlot()) return false;
		for (const Slot& slot : player.openMenuSlot()->slots()) {
			if (slot.container->contains(container)) return true;
		}
		return false;
	}
} // namespace

// ===== Chests, barrels, shulker boxes, ender chests =====

// ----- Chests -----

bool isChestBlocked(Level& level, const BlockPos& pos) { return level.isRedstoneConductor(level.getBlockState(pos.above())); }

std::shared_ptr<BlockEntity> chestPartner(Level& level, const BlockPos& pos, bool ignoreBlocked, bool& first, bool* partnerBlocked) {
	const BlockRegistry& blocks = level.blocks();
	int					 state	= level.getBlockState(pos);
	int					 typeId = blocks.property("type");
	std::string			 type	= blocks.valueName(blocks.get(state, typeId));
	if (type == "single" || (!ignoreBlocked && isChestBlocked(level, pos))) return nullptr;
	// getConnectedDirection: clockwise of the facing for the left half, counterclockwise for the right one
	Direction facing = facingAt(level, pos);
	auto	  turn	 = [](Direction d, bool clockwise) {
		   constexpr Direction order[4] = {Direction::North, Direction::East, Direction::South, Direction::West};
		   int				   i		= 0;
		   while (order[i] != d) i++;
		   return order[(i + (clockwise ? 1 : 3)) % 4];
	};
	BlockPos otherPos = pos.relative(turn(facing, type == "left"));
	int		 other	  = level.getBlockState(otherPos);
	if (blocks.blockOf(other) != blocks.blockOf(state)) return nullptr;
	std::string otherType = blocks.valueName(blocks.get(other, typeId));
	if (otherType == "single" || otherType == type || facingAt(level, otherPos) != facing) return nullptr;
	if (!ignoreBlocked && isChestBlocked(level, otherPos)) {
		// combineWithNeigbour: a blocked half blocks the whole double chest
		if (partnerBlocked) *partnerBlocked = true;
		return nullptr;
	}
	first = type == "right";
	return level.getSharedBlockEntity(otherPos);
}

void ChestBlockEntity::startOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.increment(*level(), pos(), *this, blockInteractionRange(player));
}

void ChestBlockEntity::stopOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.decrement(*level(), pos(), *this);
}

// ChestBlockEntity.playSound: from the middle of a double chest (the left half stays quiet)
void ChestBlockEntity::playSound(Level& level, bool open) {
	int			state = level.getBlockState(pos());
	std::string type  = level.blocks().valueName(level.blocks().get(state, level.blocks().property("type")));
	if (type == "left") return;
	std::string name  = level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(state));
	std::string sound = "minecraft:block.chest";
	if (name.find("copper_chest") != std::string::npos) {
		sound = name.find("oxidized") != std::string::npos	  ? "minecraft:block.copper_chest_oxidized"
				: name.find("weathered") != std::string::npos ? "minecraft:block.copper_chest_weathered"
															  : "minecraft:block.copper_chest";
	}
	double x = pos().x + 0.5, y = pos().y + 0.5, z = pos().z + 0.5;
	if (type == "right") {
		// getConnectedDirection: counterclockwise of the facing for the right half
		Direction facing	= facingAt(level, pos());
		Direction connected = facing == Direction::North ? Direction::West
							  : facing == Direction::West ? Direction::South
							  : facing == Direction::South ? Direction::East
														   : Direction::North;
		x += step(connected, 0) * 0.5;
		z += step(connected, 2) * 0.5;
	}
	level.playSoundAt(nullptr, x, y, z, sound + (open ? ".open" : ".close"), Level::SoundSource::Blocks, 0.5F, openPitch(level));
}

void ChestBlockEntity::onOpen(Level& level) { playSound(level, true); }
void ChestBlockEntity::onClose(Level& level) { playSound(level, false); }

// signalOpenCount: the lid (a block event for the clients); a trapped chest also powers what is around and below
void ChestBlockEntity::openerCountChanged(Level& level, int before, int now) {
	int block = level.blocks().blockOf(level.getBlockState(pos()));
	level.blockEvent(pos(), block, 1, now);
	if (type() == "minecraft:trapped_chest" && before != now) {
		level.updateNeighborsAt(pos(), block);
		level.updateNeighborsAt(pos().below(), block);
	}
}

bool ChestBlockEntity::isOwnContainer(Player& player) { return menuShows(player, this); }

// ----- Barrel -----

void BarrelBlockEntity::startOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.increment(*level(), pos(), *this, blockInteractionRange(player));
}

void BarrelBlockEntity::stopOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.decrement(*level(), pos(), *this);
}

void BarrelBlockEntity::setOpen(Level& level, bool open) {
	// The sound, from the side it faces
	Direction facing = facingAt(level, pos());
	double	  x		 = pos().x + 0.5 + step(facing, 0) / 2.0;
	double	  y		 = pos().y + 0.5 + step(facing, 1) / 2.0;
	double	  z		 = pos().z + 0.5 + step(facing, 2) / 2.0;
	level.playSoundAt(nullptr, x, y, z, open ? "minecraft:block.barrel.open" : "minecraft:block.barrel.close", Level::SoundSource::Blocks, 0.5F,
					  openPitch(level));
	int state = level.getBlockState(pos());
	level.setBlock(pos(), level.blocks().withBool(state, level.blocks().property("open"), open), Level::UPDATE_ALL);
}

void BarrelBlockEntity::onOpen(Level& level) { setOpen(level, true); }
void BarrelBlockEntity::onClose(Level& level) { setOpen(level, false); }
bool BarrelBlockEntity::isOwnContainer(Player& player) { return menuShows(player, this); }

// ----- Shulker box -----

bool ShulkerBoxBlockEntity::isShulkerBoxItem(const ItemStack& stack) const {
	// Item.canFitInsideContainerItems: not a shulker box (the block the item places)
	if (!level()) return false;
	const GameData& data  = level()->gameData();
	int				state = data.getPlacedBlockState(stack.item);
	return state >= 0 && data.isInstanceOf(data.getBlocks().blockOf(state), "ShulkerBoxBlock");
}

bool ShulkerBoxBlockEntity::canPlaceItemThroughFace(int, const ItemStack& stack, const Direction*) { return !isShulkerBoxItem(stack); }

void ShulkerBoxBlockEntity::startOpen(Player& player) {
	if (isRemoved() || player.getGameMode() == GameMode::Spectator || !level()) return;
	Level& at = *level();
	if (_openCount < 0) _openCount = 0;
	_openCount++;
	at.blockEvent(pos(), at.blocks().blockOf(at.getBlockState(pos())), 1, _openCount);
	if (_openCount == 1) {
		at.playSound(nullptr, pos(), "minecraft:block.shulker_box.open", Level::SoundSource::Blocks, 0.5F, openPitch(at));
	}
}

void ShulkerBoxBlockEntity::stopOpen(Player& player) {
	if (isRemoved() || player.getGameMode() == GameMode::Spectator || !level()) return;
	Level& at = *level();
	_openCount--;
	at.blockEvent(pos(), at.blocks().blockOf(at.getBlockState(pos())), 1, _openCount);
	if (_openCount <= 0) {
		at.playSound(nullptr, pos(), "minecraft:block.shulker_box.close", Level::SoundSource::Blocks, 0.5F, openPitch(at));
	}
}

// triggerEvent: the lid starts moving when the block event runs (on the server too), at the end of the tick
bool ShulkerBoxBlockEntity::triggerEvent(int type, int data) {
	if (type != 1) return false;
	_openCount = data;
	if (data == 0) _animation = Animation::Closing;
	if (data == 1) _animation = Animation::Opening;
	return true;
}

void ShulkerBoxBlockEntity::tick(Level& level) {
	auto neighborUpdates = [&] {
		int state = level.getBlockState(pos());
		level.updateNeighbourShapes(pos(), state, Level::UPDATE_ALL);
		level.updateNeighborsAt(pos(), level.blocks().blockOf(state));
	};
	_progressOld = _progress;
	switch (_animation) {
	case Animation::Closed:
		_progress = 0.0F;
		break;
	case Animation::Opening: {
		_progress += 0.1F;
		if (_progressOld == 0.0F) neighborUpdates();
		if (_progress >= 1.0F) {
			_animation = Animation::Opened;
			_progress  = 1.0F;
			neighborUpdates();
		}
		// moveCollidedEntities: what is in the way of the lid is pushed along
		Direction facing = facingAt(level, pos());
		// Like vanilla: the raw progress (0 to 1), not the lid's height
		double	  from = _progressOld, to = _progress;
		double	  low = std::min(from, to), high = std::max(from, to);
		// Shulker.getProgressDeltaAabb: the slice the lid sweeps this tick, above the box in its facing
		AABB delta{pos().x + 0.0, pos().y + 0.0, pos().z + 0.0, pos().x + 1.0, pos().y + 1.0, pos().z + 1.0};
		for (int axis = 0; axis < 3; axis++) {
			int s = step(facing, axis);
			if (s == 0) continue;
			double base = axis == 0 ? pos().x : axis == 1 ? pos().y : pos().z;
			double lo	= s > 0 ? base + 1.0 + low : base - high;
			double hi	= s > 0 ? base + 1.0 + high : base - low;
			if (axis == 0) delta.minX = lo, delta.maxX = hi;
			if (axis == 1) delta.minY = lo, delta.maxY = hi;
			if (axis == 2) delta.minZ = lo, delta.maxZ = hi;
		}
		for (Entity* entity : level.entities().entitiesIn(delta)) {
			double sx = delta.maxX - delta.minX + 0.01, sy = delta.maxY - delta.minY + 0.01, sz = delta.maxZ - delta.minZ + 0.01;
			entity->moveByShulker({sx * step(facing, 0), sy * step(facing, 1), sz * step(facing, 2)});
		}
		break;
	}
	case Animation::Opened:
		_progress = 1.0F;
		break;
	case Animation::Closing:
		_progress -= 0.1F;
		if (_progressOld == 1.0F) neighborUpdates();
		if (_progress <= 0.0F) {
			_animation = Animation::Closed;
			_progress  = 0.0F;
			neighborUpdates();
		}
		break;
	}
}

// ----- Ender chest -----

void EnderChestBlockEntity::startOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.increment(*level(), pos(), *this, blockInteractionRange(player));
}

void EnderChestBlockEntity::stopOpen(Player& player) {
	if (!isRemoved() && player.getGameMode() != GameMode::Spectator && level()) _openers.decrement(*level(), pos(), *this);
}

bool EnderChestBlockEntity::stillValid(Player& player) {
	Level* at = level();
	if (!at || isRemoved() || at->getBlockEntity(pos()) != this) return false;
	double range = (player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 4.0;
	double dx = pos().x + 0.5 - player.getX(), dy = pos().y + 0.5 - (player.getY() + 1.62), dz = pos().z + 0.5 - player.getZ();
	return dx * dx + dy * dy + dz * dz < (range + 0.87) * (range + 0.87);
}

void EnderChestBlockEntity::onOpen(Level& level) {
	level.playSound(nullptr, pos(), "minecraft:block.ender_chest.open", Level::SoundSource::Blocks, 0.5F, openPitch(level));
}

void EnderChestBlockEntity::onClose(Level& level) {
	level.playSound(nullptr, pos(), "minecraft:block.ender_chest.close", Level::SoundSource::Blocks, 0.5F, openPitch(level));
}

void EnderChestBlockEntity::openerCountChanged(Level& level, int, int now) {
	level.blockEvent(pos(), level.blocks().blockOf(level.getBlockState(pos())), 1, now);
}

bool EnderChestBlockEntity::isOwnContainer(Player& player) {
	if (!player.openMenuSlot()) return false;
	for (const Slot& slot : player.openMenuSlot()->slots()) {
		if (auto* ender = dynamic_cast<EnderChestContainer*>(slot.container); ender && ender->chest() == this) return true;
	}
	return false;
}

EnderChestContainer::EnderChestContainer(Player& player, std::shared_ptr<BlockEntity> chest)
	: _player(player), _keepAlive(std::move(chest)), _chest(dynamic_cast<EnderChestBlockEntity*>(_keepAlive.get())) {}

ItemStack& EnderChestContainer::item(int slot) { return _player.enderChest().at(slot); }

