#ifndef PORTALS_HPP
#define PORTALS_HPP

#include "world/BlockBehavior.hpp"
#include "world/BlockPos.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/entity/Geometry.hpp"

#include <functional>
#include <memory>
#include <optional>

class Actor;
class Level;

// Nether and end portals: their frames, how an entity or a player goes through them, and where it arrives. Ported
// from vanilla's PortalShape, PortalForcer, NetherPortalBlock, EndPortalBlock, EnderEyeItem, EndPlatformFeature,
// PortalProcessor and Entity.handlePortal
namespace Portals {
	enum class Kind { Nether = 0, End = 1 };
	// Axis ids as Shapes::axisOf: 0 x, 1 y, 2 z

	// BaseFireBlock.inPortalDimension: fire lights portals in the overworld and the nether only
	bool inPortalDimension(const Level& level);

	// PortalShape: an obsidian frame and what fills it
	struct PortalShape {
		int		  axis = 0; // 0 x, 2 z
		Direction rightDir;
		int		  numPortalBlocks = 0;
		BlockPos  bottomLeft;
		int		  width = 0, height = 0;

		bool isValid() const { return width >= 2 && width <= 21 && height >= 3 && height <= 21; }
		bool isComplete() const { return isValid() && numPortalBlocks == width * height; }
		// Fills the frame with portal blocks (flags 18: clients, no neighbor updates... like vanilla)
		void createPortalBlocks(Level& level) const;
	};
	PortalShape				   findAnyShape(Level& level, const BlockPos& pos, int axis);
	// PortalShape.findEmptyPortalShape: a valid empty frame along this axis, else the other one
	std::optional<PortalShape> findEmptyPortalShape(Level& level, const BlockPos& pos, int axis);

	// BlockUtil.FoundRectangle and getLargestRectangleAround
	struct FoundRectangle {
		BlockPos minCorner;
		int		 axis1Size, axis2Size;
	};
	FoundRectangle getLargestRectangleAround(const BlockPos& pos, int axis1, int limit1, int axis2, int limit2, const std::function<bool(const BlockPos&)>& test);

	// Where an actor arrives: vanilla's TeleportTransition (position, and the rotation: added to the current one when
	// relativeRotation)
	struct Transition {
		Level* level = nullptr;
		Vec3   position;
		float  yRot = 0, xRot = 0;
		bool   relativeYRot = false, relativeXRot = false;
		bool   missingRespawnBlock = false;
		bool   portalSound		   = true; // TeleportTransition.PLAY_PORTAL_SOUND
	};
	std::optional<Transition> netherPortalDestination(Level& level, Actor& actor, const BlockPos& entry);
	std::optional<Transition> endPortalDestination(Level& level, Actor& actor, const BlockPos& entry);

	// PortalForcer
	std::optional<BlockPos>		  findClosestPortalPosition(Level& level, const BlockPos& pos, bool toNether);
	std::optional<FoundRectangle> createPortal(Level& level, const BlockPos& pos, int axis);
	// EndPlatformFeature.createEndPlatform: the obsidian floor and the air above around pos
	void createEndPlatform(Level& level, const BlockPos& pos, bool dropBlocks);
	// ServerLevel.END_SPAWN_POINT
	constexpr BlockPos END_SPAWN_POINT{100, 50, 0};

	// Entity.setAsInsidePortal (from the portal blocks' entityInside)
	void setAsInsidePortal(Actor& actor, Kind kind, const BlockPos& pos);
	// Entity.handlePortal, in its base tick: the time spent in the portal, then the trip. The actor may be gone after
	// (an entity is replaced by its copy in the other dimension)
	void handlePortal(Level& level, Actor& actor);
	// Entity.teleport(TeleportTransition) for an entity (not a player) to another dimension: its copy joins the new level
	void teleportEntity(Level& from, class Entity& entity, const Transition& transition);

	// Entity.canUsePortal: alive, not riding
	bool canUsePortal(Actor& actor);

	// EnderEyeItem.useOn: an eye into an empty frame, and the portal once the ring of frames is full. Returns whether
	// it was used
	bool placeEnderEye(Level& level, const BlockPos& pos);
} // namespace Portals

// NetherPortalBlock
class NetherPortalBlock : public BlockBehavior {
  public:
	explicit NetherPortalBlock(std::shared_ptr<const BlockContext> context);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void entityInside(Level& level, const BlockPos& pos, int state, Actor* actor) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
	bool isRandomlyTicking(int) const override { return true; }

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_axis, _air;
};

// EndPortalBlock
class EndPortalBlock : public BlockBehavior {
  public:
	void entityInside(Level& level, const BlockPos& pos, int state, Actor* actor) const override;
};

#endif
