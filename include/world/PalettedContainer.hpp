#ifndef PALETTED_CONTAINER_HPP
#define PALETTED_CONTAINER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

class Buffer;

// Stores `entries` values (block states of a 16x16x16 section, or biomes of a 4x4x4 one) the way the client expects them:
//  - single value: no storage at all (most sections are only air)
//  - indirect: a small palette of distinct values + packed indices into it
//  - direct: packed values, when there are too many distinct values for a palette
// Indices are packed into 64-bit longs without spanning two longs, like the vanilla format.
class PalettedContainer {
  public:
	struct Config {
		uint32_t entries;		   // 4096 for blocks, 64 for biomes
		uint8_t	 minIndirectBits;  // The client uses at least this many bits for a palette (4 for blocks, 1 for biomes)
		uint8_t	 maxIndirectBits;  // Above this, the client expects direct values (8 for blocks, 3 for biomes)
		uint8_t	 directBits;	   // ceil(log2(number of possible values))
	};

	PalettedContainer(const Config& config, uint32_t value);

	// Builds a container from a palette and indices packed with `bits` bits (disk and vanilla formats), whatever their size
	static PalettedContainer fromPalette(const Config& config, const std::vector<uint32_t>& palette, uint8_t bits, const std::vector<uint64_t>& data);

	uint32_t get(uint32_t index) const;
	void	 set(uint32_t index, uint32_t value);
	void	 fill(uint32_t value);

	bool	 isSingleValue() const { return _bits == 0; }
	uint32_t singleValue() const { return _single; }

	// Every value, in index order
	std::vector<uint32_t> values() const;
	// Distinct values actually used (the palette may keep values that were overwritten)
	void compactPalette(std::vector<uint32_t>& palette, uint8_t& bits, std::vector<uint64_t>& data) const;

	void   writeNetwork(Buffer& buf) const;
	size_t memoryUsage() const;

	static uint8_t				 bitsFor(uint32_t count); // ceil(log2(count)), 0 for count <= 1
	static std::vector<uint64_t> pack(const std::vector<uint32_t>& indices, uint8_t bits);
	static std::vector<uint32_t> unpack(const std::vector<uint64_t>& data, uint8_t bits, uint32_t entries);

  private:
	const Config*		  _config;
	uint8_t				  _bits;	// 0 = single value
	uint32_t			  _single;	// Value when _bits == 0
	std::vector<uint32_t> _palette; // Empty in direct mode
	std::vector<uint64_t> _data;

	bool	isDirect() const { return _bits != 0 && _palette.empty(); }
	uint8_t indirectBitsFor(size_t paletteSize) const;
	void	rebuild(const std::vector<uint32_t>& values);
};

#endif
