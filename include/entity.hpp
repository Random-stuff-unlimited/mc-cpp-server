#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "lib/UUID.hpp"

#include <string>
#include <utility>

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
	std::string _entityType;
	std::string _dimensionName;
	// Status Flag
  public:
	explicit Entity(UUID id, std::string entityType, std::string dimensionName)
		: _entityId(id), _entityType(std::move(entityType)), _dimensionName(std::move(dimensionName)) {}
	virtual ~Entity() = default;

	const std::string& getEnityType() const { return _entityType; }

	vec3 getPosition() const { return _position; }
	void setPosition(vec3 position) { _position = position; }

	rot	 getRotation() const { return _rotation; }
	void setRotation(rot rotation) { _rotation = rotation; }
};

#endif
