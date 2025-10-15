#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "lib/UUID.hpp"

#include <string>

struct vec3 {
	double x, y, z;
};

struct rot {
	double pitch, yaw;
};

class Entity {
  private:
	vec3		_position;
	rot			_rotation;
	UUID		_entityId;
	std::string _entityName;
	std::string _dimensionName;
	// Status Flag
};

#endif
