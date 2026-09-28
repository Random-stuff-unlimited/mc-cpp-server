#ifndef LIVING_ENTITY_HPP
#define LIVING_ENTITY_HPP

#include "data/GameData.hpp"
#include "world/entity/Attributes.hpp"
#include "world/entity/Entity.hpp"

#include <string>

class Chunk;
class Player;

// A living entity other than a player (vanilla's LivingEntity): health and attributes from its type's defaults,
// gravity, friction and fluid drag (travel), fall damage, burning, drowning, hurt (invulnerability ticks, damage
// event, knockback), death (animation, loot, removal after 20 ticks). Ported from vanilla's LivingEntity.
//
// Resting: a living entity lying still on the ground repeats the exact same tick forever (it tries to fall 0.0784,
// the ground stops it). Once a tick changed neither its position nor its movement, its travel and fluid checks are
// skipped as long as nothing can change the outcome: same position and movement, no input, not in a fluid, and no
// block changed in the chunks around it (Chunk::version). Anything else (a push, a hit, a block broken under it)
// wakes it up. Only the pressure plates and tripwires it rests on aren't told again (they stay powered anyway)
class LivingEntity : public Entity {
  public:
	// Entity data (SynchedEntityData ids of Entity and LivingEntity) and their serializers
	static constexpr int DATA_SHARED_FLAGS = 0, DATA_AIR_SUPPLY = 1, DATA_POSE = 6, DATA_LIVING_FLAGS = 8, DATA_HEALTH = 9;
	static constexpr int SERIALIZER_BYTE = 0, SERIALIZER_INT = 1, SERIALIZER_FLOAT = 3, SERIALIZER_POSE = 20;
	static constexpr int POSE_STANDING = 0, POSE_DYING = 7;
	static constexpr int DEATH_DURATION = 20;
	// ClientboundEntityEventPacket ids (EntityEvent)
	static constexpr int EVENT_DEATH = 3, EVENT_POOF = 60, EVENT_DROWN_PARTICLES = 67;

	LivingEntity(Level& level, int typeId);

	const GameData::EntityTypeInfo& type() const { return _type; }
	AttributeMap&					attributes() { return _attributes; }
	const AttributeMap&				attributes() const { return _attributes; }
	double							getAttributeValue(int attribute) const { return _attributes.getValue(attribute); }
	const AttributeIds&				attributeIds() const { return _ids; }

	float health() const { return _health; }
	void  setHealth(float health);
	float maxHealth() const { return static_cast<float>(getAttributeValue(_ids.maxHealth)); }
	void  heal(float amount);
	bool  isDeadOrDying() const { return _health <= 0.0F; }
	bool  isAlive() const { return !isRemoved() && !isDeadOrDying(); }
	int	  hurtTime() const { return _hurtTime; }
	int	  deathTime() const { return _deathTime; }
	int	  invulnerableTime() const { return _invulnerableTime; }
	int	  airSupply() const { return _airSupply; }
	void  setAirSupply(int air);
	void  setRemainingFireTicks(int ticks);
	// Entity.igniteForSeconds
	void  igniteForTicks(int ticks) {
		 if (_remainingFireTicks < ticks) setRemainingFireTicks(ticks);
	}
	// The player that hurt it recently (entity id, -1 if none): kill credit and player-only loot
	int	  lastHurtByPlayer() const { return _lastHurtByPlayerMemoryTime > 0 ? _lastHurtByPlayer : -1; }
	bool  isResting() const { return _resting; }

	float yHeadRot() const override { return _yHeadRot; }
	void  setYHeadRot(float rot) { _yHeadRot = rot; }
	float yBodyRot() const { return _yBodyRot; }
	void  setYBodyRot(float rot) { _yBodyRot = rot; }
	float eyeHeight() const { return _type.eyeHeight; }
	double eyeY() const { return _position.y + _type.eyeHeight; }

	// Movement input, set by the AI (vanilla's xxa, yya, zza, jumping, speed)
	void  setXxa(float value) { _xxa = value; }
	void  setYya(float value) { _yya = value; }
	void  setZza(float value) { _zza = value; }
	void  setJumping(bool jumping) { _jumping = jumping; }
	float speed() const { return _speed; }
	virtual void setSpeed(float speed) { _speed = speed; }

	void tick() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	// LivingEntity.knockback: pushed away from (x, z) relative to it, less with knockback resistance
	void knockback(double strength, double x, double z);
	virtual void die(const Combat::DamageSource& source);
	bool isLiving() const override { return true; }
	bool isPushable() const override;
	bool fireImmune() const override { return _type.fireImmune; }

	void writeEntityData(Buffer& buf) const override;
	void writeDirtyEntityData(Buffer& buf) const override;
	void clearDirtyEntityData() override { _dirtyData = 0; }
	bool shouldBeSaved() const override { return _type.serializable; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	const GameData::EntityTypeInfo& _type;
	const AttributeIds&				_ids;
	AttributeMap					_attributes;
	float							_health;
	float							_yHeadRot = 0, _yBodyRot = 0;
	float							_xxa = 0, _yya = 0, _zza = 0, _speed = 0;
	bool							_jumping = false;
	int								_noJumpDelay = 0;
	int								_hurtTime = 0, _hurtDuration = 0, _deathTime = 0, _invulnerableTime = 0;
	float							_lastHurt = 0;
	bool							_dead	   = false;
	int								_noActionTime = 0;
	int								_airSupply = 300;
	int								_lastHurtByPlayer = -1, _lastHurtByPlayerMemoryTime = 0;
	bool							_eyeInWater = false, _eyeInBubbleColumn = false;
	bool							_canBreatheUnderwater, _fallDamageImmune, _flyingAnimal, _hostileSounds;
	int								_pose = POSE_STANDING;
	uint8_t							_sharedFlags = 0;
	uint32_t						_dirtyData	 = 0; // Bit per entity data id changed since last sent

	void markData(int id) {
		_dirtyData |= 1u << id;
		entityDataDirty = true;
	}
	// Entity data entries of the ids in `mask` whose value isn't the client's default (all of them if `all`)
	virtual void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const;

	// LivingEntity.baseTick: burning, below the world, air, hurt and death timers
	virtual void livingBaseTick();
	// LivingEntity.aiStep: input, AI (serverAiStep), jumping, travel, blocks, pushing
	virtual void aiStep();
	// Mob.serverAiStep plugs the AI in here
	virtual void serverAiStep() {}
	virtual bool isEffectiveAi() const { return true; }
	// LivingEntity.isImmobile: dead (sleeping isn't ported)
	virtual bool isImmobile() const { return isDeadOrDying(); }
	// Body toward the movement (LivingEntity.tickHeadTurn; mobs use their BodyRotationControl)
	virtual void tickHeadTurn(float bodyTarget);
	virtual int	 maxHeadYRot() const { return 75; }
	void		 travel(const Vec3& input);
	void		 jumpFromGround();
	virtual void tickDeath();
	virtual void dropAllDeathLoot(const Combat::DamageSource& source);
	void		 actuallyHurt(const Combat::DamageSource& source, float amount, int damageType);
	bool		 causeFallDamage(double distance, float multiplier) override;
	void		 checkFallDamage(double dy, bool onGround, const BlockPos& onPos) override;
	void		 onBelowWorld() override;
	float		 blockSpeedFactor() override;
	// Burning, lava and water from the blocks it is inside (the fire and fluid effects of applyEffectsFromBlocks)
	void		 applyFireEffectsFromBlocks();
	void		 pushEntities();
	bool		 onClimbable();
	// Sounds of its type: "minecraft:entity.<type>.<name>" when the game has one, else the fallback
	std::string	 typeSound(const char* name, const char* fallback) const;
	void		 playSound(const std::string& sound, float volume, float pitch);
	float		 voicePitch();
	void		 broadcastEntityEvent(int event);
	virtual void playHurtSound();

  private:
	// The resting state: what the last tick ended with, and the chunks whose blocks it depends on
	struct RestState {
		bool	 valid = false;
		Vec3	 position, delta;
		int		 chunkCount = 0;
		Chunk*	 chunks[4];
		uint64_t versions[4];
	};
	RestState _rest;
	bool	  _resting = false;

	bool restStillHolds();
	void recordRest(const Vec3& before, const Vec3& deltaBefore);
	void travelInAir(const Vec3& input);
	void travelInFluid(const Vec3& input);
	bool isFree(const Vec3& offset);
	void updateFluidOnEyes();
	void updateAir();
	float frictionAt(const BlockPos& pos);
};

#endif
