#ifndef BLOCK_POS_HPP
#define BLOCK_POS_HPP

#include <cstdint>
#include <functional>

// Directions in vanilla's order (Direction.values()): the ordinal matters for update orders and saved data
enum class Direction : uint8_t { Down = 0, Up = 1, North = 2, South = 3, West = 4, East = 5 };

namespace Directions {
	constexpr Direction ALL[6] = {Direction::Down, Direction::Up, Direction::North, Direction::South, Direction::West, Direction::East};
	// NeighborUpdater.UPDATE_ORDER: order of the neighbor updates (neighborChanged)
	constexpr Direction UPDATE_ORDER[6] = {Direction::West, Direction::East, Direction::Down, Direction::Up, Direction::North, Direction::South};
	// BlockBehaviour.UPDATE_SHAPE_ORDER: order of the shape updates (updateShape)
	constexpr Direction SHAPE_UPDATE_ORDER[6] = {Direction::West, Direction::East, Direction::North, Direction::South, Direction::Down, Direction::Up};

	constexpr int OFFSETS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

	constexpr Direction opposite(Direction direction) { return static_cast<Direction>(static_cast<uint8_t>(direction) ^ 1); }
} // namespace Directions

struct BlockPos {
	int x = 0, y = 0, z = 0;

	constexpr BlockPos relative(Direction direction, int distance = 1) const {
		const int* offset = Directions::OFFSETS[static_cast<int>(direction)];
		return {x + offset[0] * distance, y + offset[1] * distance, z + offset[2] * distance};
	}
	constexpr BlockPos above() const { return {x, y + 1, z}; }
	constexpr BlockPos below() const { return {x, y - 1, z}; }

	int chunkX() const { return x >> 4; }
	int chunkZ() const { return z >> 4; }

	// BlockPos.asLong: x and z on 26 bits, y on 12. Unique for every position in the world
	constexpr int64_t asLong() const {
		return static_cast<int64_t>((static_cast<uint64_t>(x) & 0x3FFFFFF) << 38 | (static_cast<uint64_t>(z) & 0x3FFFFFF) << 12 |
									(static_cast<uint64_t>(y) & 0xFFF));
	}
	// Vec3i.hashCode, for the few places where vanilla iterates a hash set of positions
	constexpr int32_t javaHashCode() const {
		return static_cast<int32_t>((static_cast<uint32_t>(y) + static_cast<uint32_t>(z) * 31u) * 31u + static_cast<uint32_t>(x));
	}

	constexpr bool operator==(const BlockPos& other) const { return x == other.x && y == other.y && z == other.z; }
	constexpr bool operator!=(const BlockPos& other) const { return !(*this == other); }
};

template <> struct std::hash<BlockPos> {
	size_t operator()(const BlockPos& pos) const noexcept { return std::hash<int64_t>()(pos.asLong()); }
};

#endif
