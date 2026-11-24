#include "world/blocks/Signs.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/blockentity/SignBlockEntity.hpp"

#include <cmath>

namespace {
	constexpr const char* DIRECTION_NAMES[6] = {"down", "up", "north", "south", "west", "east"};

	Direction directionNamed(const std::string& name) {
		for (int i = 0; i < 6; i++) {
			if (name == DIRECTION_NAMES[i]) return static_cast<Direction>(i);
		}
		return Direction::North;
	}

	bool isHorizontal(Direction direction) { return direction != Direction::Up && direction != Direction::Down; }
	bool sameAxis(Direction a, Direction b) { return a == b || a == Directions::opposite(b); }

	// RotationSegment.convertToSegment: 16 steps of 22.5 degrees
	int rotationSegment(float degrees) { return static_cast<int>(std::floor(degrees / 22.5F + 0.5F)) & 15; }

	// The sign's other variant: "minecraft:oak_sign" <-> "minecraft:oak_wall_sign", "minecraft:oak_hanging_sign" <->
	// "minecraft:oak_wall_hanging_sign"
	std::string wallVariant(const std::string& name) {
		size_t at = name.rfind("_hanging_sign");
		if (at == std::string::npos) at = name.rfind("_sign");
		return name.substr(0, at) + "_wall" + name.substr(at);
	}

	float wrapDegrees(float degrees) {
		degrees = std::fmod(degrees, 360.0F);
		if (degrees >= 180.0F) degrees -= 360.0F;
		if (degrees < -180.0F) degrees += 360.0F;
		return degrees;
	}

	SignBlockEntity* signAt(Level& level, const BlockPos& pos) { return dynamic_cast<SignBlockEntity*>(level.getBlockEntity(pos)); }

	// SignApplicator: what an item in hand does to a side of the sign (dyes, glow ink sac, ink sac, honeycomb). Whether
	// it applied (the item is used)
	bool applyItem(Level& level, Player& player, SignBlockEntity& sign, bool front, const std::string& item) {
		SignText& text = sign.text(front);
		if (item == "minecraft:honeycomb") {
			sign.setWaxed(true);
			level.levelEvent(nullptr, 3003, sign.pos(), 0); // LevelEvent.PARTICLES_AND_SOUND_WAX_ON
			return true;
		}
		if (!text.hasMessage()) return false; // Dyes and inks only go on text
		if (item == "minecraft:glow_ink_sac") {
			if (text.glowingText) return false;
			text.glowingText = true;
			level.playSound(nullptr, sign.pos(), "minecraft:item.glow_ink_sac.use", Level::SoundSource::Blocks);
		} else if (item == "minecraft:ink_sac") {
			if (!text.glowingText) return false;
			text.glowingText = false;
			level.playSound(nullptr, sign.pos(), "minecraft:item.ink_sac.use", Level::SoundSource::Blocks);
		} else if (item.size() > 14 && item.compare(item.size() - 4, 4, "_dye") == 0) {
			std::string color = item.substr(10, item.size() - 14); // "minecraft:" ... "_dye"
			if (text.color == color) return false;
			text.color = color;
			level.playSound(nullptr, sign.pos(), "minecraft:item.dye.use", Level::SoundSource::Blocks);
		} else {
			return false;
		}
		(void)player;
		sign.setChanged();
		return true;
	}

	// SignBlock.otherPlayerIsEditingSign
	bool otherPlayerIsEditing(Level& level, const Player& player, const SignBlockEntity& sign) {
		if (!sign.playerWhoMayEdit() || *sign.playerWhoMayEdit() == player.getUUID()) return false;
		for (const auto& other : level.players()) {
			if (other->getUUID() == *sign.playerWhoMayEdit() && !other->isDisconnected()) {
				double dx = other->getX() - (sign.pos().x + 0.5), dy = other->getY() - (sign.pos().y + 0.5), dz = other->getZ() - (sign.pos().z + 0.5);
				return dx * dx + dy * dy + dz * dz < 64.0; // Still near it: its editor is still open
			}
		}
		return false;
	}

	// SignBlock.use without an applicable item: edit the side the player faces, unless waxed or someone else edits it
	bool tryEdit(Level& level, Player& player, SignBlockEntity& sign) {
		if (sign.isWaxed()) {
			level.playSound(nullptr, sign.pos(), "minecraft:block.sign.waxed_interact_fail", Level::SoundSource::Blocks);
			return true;
		}
		if (otherPlayerIsEditing(level, player, sign)) return true;
		Signs::openTextEdit(level, player, sign, Signs::isFacingFrontText(level, sign.pos(), player));
		return true;
	}
} // namespace

// ----- Placement -----

int SignBlock::finish(Level& level, const BlockPos& pos, int state) const {
	// The variant's own rule (this behavior is the floor or ceiling block's; the state may be the wall one)
	if (state < 0 || !level.behavior(state).canSurvive(level, pos, state)) return -1;
	const GameData& data  = _context->data;
	bool			water = level.fluids().isWater(level.getFluidState(pos).type) && level.getFluidState(pos).amount == 8;
	int				wet	  = data.withProperty(state, "waterlogged", water ? "true" : "false");
	return wet >= 0 ? wet : state;
}

// StandingSignBlock.getStateForPlacement: turned to face the player, 16 steps
int SignBlock::placeStanding(Level& level, const PlaceContext& context, int standing) const {
	int state = _context->data.withProperty(standing, "rotation", std::to_string(rotationSegment(context.yaw + 180.0F)));
	return finish(level, context.clickedPos, state);
}

// WallSignBlock.getStateForPlacement: on the wall the player looks at
int SignBlock::placeWall(Level& level, const PlaceContext& context, int wall) const {
	for (Direction direction : context.nearestLookingDirections()) {
		if (!isHorizontal(direction)) continue;
		int state = _context->data.withProperty(wall, "facing", DIRECTION_NAMES[static_cast<int>(Directions::opposite(direction))]);
		if (int placed = finish(level, context.clickedPos, state); placed >= 0) return placed;
	}
	return -1;
}

// CeilingHangingSignBlock.getStateForPlacement: on chains (attached, any of 16 rotations) under a block that isn't
// full underneath or when sneaking; otherwise on a bracket, facing the player
int SignBlock::placeCeiling(Level& level, const PlaceContext& context, int ceiling) const {
	const GameData& data	 = _context->data;
	int				above	 = level.getBlockState(context.clickedPos.relative(Direction::Up));
	bool			attached = !_context->isFaceSturdy(above, Direction::Down, GameData::SupportType::Full) || context.secondaryUse;
	int rotation = attached ? rotationSegment(context.yaw + 180.0F) : rotationSegment(Directions::toYRot(Directions::opposite(context.horizontalDirection())));
	int state	 = data.withProperty(ceiling, "rotation", std::to_string(rotation));
	state		 = data.withProperty(state, "attached", attached ? "true" : "false");
	return finish(level, context.clickedPos, state);
}

// WallHangingSignBlock.getStateForPlacement: across the face clicked, held by the blocks on its sides
int SignBlock::placeWallHanging(Level& level, const PlaceContext& context, int wall) const {
	for (Direction direction : context.nearestLookingDirections()) {
		if (!isHorizontal(direction) || sameAxis(direction, context.clickedFace)) continue;
		int state = _context->data.withProperty(wall, "facing", DIRECTION_NAMES[static_cast<int>(Directions::opposite(direction))]);
		if (int placed = finish(level, context.clickedPos, state); placed >= 0) return placed;
	}
	return -1;
}

// StandingAndWallBlockItem.getPlacementState: the floor (or ceiling) block when the player looks that way, else the wall
// block; never against the other side (a sign can't hang under a block, a hanging sign can't stand)
int SignBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	const GameData& data	 = _context->data;
	bool			hanging	 = _kind == Kind::CeilingHanging || _kind == Kind::WallHanging;
	std::string		name	 = data.getStaticName("minecraft:block", context.block);
	int				floor	 = data.getDefaultBlockState(name);
	int				wall	 = data.getDefaultBlockState(wallVariant(name));
	Direction		attachTo = hanging ? Direction::Up : Direction::Down;
	for (Direction direction : context.nearestLookingDirections()) {
		if (direction == Directions::opposite(attachTo)) continue;
		int state;
		if (direction == attachTo) {
			state = hanging ? placeCeiling(level, context, floor) : placeStanding(level, context, floor);
		} else if (wall >= 0) {
			state = hanging ? placeWallHanging(level, context, wall) : placeWall(level, context, wall);
		} else {
			continue;
		}
		if (state >= 0) return state;
	}
	return -1;
}

// ----- Support -----

bool SignBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	const GameData& data = _context->data;
	switch (_kind) {
	case Kind::Standing:
		return _context->properties(level.getBlockState(pos.relative(Direction::Down))).solid;
	case Kind::Wall: {
		Direction facing = directionNamed(data.getProperty(state, "facing"));
		return _context->properties(level.getBlockState(pos.relative(Directions::opposite(facing)))).solid;
	}
	case Kind::CeilingHanging:
		return _context->isFaceSturdy(level.getBlockState(pos.relative(Direction::Up)), Direction::Down, GameData::SupportType::Center);
	case Kind::WallHanging: {
		// canAttachTo: a sturdy face on either side, or another wall hanging sign along the same line
		Direction facing = directionNamed(data.getProperty(state, "facing"));
		for (Direction side : {Directions::clockWise(facing), Directions::opposite(Directions::clockWise(facing))}) {
			int other = level.getBlockState(pos.relative(side));
			if (_context->isFaceSturdy(other, Directions::opposite(side), GameData::SupportType::Full)) return true;
			if (data.getStaticName("minecraft:block", level.blocks().blockOf(other)).find("_wall_hanging_sign") != std::string::npos && sameAxis(directionNamed(data.getProperty(other, "facing")), facing)) return true;
		}
		return false;
	}
	}
	return true;
}

int SignBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	level.scheduleWaterlogged(pos, state);
	bool support = false;
	switch (_kind) {
	case Kind::Standing: support = direction == Direction::Down; break;
	case Kind::CeilingHanging: support = direction == Direction::Up; break;
	case Kind::Wall: support = direction == Directions::opposite(directionNamed(_context->data.getProperty(state, "facing"))); break;
	case Kind::WallHanging: support = isHorizontal(direction); break;
	}
	if (support && !canSurvive(level, pos, state)) return _air;
	return state;
}

// ----- Use -----

UseResult SignBlock::useItemOn(Level& level, const BlockPos& pos, int, Player& player, int hand, const BlockHit&) const {
	SignBlockEntity* sign = signAt(level, pos);
	if (!sign) return UseResult::Pass;
	const ItemStack& stack = player.getStackInHand(hand);
	if (stack.isEmpty()) return UseResult::TryWithEmptyHand;
	std::string item = _context->data.getStaticName("minecraft:item", stack.item);
	bool		applicator = item == "minecraft:honeycomb" || item == "minecraft:glow_ink_sac" || item == "minecraft:ink_sac" ||
					  (item.size() > 4 && item.compare(item.size() - 4, 4, "_dye") == 0);
	if (!applicator) return UseResult::TryWithEmptyHand;
	if (sign->isWaxed()) {
		level.playSound(nullptr, pos, "minecraft:block.sign.waxed_interact_fail", Level::SoundSource::Blocks);
		return UseResult::Success;
	}
	if (otherPlayerIsEditing(level, player, *sign)) return UseResult::Success;
	if (player.getGameMode() == GameMode::Adventure || !applyItem(level, player, *sign, Signs::isFacingFrontText(level, pos, player), item)) return UseResult::TryWithEmptyHand;
	if (player.getGameMode() != GameMode::Creative) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
	return UseResult::Success;
}

bool SignBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	SignBlockEntity* sign = signAt(level, pos);
	return sign && tryEdit(level, player, *sign);
}

float SignBlock::yRotation(const GameData& data, int state) {
	std::string rotation = data.getProperty(state, "rotation");
	if (!rotation.empty()) return std::stoi(rotation) * 22.5F; // RotationSegment.convertToDegrees
	return Directions::toYRot(directionNamed(data.getProperty(state, "facing")));
}

// ----- Editing -----

namespace Signs {
	bool isFacingFrontText(Level& level, const BlockPos& pos, const Player& player) {
		float  facing = SignBlock::yRotation(level.gameData(), level.getBlockState(pos));
		double dx	  = player.getX() - (pos.x + 0.5);
		double dz	  = player.getZ() - (pos.z + 0.5);
		float  toward = static_cast<float>(std::atan2(dz, dx) * 180.0 / M_PI) - 90.0F;
		return std::fabs(wrapDegrees(toward - facing)) <= 90.0F;
	}

	void openTextEdit(Level& level, Player& player, SignBlockEntity& sign, bool front) {
		sign.setAllowedPlayerEditor(player.getUUID());
		// The text the editor starts from: the block entity first (ClientboundBlockEntityDataPacket), then the editor
		Buffer data;
		data.writePosition(sign.pos().x, sign.pos().y, sign.pos().z);
		data.writeVarInt(level.gameData().getStaticId("minecraft:block_entity_type", sign.type()));
		std::vector<uint8_t> tag;
		sign.writeUpdateTag(tag);
		data.writeBytes(tag);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::BLOCK_ENTITY_DATA, data, level.server());
		Buffer open;
		open.writePosition(sign.pos().x, sign.pos().y, sign.pos().z);
		open.writeBool(front);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::OPEN_SIGN_EDITOR, open, level.server());
	}

	void afterPlaced(Level& level, Player& player, const BlockPos& pos) {
		SignBlockEntity* sign = signAt(level, pos);
		if (sign && !sign->text(true).hasMessage()) openTextEdit(level, player, *sign, true);
	}

	void handleUpdate(Level& level, Player& player, const BlockPos& pos, bool front, const std::array<std::string, 4>& lines) {
		SignBlockEntity* sign = signAt(level, pos);
		if (!sign) return;
		// updateSignText: only the player allowed to, from near enough (the block interaction range, plus a margin)
		double dx = player.getX() - (pos.x + 0.5), dy = player.getY() - (pos.y + 0.5), dz = player.getZ() - (pos.z + 0.5);
		if (!sign->canEdit(player.getUUID()) || dx * dx + dy * dy + dz * dz > 8.0 * 8.0) {
			sign->setChanged(); // The client shows the real text again
			return;
		}
		sign->updateText(front, lines);
	}
} // namespace Signs
