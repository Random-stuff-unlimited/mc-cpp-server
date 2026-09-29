#ifndef LIVING_ENTITY_HPP
#define LIVING_ENTITY_HPP

#include "data/GameData.hpp"
#include "world/entity/Attributes.hpp"
#include "world/Fluids.hpp"
#include "world/entity/Entity.hpp"
#include "world/entity/EquipmentSlot.hpp"
#include "world/item/ItemStack.hpp"

#include <array>

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
	// LivingEntity.isBaby: ageable mobs (animals, villagers...) and baby zombies override it
	virtual bool isBaby() const { return false; }
	// Monster.isPreventingPlayerRest: monsters keep players from sleeping nearby (zombified piglins only when angry)
	virtual bool isPreventingPlayerRest(Player&) const { return _type.is("Monster"); }
	bool  isAlive() const override { return !isRemoved() && !isDeadOrDying(); }
	LivingEntity* asLiving() override { return this; }
	int	  hurtTime() const { return _hurtTime; }
	int	  deathTime() const { return _deathTime; }
	int	  invulnerableTime() const { return _invulnerableTime; }
	int	  airSupply() const { return _airSupply; }
	void  setAirSupply(int air);
	void  setRemainingFireTicks(int ticks);
	// Entity.igniteForSeconds
	// The player that hurt it recently (entity id, -1 if none): kill credit and player-only loot
	int	  lastHurtByPlayer() const { return _lastHurtByPlayerMemoryTime > 0 ? _lastHurtByPlayer : -1; }
	bool  isResting() const { return _resting; }

	float yHeadRot() const override { return _yHeadRot; }
	void  setYHeadRot(float rot) { _yHeadRot = rot; }
	float yBodyRot() const { return _yBodyRot; }
	void  setYBodyRot(float rot) { _yBodyRot = rot; }
	float eyeHeight() const override { return _eyeHeightOverride >= 0.0F ? _eyeHeightOverride : _type.eyeHeight; }

	// Movement input, set by the AI (vanilla's xxa, yya, zza, jumping, speed)
	void  setXxa(float value) { _xxa = value; }
	void  setYya(float value) { _yya = value; }
	void  setZza(float value) { _zza = value; }
	void  setJumping(bool jumping) { _jumping = jumping; }
	float speed() const { return _speed; }
	virtual void setSpeed(float speed) { _speed = speed; }

	// ----- Equipment -----

	const ItemStack& getItemBySlot(EquipmentSlot slot) const { return _equipment[static_cast<int>(slot)]; }
	// setItemSlot: the item's attribute modifiers and the viewers follow at the next tick (detectEquipmentUpdates)
	virtual void	 setItemSlot(EquipmentSlot slot, ItemStack stack) { _equipment[static_cast<int>(slot)] = std::move(stack); }
	const ItemStack& getMainHandItem() const { return getItemBySlot(EquipmentSlot::MainHand); }
	// Set Equipment entries for every slot that isn't empty (the pairing data of a new viewer); false if none
	bool			 writeEquipment(Buffer& buf) const;

	// ----- Combat memory -----

	// getLastHurtByMob: who last hurt it (for 100 ticks), null if gone
	Actor* getLastHurtByMob();
	int	   getLastHurtByMobTimestamp() const { return _lastHurtByMobTimestamp; }
	void   setLastHurtByMob(Actor* attacker);
	// getLastHurtMob: the last one it hurt
	Actor* getLastHurtMob();
	void   setLastHurtMob(Actor* target);
	// LivingEntity.getLastDamageSource: the type of the last damage taken, for 40 ticks ("" if none)
	std::string getLastDamageType() const;
	// LivingEntity.swing: the arm animation for the viewers (hand 0 main, 1 off)
	void		swing(int hand);
	// LivingEntity.hasLineOfSight: nothing solid between the eyes, 128 blocks at most
	bool   hasLineOfSight(Actor& target);
	// Entity.isInWater, public (pathfinding asks)
	bool   isInWaterNow() const { return isInWater(); }
	// Entity.isInLiquid
	bool   isInLiquid() const { return isInWater() || isInLava(); }
	virtual bool isAffectedByFluids() const { return true; }
	// LivingEntity.canStandOnFluid: striders on lava
	virtual bool canStandOnFluid(const FluidState&) const { return false; }
	// LivingEntity.maxUpStep: the step_height attribute
	float		 maxUpStep() const { return static_cast<float>(getAttributeValue(_ids.stepHeight)); }
	// LivingEntity.getMaxFallDistance
	virtual int	 getMaxFallDistance() { return 3; }
	// LivingEntity.isInvertedHealAndHarm: undead heal from harming and take damage from healing
	bool		 isInvertedHealAndHarm() const;

	// A loot table rolled for it ("minecraft:shearing/sheep", "minecraft:gameplay/chicken_lay"), with the tool used
	std::vector<ItemStack> rollLootTable(const std::string& table, const ItemStack* tool = nullptr);
	// Its data components for loot predicates ("minecraft:sheep/color" -> "white"), "" if it has none of that kind
	virtual std::string lootComponent(const std::string&) const { return ""; }
	// EntitySubPredicate ("type_specific" of loot predicates: a sheared sheep...), false if not ported for it
	virtual bool		lootTypeSpecific(const nlohmann::json&) const { return false; }

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
	std::array<ItemStack, EQUIPMENT_SLOT_COUNT> _equipment, _lastEquipment;
	EntityRef									_lastHurtByMob, _lastHurtMob;
	int											_lastHurtByMobTimestamp = 0, _lastHurtMobTimestamp = 0;
	std::string									_lastDamageType;
	int64_t										_lastDamageStamp = 0;

	// LivingEntity.detectEquipmentUpdates: modifiers of the changed slots swapped, the viewers told
	void detectEquipmentUpdates();
	void applyItemModifiers(const ItemStack& stack, EquipmentSlot slot, bool add);

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

	// Entity data entries of the subclasses (their ids come after LivingEntity's and Mob's, see
	// .cache/vanilla/entity_data_ids.txt): write one when asked (mask) and, for pairing, when not at its default
	static constexpr int SERIALIZER_BOOLEAN = 8;
	bool wantsData(uint32_t mask, int id, bool onlyNonDefault, bool isDefault) const { return (mask & (1u << id)) && !(onlyNonDefault && isDefault); }
	static void writeByteData(Buffer& buf, int id, uint8_t value);
	static void writeIntData(Buffer& buf, int id, int value);
	static void writeBoolData(Buffer& buf, int id, bool value);
	static void writeFloatData(Buffer& buf, int id, float value);
	// Entity.refreshDimensions: a new size (babies), its eye height too
	void setDimensions(float width, float height, float eyeHeight) {
		_width			   = width;
		_height			   = height;
		_eyeHeightOverride = eyeHeight;
	}
	float _eyeHeightOverride = -1.0F;

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
	// LivingEntity.onClimbable: in a climbable block (spiders: against a wall)
	virtual bool onClimbable();
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
