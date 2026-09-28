#include "world/Shapes.hpp"

#include <algorithm>
#include <cmath>

namespace {
	bool near(double value, double target) { return std::abs(value - target) < 1.0E-7; }

	double coordinate(const GameData::Box& box, int axis, bool max) {
		if (axis == 0) return max ? box.maxX : box.minX;
		if (axis == 1) return max ? box.maxY : box.minY;
		return max ? box.maxZ : box.minZ;
	}
} // namespace

namespace Shapes {
	int axisOf(Direction direction) {
		if (direction == Direction::Down || direction == Direction::Up) return 1;
		return direction == Direction::North || direction == Direction::South ? 2 : 0;
	}

	bool isPositive(Direction direction) { return direction == Direction::Up || direction == Direction::South || direction == Direction::East; }

	bool isFullBlock(const std::vector<GameData::Box>& boxes) {
		return boxes.size() == 1 && near(boxes[0].minX, 0) && near(boxes[0].minY, 0) && near(boxes[0].minZ, 0) && near(boxes[0].maxX, 1) &&
			   near(boxes[0].maxY, 1) && near(boxes[0].maxZ, 1);
	}

	void faceRectangles(const std::vector<GameData::Box>& boxes, int axis, bool maxSide, std::vector<std::array<double, 4>>& out) {
		int u = axis == 0 ? 1 : 0, v = axis == 2 ? 1 : 2;
		for (const GameData::Box& box : boxes) {
			if (!near(coordinate(box, axis, maxSide), maxSide ? 1.0 : 0.0)) continue;
			out.push_back({coordinate(box, u, false), coordinate(box, v, false), coordinate(box, u, true), coordinate(box, v, true)});
		}
	}

	bool coversSquare(const std::vector<std::array<double, 4>>& rects) {
		std::vector<double> us = {0, 1}, vs = {0, 1};
		for (const auto& r : rects) {
			for (double u : {r[0], r[2]}) {
				if (u > 0 && u < 1) us.push_back(u);
			}
			for (double v : {r[1], r[3]}) {
				if (v > 0 && v < 1) vs.push_back(v);
			}
		}
		std::sort(us.begin(), us.end());
		std::sort(vs.begin(), vs.end());
		for (size_t i = 0; i + 1 < us.size(); i++) {
			for (size_t j = 0; j + 1 < vs.size(); j++) {
				if (us[i + 1] - us[i] < 1.0E-7 || vs[j + 1] - vs[j] < 1.0E-7) continue;
				double cu = (us[i] + us[i + 1]) / 2, cv = (vs[j] + vs[j + 1]) / 2;
				bool covered = std::any_of(rects.begin(), rects.end(), [&](const auto& r) { return cu > r[0] && cu < r[2] && cv > r[1] && cv < r[3]; });
				if (!covered) return false;
			}
		}
		return true;
	}

	bool isFaceFull(const std::vector<GameData::Box>& boxes, Direction direction) {
		std::vector<std::array<double, 4>> face;
		faceRectangles(boxes, axisOf(direction), isPositive(direction), face);
		return !face.empty() && coversSquare(face);
	}

	bool hasFace(const std::vector<GameData::Box>& boxes, Direction direction) {
		std::vector<std::array<double, 4>> face;
		faceRectangles(boxes, axisOf(direction), isPositive(direction), face);
		return std::any_of(face.begin(), face.end(), [](const auto& r) { return r[2] - r[0] > 1.0E-7 && r[3] - r[1] > 1.0E-7; });
	}

	bool mergedFaceOccludes(const std::vector<GameData::Box>& first, const std::vector<GameData::Box>& second, Direction direction) {
		if (isFullBlock(first) || isFullBlock(second)) return true;
		// The block on the positive side shows its min side, the other one its max side
		const std::vector<GameData::Box>& low  = isPositive(direction) ? first : second;
		const std::vector<GameData::Box>& high = isPositive(direction) ? second : first;
		int								  axis = axisOf(direction);
		std::vector<std::array<double, 4>> face;
		faceRectangles(low, axis, true, face);
		faceRectangles(high, axis, false, face);
		return !face.empty() && coversSquare(face);
	}
} // namespace Shapes
