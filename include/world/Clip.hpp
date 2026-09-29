#ifndef CLIP_HPP
#define CLIP_HPP

#include "data/GameData.hpp"
#include "world/BlockPos.hpp"
#include "world/entity/Geometry.hpp"

#include <optional>
#include <vector>

class Level;

// Ray casts through the blocks (vanilla's BlockGetter.clip with a ClipContext, VoxelShape.clip, AABB.clip)
namespace Clip {
	// ClipContext.Block: which shape of the blocks stops the ray
	enum class BlockMode { Collider, Outline, Visual };
	// ClipContext.Fluid: which fluids stop it
	enum class FluidMode { None, SourceOnly, Any, Water };

	// BlockHitResult: the block hit, its face and where; MISS when hit is false (location: the end of the ray)
	struct HitResult {
		bool	  hit = false;
		BlockPos  pos;
		Direction face = Direction::North;
		Vec3	  location;
	};

	// Level.clip from `from` to `to`
	HitResult clip(Level& level, const Vec3& from, const Vec3& to, BlockMode blockMode, FluidMode fluidMode);
	// VoxelShape.clip of a shape (boxes in block coordinates) at pos: a ray starting inside hits at once
	std::optional<HitResult> clipShape(const std::vector<GameData::Box>& boxes, const Vec3& from, const Vec3& to, const BlockPos& pos);
	// AABB.clip: where the segment enters the box, if it does
	std::optional<Vec3> clipBox(const AABB& box, const Vec3& from, const Vec3& to);
	// Direction.getApproximateNearest
	Direction approximateNearest(double dx, double dy, double dz);
} // namespace Clip

#endif
