#ifndef HEIGHT_MAP_HPP
#define HEIGHT_MAP_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace World {
	enum class Type : uint8_t { MOTION_BLOCKING = 1, MOTION_BLOCKING_NO_LEAVES = 2, OCEAN_FLOOR = 3, WORLD_SURFACE = 4, WORLD_SURFACE_WG = 5 };

	class HeightMap {
		std::map<Type, std::vector<uint16_t>> _heightmaps;
		int									  _worldHeight;
		int									  _minY;
		uint8_t								  _bitsPerEntry;

		static constexpr size_t CHUNK_SIZE	   = 16;
		static constexpr size_t HEIGHTMAP_SIZE = CHUNK_SIZE * CHUNK_SIZE; // 256 entries

	  public:
		HeightMap(int worldHeight = 384, int minY = -64);

		// Core operations
		void	 setHeightMap(Type type, const std::vector<int64_t>& packedData);
		void	 setHeightMapDirect(Type type, const std::vector<uint16_t>& heights);
		void	 setHeight(Type type, uint8_t x, uint8_t z, uint16_t height);
		uint16_t getHeight(Type type, uint8_t x, uint8_t z) const;

		// Bulk operations
		void generateEmpty();
		void generateFlat(uint16_t height = 64);
		void copyFrom(const HeightMap& other);

		// Serialization
		std::vector<uint8_t> serializeToNBT() const;
		std::vector<int64_t> packHeightMap(Type type) const;
		void				 unpackHeightMap(Type type, const std::vector<int64_t>& packedData);

		// Accessors
		int				  getWorldHeight() const { return _worldHeight; }
		int				  getMinY() const { return _minY; }
		uint8_t			  getBitsPerEntry() const { return _bitsPerEntry; }
		bool			  hasHeightMap(Type type) const;
		std::vector<Type> getAvailableTypes() const;

		// Utilities
		static std::string getTypeName(Type type);
		static Type		   getTypeFromName(const std::string& name);

	  private:
		// Internal utilities
		size_t	coordToIndex(uint8_t x, uint8_t z) const;
		void	indexToCoord(size_t index, uint8_t& x, uint8_t& z) const;
		uint8_t calculateBitsPerEntry(int worldHeight) const;
		void	validateCoordinates(uint8_t x, uint8_t z) const;
		void	ensureHeightMapExists(Type type);

		// Bit packing utilities
		void packBits(const std::vector<uint16_t>& heights, std::vector<int64_t>& packed) const;
		void unpackBits(const std::vector<int64_t>& packed, std::vector<uint16_t>& heights) const;

		// NBT writing helpers
		void writeString(std::vector<uint8_t>& buffer, const std::string& str) const;
		void writeVarInt(std::vector<uint8_t>& buffer, int32_t value) const;
		void writeLongArray(std::vector<uint8_t>& buffer, const std::vector<int64_t>& data) const;
	};
} // namespace World
#endif
