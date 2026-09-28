#include "world/entity/Geometry.hpp"

#include <array>

namespace {
	// Mth.SIN: sin(i * 2π / 65536), computed in double then stored as float
	const std::array<float, 65536>& sinTable() {
		static const std::array<float, 65536> table = [] {
			std::array<float, 65536> values{};
			for (int i = 0; i < 65536; i++) values[i] = static_cast<float>(std::sin(i * M_PI * 2.0 / 65536.0));
			return values;
		}();
		return table;
	}
} // namespace

namespace Mth {
	// In float, like vanilla: (int)(radians * 10430.378F) & 65535
	float sin(float radians) { return sinTable()[static_cast<int>(radians * 10430.378f) & 65535]; }
	float cos(float radians) { return sinTable()[static_cast<int>(radians * 10430.378f + 16384.0f) & 65535]; }
} // namespace Mth
