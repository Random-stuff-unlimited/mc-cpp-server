#include "world/Clip.hpp"

#include "world/Level.hpp"

#include <cmath>
#include <limits>

namespace {
	constexpr int DIRECTION_NORMALS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

	// AABB.clipPoint
	bool clipPoint(double& t, double delta, double deltaB, double deltaC, double plane, double minB, double maxB, double minC, double maxC,
				   double start, double startB, double startC) {
		double s = (plane - start) / delta;
		double b = startB + s * deltaB;
		double c = startC + s * deltaC;
		if (0.0 < s && s < t && minB - 1.0E-7 < b && b < maxB + 1.0E-7 && minC - 1.0E-7 < c && c < maxC + 1.0E-7) {
			t = s;
			return true;
		}
		return false;
	}

	// AABB.getDirection: updates t and the face when this box is hit closer
	void clipFaces(const AABB& box, const Vec3& from, double& t, std::optional<Direction>& face, double dx, double dy, double dz) {
		if (dx > 1.0E-7) {
			if (clipPoint(t, dx, dy, dz, box.minX, box.minY, box.maxY, box.minZ, box.maxZ, from.x, from.y, from.z)) face = Direction::West;
		} else if (dx < -1.0E-7) {
			if (clipPoint(t, dx, dy, dz, box.maxX, box.minY, box.maxY, box.minZ, box.maxZ, from.x, from.y, from.z)) face = Direction::East;
		}
		if (dy > 1.0E-7) {
			if (clipPoint(t, dy, dz, dx, box.minY, box.minZ, box.maxZ, box.minX, box.maxX, from.y, from.z, from.x)) face = Direction::Down;
		} else if (dy < -1.0E-7) {
			if (clipPoint(t, dy, dz, dx, box.maxY, box.minZ, box.maxZ, box.minX, box.maxX, from.y, from.z, from.x)) face = Direction::Up;
		}
		if (dz > 1.0E-7) {
			if (clipPoint(t, dz, dx, dy, box.minZ, box.minX, box.maxX, box.minY, box.maxY, from.z, from.x, from.y)) face = Direction::North;
		} else if (dz < -1.0E-7) {
			if (clipPoint(t, dz, dx, dy, box.maxZ, box.minX, box.maxX, box.minY, box.maxY, from.z, from.x, from.y)) face = Direction::South;
		}
	}

	double frac(double value) { return value - std::floor(value); }
	double lerp(double delta, double a, double b) { return a + delta * (b - a); }
	int	   sign(double value) { return value == 0.0 ? 0 : value > 0.0 ? 1 : -1; }
} // namespace

namespace Clip {

	Direction approximateNearest(double dx, double dy, double dz) {
		float	  x = static_cast<float>(dx), y = static_cast<float>(dy), z = static_cast<float>(dz);
		Direction best = Direction::North;
		float	  max  = std::numeric_limits<float>::denorm_min(); // Float.MIN_VALUE
		for (int d = 0; d < 6; d++) {
			float dot = x * DIRECTION_NORMALS[d][0] + y * DIRECTION_NORMALS[d][1] + z * DIRECTION_NORMALS[d][2];
			if (dot > max) {
				max	 = dot;
				best = static_cast<Direction>(d);
			}
		}
		return best;
	}

	std::optional<Vec3> clipBox(const AABB& box, const Vec3& from, const Vec3& to) {
		double					 t = 1.0;
		std::optional<Direction> face;
		Vec3					 delta = to - from;
		clipFaces(box, from, t, face, delta.x, delta.y, delta.z);
		if (!face) return std::nullopt;
		return from + delta.scale(t);
	}

	std::optional<HitResult> clipShape(const std::vector<GameData::Box>& boxes, const Vec3& from, const Vec3& to, const BlockPos& pos) {
		if (boxes.empty()) return std::nullopt;
		Vec3 delta = to - from;
		if (delta.lengthSqr() < 1.0E-7) return std::nullopt;
		Vec3 inside = from + delta.scale(0.001);
		for (const GameData::Box& b : boxes) {
			double x = inside.x - pos.x, y = inside.y - pos.y, z = inside.z - pos.z;
			if (x >= b.minX && x < b.maxX && y >= b.minY && y < b.maxY && z >= b.minZ && z < b.maxZ) {
				return HitResult{true, pos, Directions::opposite(approximateNearest(delta.x, delta.y, delta.z)), inside};
			}
		}
		double					 t = 1.0;
		std::optional<Direction> face;
		for (const GameData::Box& b : boxes) {
			clipFaces({pos.x + b.minX, pos.y + b.minY, pos.z + b.minZ, pos.x + b.maxX, pos.y + b.maxY, pos.z + b.maxZ}, from, t, face, delta.x, delta.y,
					  delta.z);
		}
		if (!face) return std::nullopt;
		return HitResult{true, pos, *face, from + delta.scale(t)};
	}

	HitResult clip(Level& level, const Vec3& fromPoint, const Vec3& toPoint, BlockMode blockMode, FluidMode fluidMode) {
		const GameData& gd = level.gameData();
		auto			at = [&](const BlockPos& pos) -> std::optional<HitResult> {
			   int								 state = level.getBlockState(pos);
			   const std::vector<GameData::Box>& shape = blockMode == BlockMode::Outline ? gd.getOutlineShape(state) : gd.getCollisionShape(state);
			   std::optional<HitResult>			 blockHit = clipShape(shape, fromPoint, toPoint, pos);
			   std::optional<HitResult>			 fluidHit;
			   FluidState						 fluid = level.fluids().stateOf(state);
			   bool								 fluidStops = false;
			   if (!fluid.isEmpty()) {
				   switch (fluidMode) {
				   case FluidMode::None: break;
				   case FluidMode::SourceOnly: fluidStops = level.fluids().isSource(fluid); break;
				   case FluidMode::Any: fluidStops = true; break;
				   case FluidMode::Water: fluidStops = level.fluids().isWater(fluid.type); break;
				   }
			   }
			   if (fluidStops) {
				   // FlowingFluid.getShape: up to the fluid's height
				   fluidHit = clipShape({{0, 0, 0, 1, level.fluids().height(fluid, pos), 1}}, fromPoint, toPoint, pos);
			   }
			   double blockDistance = blockHit ? (blockHit->location - fromPoint).lengthSqr() : std::numeric_limits<double>::max();
			   double fluidDistance = fluidHit ? (fluidHit->location - fromPoint).lengthSqr() : std::numeric_limits<double>::max();
			   return blockDistance <= fluidDistance ? blockHit : fluidHit;
		};
		auto miss = [&]() {
			Vec3	  back = fromPoint - toPoint;
			HitResult result;
			result.face		= approximateNearest(back.x, back.y, back.z);
			result.pos		= {Mth::floor(toPoint.x), Mth::floor(toPoint.y), Mth::floor(toPoint.z)};
			result.location = toPoint;
			return result;
		};
		// BlockGetter.traverseBlocks
		if (fromPoint.x == toPoint.x && fromPoint.y == toPoint.y && fromPoint.z == toPoint.z) return miss();
		double toX = lerp(-1.0E-7, toPoint.x, fromPoint.x), toY = lerp(-1.0E-7, toPoint.y, fromPoint.y), toZ = lerp(-1.0E-7, toPoint.z, fromPoint.z);
		double fromX = lerp(-1.0E-7, fromPoint.x, toPoint.x), fromY = lerp(-1.0E-7, fromPoint.y, toPoint.y), fromZ = lerp(-1.0E-7, fromPoint.z, toPoint.z);
		int	   x = Mth::floor(fromX), y = Mth::floor(fromY), z = Mth::floor(fromZ);
		if (auto hit = at({x, y, z})) return *hit;
		double dx = toX - fromX, dy = toY - fromY, dz = toZ - fromZ;
		int	   sx = sign(dx), sy = sign(dy), sz = sign(dz);
		double stepX = sx == 0 ? std::numeric_limits<double>::max() : sx / dx;
		double stepY = sy == 0 ? std::numeric_limits<double>::max() : sy / dy;
		double stepZ = sz == 0 ? std::numeric_limits<double>::max() : sz / dz;
		double tx	 = stepX * (sx > 0 ? 1.0 - frac(fromX) : frac(fromX));
		double ty	 = stepY * (sy > 0 ? 1.0 - frac(fromY) : frac(fromY));
		double tz	 = stepZ * (sz > 0 ? 1.0 - frac(fromZ) : frac(fromZ));
		while (tx <= 1.0 || ty <= 1.0 || tz <= 1.0) {
			if (tx < ty) {
				if (tx < tz) {
					x += sx;
					tx += stepX;
				} else {
					z += sz;
					tz += stepZ;
				}
			} else if (ty < tz) {
				y += sy;
				ty += stepY;
			} else {
				z += sz;
				tz += stepZ;
			}
			if (auto hit = at({x, y, z})) return *hit;
		}
		return miss();
	}

} // namespace Clip
