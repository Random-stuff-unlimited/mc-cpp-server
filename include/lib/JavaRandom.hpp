#ifndef JAVA_RANDOM_HPP
#define JAVA_RANDOM_HPP

#include <cmath>
#include <cstdint>

// java.util.Random (vanilla's LegacyRandomSource): same algorithm, so the same distributions (nextInt(bound) isn't a
// plain modulo, nextFloat has 24 bits...). The values can't match a vanilla server anyway: its generator is shared
// by everything happening in the world.
class JavaRandom {
  public:
	explicit JavaRandom(int64_t seed) { setSeed(seed); }

	void setSeed(int64_t seed) {
		_seed					= (static_cast<uint64_t>(seed) ^ MULTIPLIER) & MASK;
		_haveNextNextGaussian = false;
	}

	int32_t nextInt() { return next(32); }
	// Uniform in [0, bound), bound > 0
	int32_t nextInt(int32_t bound) {
		if ((bound & -bound) == bound) return static_cast<int32_t>((static_cast<int64_t>(bound) * next(31)) >> 31);
		int32_t bits, value;
		do {
			bits  = next(31);
			value = bits % bound;
		} while (static_cast<int32_t>(static_cast<uint32_t>(bits) - static_cast<uint32_t>(value) + static_cast<uint32_t>(bound - 1)) < 0); // Java's overflow
		return value;
	}
	int64_t nextLong() { return (static_cast<int64_t>(next(32)) << 32) + next(32); }
	bool	nextBoolean() { return next(1) != 0; }
	float	nextFloat() { return next(24) / static_cast<float>(1 << 24); }
	double	nextDouble() { return static_cast<double>((static_cast<int64_t>(next(26)) << 27) + next(27)) * 0x1.0p-53; }
	// MarsagliaPolarGaussian
	double	nextGaussian() {
		 if (_haveNextNextGaussian) {
			 _haveNextNextGaussian = false;
			 return _nextNextGaussian;
		 }
		 double v1, v2, s;
		 do {
			 v1 = 2.0 * nextDouble() - 1.0;
			 v2 = 2.0 * nextDouble() - 1.0;
			 s	= v1 * v1 + v2 * v2;
		 } while (s >= 1.0 || s == 0.0);
		 double multiplier	   = std::sqrt(-2.0 * std::log(s) / s);
		 _nextNextGaussian	   = v2 * multiplier;
		 _haveNextNextGaussian = true;
		 return v1 * multiplier;
	}

  private:
	static constexpr uint64_t MULTIPLIER = 0x5DEECE66DULL;
	static constexpr uint64_t MASK		 = (1ULL << 48) - 1;
	uint64_t				  _seed		 = 0;
	double					  _nextNextGaussian		= 0.0;
	bool					  _haveNextNextGaussian = false;

	int32_t next(int bits) {
		_seed = (_seed * MULTIPLIER + 0xBULL) & MASK;
		return static_cast<int32_t>(static_cast<int64_t>(_seed) >> (48 - bits));
	}
};

#endif
