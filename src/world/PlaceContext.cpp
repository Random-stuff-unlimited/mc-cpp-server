#include "world/PlaceContext.hpp"

#include "world/entity/Geometry.hpp"

#include <cmath>

Direction PlaceContext::horizontalDirection() const {
	// Direction.fromYRot: 2D values south, west, north, east
	constexpr Direction byValue[4] = {Direction::South, Direction::West, Direction::North, Direction::East};
	return byValue[Mth::floor(yaw / 90.0 + 0.5) & 3];
}

std::array<Direction, 6> PlaceContext::orderedByNearest() const {
	float xRot = pitch * static_cast<float>(M_PI / 180.0);
	float yRot = -yaw * static_cast<float>(M_PI / 180.0);
	float sinX = Mth::sin(xRot), cosX = Mth::cos(xRot);
	float sinY = Mth::sin(yRot), cosY = Mth::cos(yRot);
	bool  east = sinY > 0.0F, up = sinX < 0.0F, south = cosY > 0.0F;
	float x = east ? sinY : -sinY, y = up ? -sinX : sinX, z = south ? cosY : -cosY;
	float xs = x * cosX, zs = z * cosX;
	Direction dx = east ? Direction::East : Direction::West;
	Direction dy = up ? Direction::Up : Direction::Down;
	Direction dz = south ? Direction::South : Direction::North;
	auto make = [](Direction a, Direction b, Direction c) {
		return std::array<Direction, 6>{a, b, c, Directions::opposite(c), Directions::opposite(b), Directions::opposite(a)};
	};
	if (x > z) {
		if (y > xs) return make(dy, dx, dz);
		return zs > y ? make(dx, dz, dy) : make(dx, dy, dz);
	}
	if (y > zs) return make(dy, dz, dx);
	return xs > y ? make(dz, dx, dy) : make(dz, dy, dx);
}

std::array<Direction, 6> PlaceContext::nearestLookingDirections() const {
	std::array<Direction, 6> directions = orderedByNearest();
	if (replaceClicked) return directions;
	Direction against = Directions::opposite(clickedFace);
	int		  i		  = 0;
	while (i < 6 && directions[i] != against) i++;
	if (i > 0) {
		for (int j = i; j > 0; j--) directions[j] = directions[j - 1];
		directions[0] = against;
	}
	return directions;
}
