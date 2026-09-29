#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "lib/JavaRandom.hpp"
#include "lib/UUID.hpp"
#include "world/BlockPos.hpp"
#include "world/entity/Actor.hpp"
#include "world/entity/Geometry.hpp"

#include <algorithm>
#include <optional>
#include <vector>

class Buffer;
class Level;
namespace Combat {
	struct DamageSource;
}

// An entity other than a player, on the game thread (vanilla's Entity): position, movement and collisions with
// blocks, fluids pushing it. Ported from vanilla's Entity.move, collide and updateFluidHeightAndDoFluidPushing
class Entity : public Actor {
  public:
	Entity(Level& level, int typeId, float width, float height);
	virtual ~Entity();
	Entity(const Entity&)			 = delete;
	Entity& operator=(const Entity&) = delete;

	int			id() const override { return _id; }
	// Entity.getFluidHeight(WATER / LAVA) as of the last fluid update, and whether it is in lava
	double		waterHeight() const { return _waterHeight; }
	bool		isInLavaNow() const { return isInLava(); }
	// Entity.getFluidJumpThreshold
	double		fluidJumpThreshold() const { return const_cast<Entity*>(this)->eyeHeight() < 0.4 ? 0.0 : 0.4; }
	// The entity's own random (Entity.random)
	JavaRandom& random() { return _random; }
	Level&		level() const { return _level; }
	Level*		actorLevel() const override { return &_level; }
	const UUID& uuid() const override { return _uuid; }
	int			typeId() const override { return _typeId; }
	const Vec3& position() const override { return _position; }
	const Vec3& deltaMovement() const { return _delta; }
	void		setDeltaMovement(const Vec3& delta) { _delta = delta; }
	void		setPos(const Vec3& position) { _position = position; }
	bool		onGround() const { return _onGround; }
	void		setOnGround(bool onGround) { _onGround = onGround; }
	// Rotations in degrees (vanilla's yRot / xRot, the head's for living entities)
	float		yRot() const override { return _yRot; }
	float		xRot() const { return _xRot; }
	void		setYRot(float yRot) { _yRot = yRot; }
	void		setXRot(float xRot) { _xRot = xRot; }
	virtual float yHeadRot() const { return 0.0F; }
	// Entity.snapTo: position and rotation at once (spawning)
	void		snapTo(const Vec3& position, float yRot, float xRot);
	// Where it was at the start of the tick (xo, yo, zo), set by the level before each tick (setOldPosAndRot)
	const Vec3& oldPosition() const { return _oldPosition; }
	void		setOldPosAndRot() { _oldPosition = _position; _yRotO = _yRot; _xRotO = _xRot; }
	double		fallDistance() const { return _fallDistance; }
	void		resetFallDistance() { _fallDistance = 0.0; }
	int			remainingFireTicks() const { return _remainingFireTicks; }
	virtual void setRemainingFireTicks(int ticks) { _remainingFireTicks = ticks; }
	bool		isOnFire() const { return _remainingFireTicks > 0; }
	void		igniteForTicks(int ticks) override {
		   if (_remainingFireTicks < ticks) setRemainingFireTicks(ticks);
	}
	float		width() const { return _width; }
	float		height() const { return _height; }
	// Entity.move(PISTON): a push by a piston, at most 0.51 per axis per tick (limitPistonMovement)
	void		moveByPiston(Vec3 movement);
	// Entity.move(SHULKER_BOX): pushed by a shulker box's lid
	void		moveByShulker(Vec3 movement) { move(movement); }
	bool		isRemoved() const { return _removed; }
	void		discard() { _removed = true; }
	int			tickCount() const { return _tickCount; }
	void		tickCountIncrement() { _tickCount++; } // Before each tick (ServerLevel.tickNonPassenger)
	AABB		boundingBox() const override;
	// Entity.getEyeY: from its type's eye height (85% of its height by default)
	double		eyeY() const override { return _position.y + eyeHeight(); }
	virtual float eyeHeight() const { return _height * 0.85F; }
	bool		isAlive() const override { return !_removed; }
	void		pushMotion(const Vec3& impulse) override { push(impulse.x, impulse.y, impulse.z); }
	Entity*		asEntity() override { return this; }
	// TraceableEntity.getOwner: who shot, threw or lit it (projectiles, primed TNT), null if none or gone
	virtual Actor* owner() const { return nullptr; }
	BlockPos	blockPosition() const { return {Mth::floor(_position.x), Mth::floor(_position.y), Mth::floor(_position.z)}; }

	// Entity.tick; subclasses add their own logic around baseTick
	virtual void tick();
	// Entity.checkDespawn: every tick before the entity ticks (even outside ticking chunks)
	virtual void checkDespawn() {}
	// Entity.hurtServer: damage from a source. Returns whether it hurt (non-living entities ignore it here)
	bool hurtServer(const Combat::DamageSource&, float) override { return false; }
	virtual bool isLiving() const { return false; }
	// Entity.push(Entity) needs it: pushed by the entities it touches
	virtual bool isPushable() const { return false; }
	// Entity.push(x, y, z)
	void push(double x, double y, double z) {
		_delta	   = _delta + Vec3{x, y, z};
		hasImpulse = true;
	}
	// Entity.push(Entity): both pushed apart when they overlap
	void pushAgainst(Entity& other);

	// Saved with its chunk (Entity.shouldBeSaved): mobs are, items aren't
	virtual bool shouldBeSaved() const { return false; }
	// The entity's own data (after its type, written by EntityManager); load reads what save wrote
	virtual void save(Buffer&) const {}
	virtual void load(Buffer&) {}
	// What every entity saves (Entity.saveWithoutId): UUID, position, movement, rotation, fire, fall distance, portal
	// cooldown. For the entities that aren't living (their own data follows)
	void		 saveBase(Buffer& buf) const;
	void		 loadBase(Buffer& buf);
	void		 setUuid(const UUID& uuid) { _uuid = uuid; }
	// Shapes.collide(Y) against the collision boxes of the blocks touching `area`: how far `box` can move down (desired
	// is negative)
	static double collideDown(Level& level, const AABB& box, const AABB& area, double desired);

	// Networking: the entity's synced data (Set Entity Data entries, without the 0xFF end), empty if none
	virtual void writeEntityData(Buffer&) const {}
	// The value of the Spawn Entity packet's data field (Entity.getData): the XP value of an experience orb, 0 else
	virtual int entityData() const { return 0; }
	// Changed synced data only (Set Entity Data after a change). Defaults to everything
	virtual void writeDirtyEntityData(Buffer& buf) const { writeEntityData(buf); }
	virtual void clearDirtyEntityData() {}
	// Set when the synced data changed, cleared once sent
	bool entityDataDirty	= false;
	// Movement the players should see right away (a push...), cleared once sent
	bool hasImpulse			= false;
	// Entity.hurtMarked: its movement is sent to everyone at the end of the tick (knockback)
	bool hurtMarked			= false;

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
	float					_yRot = 0, _xRot = 0, _yRotO = 0, _xRotO = 0;
	Vec3					_oldPosition;
	double					_fallDistance		= 0.0;
	int						_remainingFireTicks = 0;

	void baseTick();
	// Entity.checkFallDamage: counts the fall, lands on the block (fallOn) once on the ground
	virtual void checkFallDamage(double dy, bool onGround, const BlockPos& onPos);
	// Entity.causeFallDamage: nothing for most entities. Returns whether it hurt
	virtual bool causeFallDamage(double, float) { return false; }
	// Entity.onBelowWorld: under the world's bottom by 64 blocks
	virtual void onBelowWorld() { discard(); }
	virtual bool fireImmune() const { return false; }
	void		 clearFire() { _remainingFireTicks = std::min(0, _remainingFireTicks); }
	// Entity.moveRelative / getInputVector: input (strafe, up, forward) turned by the entity's yaw, scaled by speed
	void		 moveRelative(float speed, const Vec3& input);
	// Entity.updateInWaterStateAndDoWaterCurrentPushing: water only (a fall ends in water)
	void		 updateInWaterStateAndDoWaterCurrentPushing();
	// Collision boxes of the blocks touching the box, reused between calls (no allocation per tick)
	static std::vector<AABB>& collisionScratch();
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
	// Entity.getBlockSpeedFactor: soul sand, honey
	virtual float blockSpeedFactor();
	// Entity.getBlockJumpFactor: honey
	float		  blockJumpFactor();
	// Set by resting living entities: the fluids around them can't have changed, baseTick doesn't look again
	bool		  _skipFluidUpdate = false;

  private:
	Vec3 collide(const Vec3& movement);
	void checkSupportingBlock(bool onGround, const Vec3& movement);
	std::optional<BlockPos> findSupportingBlock(const AABB& box);
	bool updateFluidHeightAndDoFluidPushing(bool lava, double pushStrength);
	bool  touchingUnloadedChunk();
};

#endif
