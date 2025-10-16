#ifndef PALETTED_CONTAINER_HPP
#define PALETTED_CONTAINER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace World {
	enum class PalettedType {
		SINGLE_VALUED = 0,
		INDIRECT	  = 1,
		DIRECT		  = 2,
	};

	class PalettedContainer {
	  private:
		World::PalettedType	  _containerType;
		uint8_t				  _bitsPerEntry;
		std::vector<uint32_t> _palette; // Small array of unique values
		std::vector<uint64_t> _data;	// Bit-packed indices/values
		size_t				  _size;	// Number of entries (4096 for blocks)
		bool				  _isBlockContainer;

		static constexpr uint8_t MIN_BITS_BLOCK = 4;
		static constexpr uint8_t MAX_BITS_BLOCK = 15;
		static constexpr uint8_t MIN_BITS_BIOME = 1;
		static constexpr uint8_t MAX_BITS_BIOME = 6;

	  public:
		explicit PalettedContainer(size_t size, bool isBlockContainer = true);

		void	 setSingleValue(uint32_t value);
		void	 setFromArray(const std::vector<uint32_t>& values);
		uint32_t getValue(size_t index) const; // Get block at position
		void	 setValue(size_t index, uint32_t value);

		std::vector<uint8_t> serialize() const; // Convert to network format

		PalettedType				 getType() const { return _containerType; }
		uint8_t						 getBitsPerEntry() const { return _bitsPerEntry; }
		const std::vector<uint32_t>& getPalette() const { return _palette; }
		size_t						 getSize() const { return _size; }

	  private:
		// Internal optimization methods
		void optimizeEncoding(const std::vector<uint32_t>& values);
		void packData(const std::vector<uint32_t>& values);
		void unpackData(std::vector<uint32_t>& values) const;

		// Bit manipulation utilities
		void	 setBits(size_t index, uint32_t value);
		uint32_t getBits(size_t index) const;

		// Palette management
		uint32_t addToPalette(uint32_t value);
		uint32_t findInPalette(uint32_t value) const;

		// Encoding strategy selection
		uint8_t calculateOptimalBits(size_t uniqueValues) const;
		bool	shouldUseDirect(size_t uniqueValues) const;

		// Serialization helpers
		void writeVarInt(std::vector<uint8_t>& buffer, int32_t value) const;
		void writeVarLong(std::vector<uint8_t>& buffer, int64_t value) const;
	};
}; // namespace World

#endif
