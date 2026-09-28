#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "lib/JavaRandom.hpp"
#include "lib/UUID.hpp"
#include "world/BlockPos.hpp"
#include "world/entity/Geometry.hpp"

#include <optional>
#include <vector>

class Buffer;
class Level;

// An entity other than a player, on the game thread (vanilla's Entity): position, movement and collisions with
// blocks, fluids pushing it. Ported from vanilla's Entity.move, collide and updateFluidHeightAndDoFluidPushing
class Entity {
  public:
	Entity(Level& level, int typeId, float width, float height);
	virtual ~Entity();
	Entity(const Entity&)			 = delete;
	Entity& operator=(const Entity&) = delete;

	int			id() const { return _id; }
	const UUID& uuid() const { return _uuid; }
	int			typeId() const { return _typeId; }
	const Vec3& position() const { return _position; }
	const Vec3& deltaMovement() const { return _delta; }
	void		setDeltaMovement(const Vec3& delta) { _delta = delta; }
	void		setPos(const Vec3& position) { _position = position; }
	bool		onGround() const { return _onGround; }
	// Entity.move(PISTON): a push by a piston, at most 0.51 per axis per tick (limitPistonMovement)
	void		moveByPiston(Vec3 movement);
	bool		isRemoved() const { return _removed; }
	void		discard() { _removed = true; }
	int			tickCount() const { return _tickCount; }
	void		tickCountIncrement() { _tickCount++; } // Before each tick (ServerLevel.tickNonPassenger)
	AABB		boundingBox() const;
	BlockPos	blockPosition() const { return {Mth::floor(_position.x), Mth::floor(_position.y), Mth::floor(_position.z)}; }

	// Entity.tick; subclasses add their own logic around baseTick
	virtual void tick();

	// Networking: the entity's synced data (Set Entity Data entries, without the 0xFF end), empty if none
	virtual void writeEntityData(Buffer&) const {}
	// Set when the synced data changed, cleared once sent
	bool entityDataDirty	= false;
	// Movement the players should see right away (a push...), cleared once sent
	bool hasImpulse			= false;

  protected:
	Level&	   _level;
	JavaRandom _random; // The entity's own random (Entity.random), not the level's
	int		   _id;
	UUID   _uuid;
	int	   _typeId;
	float  _width, _height;
	Vec3   _position;
	Vec3   _delta;
	bool   _onGround			= false;
	bool   _horizontalCollision = false;
	bool   _verticalCollision	= false;
	bool   _removed				= false;
	double	_pistonDeltas[3]	= {0.0, 0.0, 0.0};
	int64_t _pistonDeltasTime	= 0;
	bool   _noPhysics			= false;
	bool   _firstTick			= true;
	bool   _wasTouchingWater	= false;
	int	   _tickCount			= 0;
	double _waterHeight = 0, _lavaHeight = 0; // Fluid heights inside the bounding box, this tick
	std::optional<BlockPos> _supportingBlock;  // Main block under the entity (vanilla's mainSupportingBlockPos)
	bool					_onGroundNoBlocks = false;

	void baseTick();
	// Entity.move(SELF): moves as far as the blocks allow, updates onGround and the collisions
	void move(Vec3 movement);
	void applyGravity(double gravity) { _delta.y -= gravity; }
	bool isInWater() const { return _wasTouchingWater; }
	bool isInLava() const { return !_firstTick && _lavaHeight > 0.0; }
	// Entity.updateInWaterStateAndDoFluidPushing. Returns true when in a fluid
	bool updateInWaterStateAndDoFluidPushing();
	// Entity.moveTowardsClosestSpace: pushes out of a block the entity is stuck in
	void moveTowardsClosestSpace(double x, double y, double z);
	// Entity.getOnPos(offset): block under the entity whose friction and speed apply
	BlockPos getOnPos(float offset);
	bool	 noCollision(const AABB& box);
	virtual bool isPushedByFluid() const { return true; }
	// getBlockPosBelowThatAffectsMyMovement: whose friction and speed factor apply
	virtual BlockPos blockPosBelowAffectingMovement() { return getOnPos(0.500001f); }

  private:
	Vec3 collide(const Vec3& movement);
	void checkSupportingBlock(bool onGround, const Vec3& movement);
	std::optional<BlockPos> findSupportingBlock(const AABB& box);
	bool updateFluidHeightAndDoFluidPushing(bool lava, double pushStrength);
	float blockSpeedFactor();
	bool  touchingUnloadedChunk();
};

#endif
