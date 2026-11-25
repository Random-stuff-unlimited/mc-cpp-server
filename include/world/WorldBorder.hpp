#ifndef WORLD_BORDER_HPP
#define WORLD_BORDER_HPP

#include <cstdint>

// WorldBorder: a square centered on (centerX, centerZ), growing or shrinking over game ticks toward its target size.
// Outside it (beyond the safe zone) the living take damage per block; nothing can be broken or placed beyond it.
// One border for the whole world (the other dimensions follow the overworld's)
struct WorldBorder {
	static constexpr double MAX_SIZE		  = 5.999997E7;
	static constexpr int	ABSOLUTE_MAX_SIZE = 29999984;

	double	centerX = 0.0, centerZ = 0.0;
	double	fromSize = MAX_SIZE, toSize = MAX_SIZE;
	int64_t lerpStart = 0, lerpDuration = 0; // Game ticks
	double	damagePerBlock = 0.2, safeZone = 5.0;
	int		warningBlocks = 5, warningTime = 15;

	double size(int64_t gameTime) const {
		if (lerpDuration <= 0 || gameTime >= lerpStart + lerpDuration) return toSize;
		double progress = static_cast<double>(gameTime - lerpStart) / static_cast<double>(lerpDuration);
		return fromSize + (toSize - fromSize) * progress;
	}
	int64_t lerpRemaining(int64_t gameTime) const { return lerpDuration <= 0 ? 0 : lerpStart + lerpDuration > gameTime ? lerpStart + lerpDuration - gameTime : 0; }
	void	setSize(double size) {
		   fromSize = toSize = size;
		   lerpDuration		 = 0;
	}
	void lerpSizeBetween(double from, double to, int64_t ticks, int64_t gameTime) {
		fromSize	 = from;
		toSize		 = to;
		lerpStart	 = gameTime;
		lerpDuration = ticks;
	}
	double minX(int64_t t) const { return centerX - size(t) / 2.0; }
	double maxX(int64_t t) const { return centerX + size(t) / 2.0; }
	double minZ(int64_t t) const { return centerZ - size(t) / 2.0; }
	double maxZ(int64_t t) const { return centerZ + size(t) / 2.0; }
	// isWithinBounds of a block
	bool isWithinBounds(int x, int z, int64_t t) const { return x + 1 > minX(t) && x < maxX(t) && z + 1 > minZ(t) && z < maxZ(t); }
	// getDistanceToBorder: to the nearest edge, negative outside
	double distanceToBorder(double x, double z, int64_t t) const {
		double toMinX = x - minX(t), toMaxX = maxX(t) - x, toMinZ = z - minZ(t), toMaxZ = maxZ(t) - z;
		double inside = toMinX < toMaxX ? toMinX : toMaxX;
		inside		  = inside < toMinZ ? inside : toMinZ;
		return inside < toMaxZ ? inside : toMaxZ;
	}
};

#endif
