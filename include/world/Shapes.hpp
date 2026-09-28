#ifndef SHAPES_HPP
#define SHAPES_HPP

#include "data/GameData.hpp"
#include "world/BlockPos.hpp"

#include <array>
#include <vector>

// Collision shapes (lists of boxes in block coordinates), as vanilla's VoxelShape questions need them
namespace Shapes {
	// Axis of a direction (0 = x, 1 = y, 2 = z) and whether it points to the positive side
	int	 axisOf(Direction direction);
	bool isPositive(Direction direction);

	// A single full cube (vanilla's Shapes.block())
	bool isFullBlock(const std::vector<GameData::Box>& boxes);
	// Rectangles of the boxes touching this side of the block cell (the max side or the min side of the axis), on the
	// two other axes: vanilla's getFaceShape
	void faceRectangles(const std::vector<GameData::Box>& boxes, int axis, bool maxSide, std::vector<std::array<double, 4>>& out);
	// Whether rectangles cover the whole unit square
	bool coversSquare(const std::vector<std::array<double, 4>>& rects);
	// Block.isFaceFull: the side of the shape in this direction is a full square
	bool isFaceFull(const std::vector<GameData::Box>& boxes, Direction direction);
	// The side of the shape in this direction isn't empty
	bool hasFace(const std::vector<GameData::Box>& boxes, Direction direction);
	// Shapes.mergedFaceOccludes: `first` and `second` (next to it in this direction) together close the face between
	// them. Empty lists are empty shapes
	bool mergedFaceOccludes(const std::vector<GameData::Box>& first, const std::vector<GameData::Box>& second, Direction direction);
} // namespace Shapes

#endif
