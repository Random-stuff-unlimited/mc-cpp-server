#ifndef SIGNS_HPP
#define SIGNS_HPP

#include "world/blocks/Attached.hpp"

#include <array>
#include <string>

class SignBlockEntity;

// SignBlock and its four kinds (StandingSignBlock, WallSignBlock, CeilingHangingSignBlock, WallHangingSignBlock):
// placed on the floor, a wall or under a ceiling depending on where the player looks (StandingAndWallBlockItem),
// edited by the player who placed them, or later by clicking them when they aren't waxed; dyes, glow ink sacs, ink
// sacs and honeycomb apply to the side that faces the player (SignApplicator)
class SignBlock : public AttachedBlock {
  public:
	enum class Kind { Standing, Wall, CeilingHanging, WallHanging };
	SignBlock(std::shared_ptr<const BlockContext> context, Kind kind) : AttachedBlock(std::move(context)), _kind(kind) {}

	int		  getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool	  canSurvive(Level& level, const BlockPos& pos, int state) const override;
	int		  updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;

	// getYRotationDegrees: the direction the sign's front faces
	static float yRotation(const GameData& data, int state);

  private:
	Kind _kind;

	int placeStanding(Level& level, const PlaceContext& context, int standing) const;
	int placeWall(Level& level, const PlaceContext& context, int wall) const;
	int placeCeiling(Level& level, const PlaceContext& context, int ceiling) const;
	int placeWallHanging(Level& level, const PlaceContext& context, int wall) const;
	// The state with waterlogged set from the fluid at the position, -1 if it can't stay there
	int finish(Level& level, const BlockPos& pos, int state) const;
};

namespace Signs {
	// SignBlockEntity.isFacingFrontText: the player is in front of the sign's front
	bool isFacingFrontText(Level& level, const BlockPos& pos, const Player& player);
	// Player.openTextEdit: the player may edit that side; its client opens the editor (Open Sign Editor)
	void openTextEdit(Level& level, Player& player, SignBlockEntity& sign, bool front);
	// SignItem.updateCustomBlockEntityTag: a sign just placed by a player opens its editor (front side)
	void afterPlaced(Level& level, Player& player, const BlockPos& pos);
	// ServerboundSignUpdatePacket: the text the player typed, kept if it is the one allowed to edit it
	void handleUpdate(Level& level, Player& player, const BlockPos& pos, bool front, const std::array<std::string, 4>& lines);
} // namespace Signs

#endif
