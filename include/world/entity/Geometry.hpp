#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

// Vanilla's Vec3, AABB and the Mth functions entities use
struct Vec3 {
	double x = 0, y = 0, z = 0;

	Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
	Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
	Vec3 scale(double f) const { return {x * f, y * f, z * f}; }
	Vec3 multiply(double fx, double fy, double fz) const { return {x * fx, y * fy, z * fz}; }
	double lengthSqr() const { return x * x + y * y + z * z; }
	double length() const { return std::sqrt(lengthSqr()); }
	double horizontalDistanceSqr() const { return x * x + z * z; }
	// Vec3.normalize: zero below 1.0E-5
	Vec3 normalize() const {
		double l = length();
		return l < 1.0E-5 ? Vec3{} : Vec3{x / l, y / l, z / l};
	}
	double get(int axis) const { return axis == 0 ? x : axis == 1 ? y : z; }
	void   set(int axis, double value) { (axis == 0 ? x : axis == 1 ? y : z) = value; }
};

struct AABB {
	double minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;

	AABB move(double dx, double dy, double dz) const { return {minX + dx, minY + dy, minZ + dz, maxX + dx, maxY + dy, maxZ + dz}; }
	AABB move(const Vec3& v) const { return move(v.x, v.y, v.z); }
	AABB inflate(double dx, double dy, double dz) const { return {minX - dx, minY - dy, minZ - dz, maxX + dx, maxY + dy, maxZ + dz}; }
	AABB deflate(double d) const { return inflate(-d, -d, -d); }
	// AABB.expandTowards: grows toward the movement
	AABB expandTowards(const Vec3& v) const {
		AABB box = *this;
		(v.x < 0 ? box.minX : box.maxX) += v.x;
		(v.y < 0 ? box.minY : box.maxY) += v.y;
		(v.z < 0 ? box.minZ : box.maxZ) += v.z;
		return box;
	}
	bool intersects(const AABB& o) const {
		return minX < o.maxX && maxX > o.minX && minY < o.maxY && maxY > o.minY && minZ < o.maxZ && maxZ > o.minZ;
	}
	double min(int axis) const { return axis == 0 ? minX : axis == 1 ? minY : minZ; }
	double max(int axis) const { return axis == 0 ? maxX : axis == 1 ? maxY : maxZ; }
};

namespace Mth {
	inline int floor(double value) { return static_cast<int>(std::floor(value)); }
	inline int ceil(double value) { return static_cast<int>(std::ceil(value)); }
	// Mth.sin and Mth.cos: a 65536 entry table, like vanilla
	float sin(float radians);
	float cos(float radians);
	// Mth.equal
	inline bool equal(double a, double b) { return std::abs(b - a) < 1.0E-5; }
	// Mth.atan2: vanilla's table approximation (not std::atan2), for rotations computed from positions
	double atan2(double y, double x);
	inline float clamp(float value, float min, float max) { return value < min ? min : (value > max ? max : value); }
	// Mth.wrapDegrees: in [-180, 180)
	inline float wrapDegrees(float degrees) {
		float wrapped = std::fmod(degrees, 360.0f);
		if (wrapped >= 180.0f) wrapped -= 360.0f;
		if (wrapped < -180.0f) wrapped += 360.0f;
		return wrapped;
	}
	inline float degreesDifference(float from, float to) { return wrapDegrees(to - from); }
	// Mth.rotateIfNecessary: current brought within max degrees of target
	inline float rotateIfNecessary(float current, float target, float max) { return target - clamp(degreesDifference(current, target), -max, max); }
	// Mth.packDegrees: an angle as a byte (1/256 of a turn), as sent in movement packets
	inline int8_t packDegrees(float degrees) { return static_cast<int8_t>(floor(degrees * 256.0f / 360.0f)); }
	inline double absMax(double a, double b) { return std::max(std::abs(a), std::abs(b)); }
} // namespace Mth

#endif
