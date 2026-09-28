#include "world/entity/Geometry.hpp"

#include <array>
#include <cstdint>
#include <cstring>

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

	// Mth.ASIN_TAB and COS_TAB: 257 entries for atan2
	struct AsinTables {
		double asin[257], cos[257];
		AsinTables() {
			for (int i = 0; i < 257; i++) {
				double value = std::asin(i / 256.0);
				cos[i]		 = std::cos(value);
				asin[i]		 = value;
			}
		}
	};
	const AsinTables& asinTables() {
		static const AsinTables tables;
		return tables;
	}

	// Mth.fastInvSqrt
	double fastInvSqrt(double value) {
		double	half = 0.5 * value;
		int64_t bits;
		std::memcpy(&bits, &value, sizeof(bits));
		bits = 6910469410427058090LL - (bits >> 1);
		std::memcpy(&value, &bits, sizeof(bits));
		return value * (1.5 - half * value * value);
	}
} // namespace

namespace Mth {
	double atan2(double y, double x) {
		double lengthSqr = x * x + y * y;
		if (std::isnan(lengthSqr)) return NAN;
		bool negativeY = y < 0.0;
		if (negativeY) y = -y;
		bool negativeX = x < 0.0;
		if (negativeX) x = -x;
		bool steep = y > x;
		if (steep) std::swap(x, y);
		double inverse = fastInvSqrt(lengthSqr);
		x *= inverse;
		y *= inverse;
		int64_t fracBiasBits = 4805340802404319232LL;
		double	fracBias;
		std::memcpy(&fracBias, &fracBiasBits, sizeof(fracBias));
		double	biased = fracBias + y;
		int64_t bits;
		std::memcpy(&bits, &biased, sizeof(bits));
		int	   index  = static_cast<int>(bits); // (int) Double.doubleToRawLongBits: the low bits
		double asin	  = asinTables().asin[index];
		double cos	  = asinTables().cos[index];
		double frac	  = biased - fracBias;
		double delta  = y * cos - x * frac;
		double result = asin + (6.0 + delta * delta) * delta * 0.16666666666666666;
		if (steep) result = M_PI / 2 - result;
		if (negativeX) result = M_PI - result;
		if (negativeY) result = -result;
		return result;
	}

	// In float, like vanilla: (int)(radians * 10430.378F) & 65535
	float sin(float radians) { return sinTable()[static_cast<int>(radians * 10430.378f) & 65535]; }
	float cos(float radians) { return sinTable()[static_cast<int>(radians * 10430.378f + 16384.0f) & 65535]; }
} // namespace Mth
