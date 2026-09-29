#include "world/entity/LivingEntity.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Chunk.hpp"
#include "world/Combat.hpp"
#include "world/Clip.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/ExperienceOrb.hpp"
#include "world/Xp.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
	constexpr int	 MAX_AIR_SUPPLY		  = 300;
	constexpr int	 MAX_ENTITY_CRAMMING  = 24; // Game rule maxEntityCramming, default
	constexpr int	 PLAYER_MEMORY_TICKS  = 100;
	constexpr int	 DEFAULT_PICKUP_DELAY = 10;
	constexpr int	 SAVE_VERSION		  = 1;
	constexpr uint32_t LIVING_DATA = 1u << LivingEntity::DATA_SHARED_FLAGS | 1u << LivingEntity::DATA_AIR_SUPPLY | 1u << LivingEntity::DATA_POSE |
									 1u << LivingEntity::DATA_LIVING_FLAGS | 1u << LivingEntity::DATA_HEALTH;

	const GameData::EntityTypeInfo& typeOf(Level& level, int typeId) {
		const GameData::EntityTypeInfo* type = level.gameData().getEntityType(typeId);
		if (!type || !type->living) throw std::runtime_error("not a living entity type: " + std::to_string(typeId));
		return *type;
	}

	// Block tags looked at every tick, as tables by block id
	struct BlockTables {
		std::vector<bool> climbable;
	};
	const BlockTables& blockTables(const GameData& data) {
		static const BlockTables tables{data.blockTag("minecraft:climbable")};
		return tables;
	}

	bool same(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
} // namespace

LivingEntity::LivingEntity(Level& level, int typeId)
	: Entity(level, typeId, typeOf(level, typeId).width, typeOf(level, typeId).height), _type(typeOf(level, typeId)),
	  _ids(AttributeIds::get(level.gameData())), _attributes(level.gameData(), _type), _health(1.0F) {
	const GameData& data = level.gameData();
	setHealth(maxHealth()); // Not the client's default (1): sent with the spawn
	_yRot	  = static_cast<float>(_random.nextDouble() * (float)(M_PI * 2)); // Radians as degrees: vanilla's Math.random() * 2π
	_yHeadRot = _yRot;
	_canBreatheUnderwater = data.isInTag("minecraft:entity_type", "minecraft:can_breathe_under_water", typeId);
	_fallDamageImmune	  = data.isInTag("minecraft:entity_type", "minecraft:fall_damage_immune", typeId);
	_flyingAnimal		  = _type.is("FlyingAnimal");
	_hostileSounds		  = _type.is("Monster");
}

void LivingEntity::setHealth(float health) {
	float clamped = std::clamp(health, 0.0F, maxHealth());
	if (clamped == _health) return;
	_health = clamped;
	markData(DATA_HEALTH);
}

void LivingEntity::heal(float amount) {
	if (_health > 0.0F) setHealth(_health + amount);
}

void LivingEntity::setAirSupply(int air) {
	if (air == _airSupply) return;
	_airSupply = air;
	markData(DATA_AIR_SUPPLY);
}

void LivingEntity::setRemainingFireTicks(int ticks) { _remainingFireTicks = ticks; }

bool LivingEntity::isPushable() const { return isAlive() && !const_cast<LivingEntity*>(this)->onClimbable(); }

// ----- Equipment -----

void LivingEntity::applyItemModifiers(const ItemStack& stack, EquipmentSlot slot, bool add) {
	if (stack.isEmpty()) return;
	const GameData::ItemProperties* item = _level.gameData().getItemProperties(stack.item);
	if (!item) return;
	for (const auto& entry : item->attributeModifiers) {
		if (!EquipmentSlots::groupContains(entry.slot, slot)) continue;
		AttributeInstance* instance = _attributes.getInstance(entry.attribute);
		if (!instance) continue;
		if (add) {
			instance->removeModifier(entry.id);
			instance->addModifier({entry.id, entry.amount, static_cast<AttributeModifier::Operation>(entry.operation)}, false);
		} else {
			instance->removeModifier(entry.id);
		}
	}
}

void LivingEntity::detectEquipmentUpdates() {
	Buffer changes;
	int	   count = 0;
	std::vector<int> changed;
	for (int i = 0; i < EQUIPMENT_SLOT_COUNT; i++) {
		const ItemStack& now = _equipment[i];
		const ItemStack& before = _lastEquipment[i];
		if (now.item == before.item && now.count == before.count && now.components == before.components) continue;
		EquipmentSlot slot = static_cast<EquipmentSlot>(i);
		applyItemModifiers(before, slot, false);
		applyItemModifiers(now, slot, true);
		_lastEquipment[i] = now;
		changed.push_back(i);
	}
	if (changed.empty()) return;
	// ClientboundSetEquipmentPacket: the entries, each slot flagged when another follows
	changes.writeVarInt(_id);
	for (size_t i = 0; i < changed.size(); i++) {
		changes.writeUByte(static_cast<uint8_t>(changed[i] | (i + 1 < changed.size() ? 0x80 : 0)));
		_equipment[changed[i]].write(changes);
		count++;
	}
	_level.entities().broadcast(*this, PacketId::Play::Clientbound::SET_EQUIPMENT, changes);
}

bool LivingEntity::writeEquipment(Buffer& buf) const {
	std::vector<int> slots;
	for (int i = 0; i < EQUIPMENT_SLOT_COUNT; i++) {
		if (!_equipment[i].isEmpty()) slots.push_back(i);
	}
	if (slots.empty()) return false;
	buf.writeVarInt(_id);
	for (size_t i = 0; i < slots.size(); i++) {
		buf.writeUByte(static_cast<uint8_t>(slots[i] | (i + 1 < slots.size() ? 0x80 : 0)));
		_equipment[slots[i]].write(buf);
	}
	return true;
}

std::string LivingEntity::getLastDamageType() const {
	if (_lastDamageType.empty() || _level.getGameTime() - _lastDamageStamp > 40) return "";
	return _lastDamageType;
}

void LivingEntity::swing(int hand) {
	Buffer animate;
	animate.writeVarInt(_id);
	animate.writeUByte(hand == 0 ? 0 : 3); // SWING_MAIN_HAND, SWING_OFF_HAND
	_level.entities().broadcast(*this, PacketId::Play::Clientbound::ANIMATE, animate);
}

bool LivingEntity::isInvertedHealAndHarm() const {
	return _level.gameData().isInTag("minecraft:entity_type", "minecraft:inverted_healing_and_harm", _typeId);
}

// ----- Combat memory -----

Actor* LivingEntity::getLastHurtByMob() {
	if (!_lastHurtByMob.isSet()) return nullptr;
	Actor* actor = _level.actorByRef(_lastHurtByMob);
	if (!actor) _lastHurtByMob.clear();
	return actor;
}

void LivingEntity::setLastHurtByMob(Actor* attacker) {
	if (attacker) {
		_lastHurtByMob			= EntityRef(*attacker);
		_lastHurtByMobTimestamp = _tickCount;
	} else {
		_lastHurtByMob.clear();
	}
}

Actor* LivingEntity::getLastHurtMob() { return _lastHurtMob.isSet() ? _level.actorByRef(_lastHurtMob) : nullptr; }

void LivingEntity::setLastHurtMob(Actor* target) {
	if (target) {
		_lastHurtMob		  = EntityRef(*target);
		_lastHurtMobTimestamp = _tickCount;
	}
}

bool LivingEntity::hasLineOfSight(Actor& target) {
	if (target.actorLevel() != &_level) return false;
	Vec3 from{_position.x, eyeY(), _position.z};
	Vec3 to{target.position().x, target.eyeY(), target.position().z};
	if ((to - from).length() > 128.0) return false;
	return !Clip::clip(_level, from, to, Clip::BlockMode::Collider, Clip::FluidMode::None).hit;
}

// ----- Tick -----

void LivingEntity::tick() {
	_resting		 = _rest.valid && restStillHolds();
	_skipFluidUpdate = _resting;
	livingBaseTick();
	if (isRemoved()) return;
	detectEquipmentUpdates();
	if (!isRemoved()) aiStep();

	// The body turns toward where it moved
	double dx		= _position.x - _oldPosition.x;
	double dz		= _position.z - _oldPosition.z;
	float  distance = static_cast<float>(dx * dx + dz * dz);
	float  body		= _yBodyRot;
	if (distance > 0.0025000002F) {
		float angle = static_cast<float>(Mth::atan2(dz, dx)) * (180.0F / (float)M_PI) - 90.0F;
		float diff	= std::abs(Mth::wrapDegrees(_yRot) - angle);
		body		= 95.0F < diff && diff < 265.0F ? angle - 180.0F : angle;
	}
	tickHeadTurn(body);
}

void LivingEntity::tickHeadTurn(float bodyTarget) {
	_yBodyRot += Mth::wrapDegrees(bodyTarget - _yBodyRot) * 0.3F;
	float head = Mth::wrapDegrees(_yRot - _yBodyRot);
	float max  = 50.0F; // getMaxHeadRotationRelativeToBody
	if (std::abs(head) > max) _yBodyRot += head - (head > 0 ? 1.0F : -1.0F) * max;
}

void LivingEntity::livingBaseTick() {
	Entity::baseTick();
	uint8_t flags = static_cast<uint8_t>((_sharedFlags & ~1) | (_remainingFireTicks > 0 ? 1 : 0)); // setSharedFlagOnFire
	if (flags != _sharedFlags) {
		_sharedFlags = flags;
		markData(DATA_SHARED_FLAGS);
	}
	if (isAlive()) {
		// In-wall suffocation isn't ported (needs each block's isSuffocating)
		if (!_resting) updateFluidOnEyes();
		updateAir();
	}
	if (_hurtTime > 0) _hurtTime--;
	if (_invulnerableTime > 0) _invulnerableTime--;
	if (isDeadOrDying()) tickDeath();
	if (_lastHurtByPlayerMemoryTime > 0) {
		_lastHurtByPlayerMemoryTime--;
	} else {
		_lastHurtByPlayer = -1;
	}
	// The attacker is forgotten after 100 ticks, or when it is gone
	if (_lastHurtByMob.isSet() && (_tickCount - _lastHurtByMobTimestamp > 100 || !getLastHurtByMob())) _lastHurtByMob.clear();
}

// Entity.updateFluidOnEyes: water above the eyes, and whether it is a bubble column (air comes from it)
void LivingEntity::updateFluidOnEyes() {
	double	 eye = eyeY();
	BlockPos pos{Mth::floor(_position.x), Mth::floor(eye), Mth::floor(_position.z)};
	int		 state = _level.getBlockState(pos);
	FluidState fluid = _level.fluids().stateOf(state);
	_eyeInWater		  = _level.fluids().isWater(fluid.type) && pos.y + _level.fluids().height(fluid, pos) > eye;
	_eyeInBubbleColumn = _level.gameData().getBlocks().blockOf(state) == _level.bubbleColumnBlock();
}

void LivingEntity::updateAir() {
	if (_eyeInWater && !_eyeInBubbleColumn) {
		if (!_canBreatheUnderwater) {
			// decreaseAirSupply: oxygen bonus sometimes saves the air
			double bonus = _attributes.hasAttribute(_ids.oxygenBonus) ? getAttributeValue(_ids.oxygenBonus) : 0.0;
			setAirSupply(bonus > 0.0 && _random.nextDouble() >= 1.0 / (bonus + 1.0) ? _airSupply : _airSupply - 1);
			if (_airSupply <= -20) {
				setAirSupply(0);
				broadcastEntityEvent(EVENT_DROWN_PARTICLES);
				hurtServer({"minecraft:drown", nullptr}, 2.0F);
			}
		} else if (_airSupply < MAX_AIR_SUPPLY) {
			setAirSupply(std::min(_airSupply + 4, MAX_AIR_SUPPLY));
		}
	} else if (_airSupply < MAX_AIR_SUPPLY) {
		setAirSupply(std::min(_airSupply + 4, MAX_AIR_SUPPLY)); // increaseAirSupply
	}
}

void LivingEntity::tickDeath() {
	_deathTime++;
	if (_deathTime >= DEATH_DURATION && !isRemoved()) {
		broadcastEntityEvent(EVENT_POOF);
		discard();
	}
}

void LivingEntity::aiStep() {
	if (_noJumpDelay > 0) _noJumpDelay--;
	// Tiny movements stop
	if (std::abs(_delta.x) < 0.003) _delta.x = 0.0;
	if (std::abs(_delta.z) < 0.003) _delta.z = 0.0;
	if (std::abs(_delta.y) < 0.003) _delta.y = 0.0;
	// applyInput
	_xxa *= 0.98F;
	_zza *= 0.98F;
	if (isImmobile()) {
		_jumping = false;
		_xxa = _zza = 0.0F;
	} else if (isEffectiveAi()) {
		serverAiStep();
	}

	if (_jumping) {
		double height	   = isInLava() ? _lavaHeight : _waterHeight;
		bool   inWater	   = isInWater() && height > 0.0;
		double threshold   = eyeHeight() < 0.4 ? 0.0 : 0.4; // getFluidJumpThreshold
		if (!inWater || (_onGround && !(height > threshold))) {
			if (!isInLava() || (_onGround && !(height > threshold))) {
				if ((_onGround || (inWater && height <= threshold)) && _noJumpDelay == 0) {
					jumpFromGround();
					_noJumpDelay = 10;
				}
			} else {
				_delta.y += 0.04F; // jumpInLiquid
			}
		} else {
			_delta.y += 0.04F;
		}
	} else {
		_noJumpDelay = 0;
	}

	if (isEffectiveAi()) {
		Vec3 input{_xxa, _yya, _zza};
		bool still = input.lengthSqr() == 0.0 && !_jumping;
		if (still && _resting && same(_position, _rest.position) && same(_delta, _rest.delta)) {
			// Resting: this travel would end exactly where the last one did
		} else {
			Vec3 before = _position, deltaBefore = _delta;
			travel(input);
			if (still) {
				recordRest(before, deltaBefore);
			} else {
				_rest.valid = false;
			}
		}
	}
	applyFireEffectsFromBlocks();
	pushEntities();
}

// ----- Resting -----

void LivingEntity::recordRest(const Vec3& before, const Vec3& deltaBefore) {
	_rest.valid = false;
	if (!_onGround || isInWater() || isInLava() || !same(before, _position) || !same(deltaBefore, _delta) || _removed) return;
	AABB box  = boundingBox().inflate(1.0, 1.0, 1.0);
	int	 minX = Mth::floor(box.minX) >> 4, maxX = Mth::floor(box.maxX) >> 4;
	int	 minZ = Mth::floor(box.minZ) >> 4, maxZ = Mth::floor(box.maxZ) >> 4;
	if ((maxX - minX + 1) * (maxZ - minZ + 1) > 4) return; // Huge mobs never rest
	_rest.chunkCount = 0;
	for (int x = minX; x <= maxX; x++) {
		for (int z = minZ; z <= maxZ; z++) {
			Chunk* chunk = _level.loadedChunk(x, z);
			if (!chunk) return;
			_rest.chunks[_rest.chunkCount]	 = chunk;
			_rest.versions[_rest.chunkCount] = chunk->version();
			_rest.chunkCount++;
		}
	}
	_rest.position = _position;
	_rest.delta	   = _delta;
	_rest.valid	   = true;
}

bool LivingEntity::restStillHolds() {
	if (!same(_position, _rest.position) || !same(_delta, _rest.delta)) return false;
	AABB box  = boundingBox().inflate(1.0, 1.0, 1.0);
	int	 minX = Mth::floor(box.minX) >> 4, maxX = Mth::floor(box.maxX) >> 4;
	int	 minZ = Mth::floor(box.minZ) >> 4, maxZ = Mth::floor(box.maxZ) >> 4;
	int	 i	  = 0;
	for (int x = minX; x <= maxX; x++) {
		for (int z = minZ; z <= maxZ; z++, i++) {
			// Looked up again: an unloaded chunk isn't the one recorded (never dereferenced before this matches)
			Chunk* chunk = _level.loadedChunk(x, z);
			if (i >= _rest.chunkCount || chunk != _rest.chunks[i] || chunk->version() != _rest.versions[i]) return false;
		}
	}
	return i == _rest.chunkCount;
}

// ----- Movement (LivingEntity.travel) -----

float LivingEntity::frictionAt(const BlockPos& pos) { return _level.gameData().getBlockProperties(_level.getBlockState(pos)).friction; }

float LivingEntity::blockSpeedFactor() {
	float efficiency = static_cast<float>(getAttributeValue(_ids.movementEfficiency));
	float base		 = Entity::blockSpeedFactor();
	return base + efficiency * (1.0F - base); // Mth.lerp
}

void LivingEntity::travel(const Vec3& input) {
	// Fluids that can be stood on (striders on lava) aren't ported
	if (isInWater() || isInLava()) {
		travelInFluid(input);
	} else {
		travelInAir(input);
	}
}

bool LivingEntity::onClimbable() {
	int block = _level.gameData().getBlocks().blockOf(_level.getBlockState(blockPosition()));
	const std::vector<bool>& climbable = blockTables(_level.gameData()).climbable;
	return block >= 0 && static_cast<size_t>(block) < climbable.size() && climbable[block]; // Trapdoors over ladders aren't ported
}

void LivingEntity::travelInAir(const Vec3& input) {
	BlockPos below	  = blockPosBelowAffectingMovement();
	float	 friction = _onGround ? frictionAt(below) : 1.0F;
	float	 inertia  = friction * 0.91F;
	// handleRelativeFrictionAndCalculateMovement
	float speed = _onGround ? _speed * (0.21600002F / (friction * friction * friction)) : 0.02F;
	moveRelative(speed, input);
	if (onClimbable()) {
		// handleOnClimbable: slow on ladders, no fall damage
		resetFallDistance();
		_delta = {std::clamp(_delta.x, -0.15F * 1.0, 0.15F * 1.0), std::max(_delta.y, -0.15F * 1.0), std::clamp(_delta.z, -0.15F * 1.0, 0.15F * 1.0)};
	}
	move(_delta);
	Vec3 movement = _delta;
	if ((_horizontalCollision || _jumping) && onClimbable()) movement.y = 0.2;

	double y = movement.y - getAttributeValue(_ids.gravity); // Levitation and slow falling aren't ported
	_delta	 = {movement.x * inertia, y * (_flyingAnimal ? inertia : 0.98F), movement.z * inertia};
}

void LivingEntity::travelInFluid(const Vec3& input) {
	bool   falling = _delta.y <= 0.0;
	double startY  = _position.y;
	double gravity = getAttributeValue(_ids.gravity);
	auto   fallingAdjusted = [&](Vec3 movement) {
		  // getFluidFallingAdjustedMovement
		  if (gravity != 0.0) {
			  if (falling && std::abs(movement.y - 0.005) >= 0.003 && std::abs(movement.y - gravity / 16.0) < 0.003) {
				  movement.y = -0.003;
			  } else {
				  movement.y -= gravity / 16.0;
			  }
		  }
		  return movement;
	};
	if (isInWater()) {
		float slowDown	 = 0.8F; // getWaterSlowDown
		float speed		 = 0.02F;
		float efficiency = static_cast<float>(getAttributeValue(_ids.waterMovementEfficiency));
		if (!_onGround) efficiency *= 0.5F;
		if (efficiency > 0.0F) {
			slowDown += (0.54600006F - slowDown) * efficiency;
			speed += (_speed - speed) * efficiency;
		}
		moveRelative(speed, input);
		move(_delta);
		Vec3 movement = _delta;
		if (_horizontalCollision && onClimbable()) movement.y = 0.2;
		movement = movement.multiply(slowDown, 0.8F, slowDown);
		_delta	 = fallingAdjusted(movement);
	} else {
		moveRelative(0.02F, input);
		move(_delta);
		if (_lavaHeight <= (eyeHeight() < 0.4 ? 0.0 : 0.4)) {
			_delta = fallingAdjusted(_delta.multiply(0.5, 0.8F, 0.5));
		} else {
			_delta = _delta.scale(0.5);
		}
		if (gravity != 0.0) _delta.y += -gravity / 4.0;
	}
	if (_horizontalCollision && isFree({_delta.x, _delta.y + 0.6F - _position.y + startY, _delta.z})) _delta.y = 0.3F;
}

// Entity.isFree: no block collision and no fluid in the moved box
bool LivingEntity::isFree(const Vec3& offset) {
	AABB box = boundingBox().move(offset);
	if (!noCollision(box)) return false;
	for (int x = Mth::floor(box.minX); x < Mth::ceil(box.maxX); x++) {
		for (int y = Mth::floor(box.minY); y < Mth::ceil(box.maxY); y++) {
			for (int z = Mth::floor(box.minZ); z < Mth::ceil(box.maxZ); z++) {
				if (_level.getFluidState({x, y, z}).type != 0) return false;
			}
		}
	}
	return true;
}

void LivingEntity::jumpFromGround() {
	float power = static_cast<float>(getAttributeValue(_ids.jumpStrength)) * blockJumpFactor(); // No jump boost
	if (power <= 1.0E-5F) return;
	_delta.y   = std::max(static_cast<double>(power), _delta.y);
	hasImpulse = true;
}

// ----- Blocks around -----

void LivingEntity::checkFallDamage(double dy, bool onGround, const BlockPos& onPos) {
	if (!isInWater()) updateInWaterStateAndDoWaterCurrentPushing();
	Entity::checkFallDamage(dy, onGround, onPos); // The block particles of a hard landing aren't sent
}

bool LivingEntity::causeFallDamage(double distance, float multiplier) {
	if (_fallDamageImmune) return false;
	double power  = distance + 1.0E-6 - getAttributeValue(_ids.safeFallDistance);
	int	   damage = Mth::floor(power * multiplier * getAttributeValue(_ids.fallDamageMultiplier));
	if (damage <= 0) return false;
	// getFallDamageSound: monsters use the hostile sounds (the block's own fall sound isn't known)
	const char* size = damage > 4 ? "big_fall" : "small_fall";
	playSound(std::string(_hostileSounds ? "minecraft:entity.hostile." : "minecraft:entity.generic.") + size, 1.0F, 1.0F);
	hurtServer({"minecraft:fall", nullptr}, static_cast<float>(damage));
	return true;
}

void LivingEntity::onBelowWorld() { hurtServer({"minecraft:out_of_world", nullptr}, 4.0F); }

// The fire and fluid effects of Entity.applyEffectsFromBlocks, in InsideBlockEffectType order: fire, lava, water
void LivingEntity::applyFireEffectsFromBlocks() {
	if (_removed || _noPhysics) return;
	const GameData& data	 = _level.gameData();
	Fluids&			fluids	 = _level.fluids();
	AABB			box		 = boundingBox().deflate(1.0E-5);
	float			fire	 = 0.0F;
	bool			lava	 = false, water = false;
	for (int x = Mth::floor(box.minX); x <= Mth::floor(box.maxX); x++) {
		for (int y = Mth::floor(box.minY); y <= Mth::floor(box.maxY); y++) {
			for (int z = Mth::floor(box.minZ); z <= Mth::floor(box.maxZ); z++) {
				BlockPos   pos{x, y, z};
				int		   state = _level.getBlockState(pos);
				int		   block = data.getBlocks().blockOf(state);
				if (block == _level.fireBlock()) fire = std::max(fire, 1.0F);
				if (block == _level.soulFireBlock()) fire = std::max(fire, 2.0F);
				FluidState fluid = fluids.stateOf(state);
				if (fluid.type == 0 || y + fluids.height(fluid, pos) < box.minY) continue;
				if (fluids.isLava(fluid.type)) lava = true;
				if (fluids.isWater(fluid.type)) water = true;
			}
		}
	}
	int before = _remainingFireTicks;
	if (fire > 0.0F && !fireImmune()) {
		// BaseFireBlock.fireIgnite, then the fire's damage
		if (_remainingFireTicks < 0) setRemainingFireTicks(_remainingFireTicks + 1);
		if (_remainingFireTicks >= 0) igniteForTicks(160);
		hurtServer({"minecraft:in_fire", nullptr}, fire);
	}
	if (lava && !fireImmune() && !_removed) {
		igniteForTicks(300); // lavaIgnite: 15 seconds
		if (hurtServer({"minecraft:lava", nullptr}, 4.0F)) {
			playSound("minecraft:entity.generic.burn", 0.4F, 2.0F + _random.nextFloat() * 0.4F); // Entity.lavaHurt
		}
	}
	if (water) clearFire(); // EXTINGUISH
	if (!isOnFire() && _remainingFireTicks <= before) setRemainingFireTicks(-1); // -getFireImmuneTicks()
}

// LivingEntity.pushEntities: pushed apart from the entities it overlaps; crushed when too many (maxEntityCramming)
void LivingEntity::pushEntities() {
	AABB box   = boundingBox();
	int	 count = 0;
	_level.entities().forEachIn(box, [&](Entity& other) {
		if (&other != this && other.isPushable()) count++;
	});
	if (count > MAX_ENTITY_CRAMMING - 1 && _random.nextInt(4) == 0) hurtServer({"minecraft:cramming", nullptr}, 6.0F); // No passengers
	if (count > 0) {
		_level.entities().forEachIn(box, [&](Entity& other) {
			if (&other != this && other.isPushable()) other.pushAgainst(*this); // doPush: other.push(this)
		});
	}
	// Players it touches push it (their Player.push(this) is the half that concerns it)
	for (const auto& player : _level.players()) {
		if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator || player->combat().dead) continue;
		double half = Player::BB_WIDTH / 2.0;
		AABB   playerBox{player->getX() - half, player->getY(), player->getZ() - half, player->getX() + half, player->getY() + Player::BB_HEIGHT,
						 player->getZ() + half};
		if (!playerBox.intersects(box) || _noPhysics) continue;
		double dx = _position.x - player->getX(), dz = _position.z - player->getZ();
		double largest = Mth::absMax(dx, dz);
		if (!(largest >= 0.01F)) continue;
		largest = std::sqrt(largest);
		double factor = std::min(1.0 / largest, 1.0);
		if (isPushable()) push(dx / largest * factor * 0.05F, 0.0, dz / largest * factor * 0.05F);
	}
}

// ----- Damage -----

bool LivingEntity::hurtServer(const Combat::DamageSource& source, float amount) {
	const GameData& data	   = _level.gameData();
	int				damageType = data.getSyncedId("minecraft:damage_type", source.type);
	auto			tagged	   = [&](const char* tag) { return data.isInTag("minecraft:damage_type", tag, damageType); };
	// isInvulnerableTo
	if (isRemoved() || (tagged("minecraft:is_fire") && fireImmune()) || (tagged("minecraft:is_fall") && _fallDamageImmune)) return false;
	if (isDeadOrDying()) return false;
	_noActionTime = 0;
	if (amount < 0.0F) amount = 0.0F;
	if (tagged("minecraft:is_freezing") && data.isInTag("minecraft:entity_type", "minecraft:freeze_hurts_extra_types", _typeId)) amount *= 5.0F;
	if (std::isnan(amount) || std::isinf(amount)) amount = 3.4028235E38F;

	bool fullHit = true;
	if (_invulnerableTime > 10.0F && !tagged("minecraft:bypasses_cooldown")) {
		// Hit again while flashing: only what exceeds the last hit counts
		if (amount <= _lastHurt) return false;
		actuallyHurt(source, amount - _lastHurt, damageType);
		_lastHurt = amount;
		fullHit	  = false;
	} else {
		_lastHurt		  = amount;
		_invulnerableTime = 20;
		actuallyHurt(source, amount, damageType);
		_hurtDuration = 10;
		_hurtTime	  = _hurtDuration;
	}
	// resolveMobResponsibleForDamage: a living attacker (a player too) is remembered, for 100 ticks
	if (source.causing && (source.causing->asLiving() || source.causing->isPlayer()) &&
		!data.isInTag("minecraft:damage_type", "minecraft:no_anger", damageType)) {
		setLastHurtByMob(source.causing);
	}
	if (Player* player = source.attackerPlayer()) {
		// resolvePlayerResponsibleForDamage
		_lastHurtByPlayer			= player->getPlayerID();
		_lastHurtByPlayerMemoryTime = PLAYER_MEMORY_TICKS;
	}
	if (fullHit) {
		// ServerLevel.broadcastDamageEvent: the red flash and hurt animation; ids of the source entities are sent + 1
		Buffer event;
		event.writeVarInt(_id);
		event.writeVarInt(damageType);
		event.writeVarInt(source.causing ? source.causing->id() + 1 : 0);
		event.writeVarInt(source.directEntity() ? source.directEntity()->id() + 1 : 0);
		event.writeBool(source.position.has_value()); // sourcePositionRaw
		if (source.position) {
			event.writeDouble(source.position->x);
			event.writeDouble(source.position->y);
			event.writeDouble(source.position->z);
		}
		_level.entities().broadcast(*this, PacketId::Play::Clientbound::DAMAGE_EVENT, event);
		if (!tagged("minecraft:no_impact")) hurtMarked = true; // markHurt
		if (!tagged("minecraft:no_knockback")) {
			double dx = 0.0, dz = 0.0;
			if (std::optional<Vec3> from = source.sourcePosition()) {
				dx = from->x - _position.x;
				dz = from->z - _position.z;
			}
			knockback(0.4F, dx, dz);
		}
	}
	_lastDamageType	 = source.type;
	_lastDamageStamp = _level.getGameTime();
	if (isDeadOrDying()) {
		if (fullHit) playSound(typeSound("death", "minecraft:entity.generic.death"), 1.0F, voicePitch());
		die(source);
	} else if (fullHit) {
		playHurtSound();
	}
	return true;
}

void LivingEntity::playHurtSound() { playSound(typeSound("hurt", "minecraft:entity.generic.hurt"), 1.0F, voicePitch()); }

void LivingEntity::actuallyHurt(const Combat::DamageSource& source, float amount, int damageType) {
	(void)source;
	if (!_level.gameData().isInTag("minecraft:damage_type", "minecraft:bypasses_armor", damageType)) {
		// CombatRules.getDamageAfterAbsorb (armor from attributes: equipment isn't ported)
		float armor		= static_cast<float>(Mth::floor(getAttributeValue(_ids.armor)));
		float toughness = 2.0F + static_cast<float>(getAttributeValue(_ids.armorToughness)) / 4.0F;
		float effective = Mth::clamp(armor - amount / toughness, armor * 0.2F, 20.0F) / 25.0F;
		amount *= 1.0F - effective;
	}
	// No effects, enchantments or absorption yet
	if (amount != 0.0F) setHealth(_health - amount);
}

void LivingEntity::knockback(double strength, double x, double z) {
	strength *= 1.0 - getAttributeValue(_ids.knockbackResistance);
	if (strength <= 0.0) return;
	hasImpulse = true;
	while (x * x + z * z < 1.0E-5F) {
		x = (_random.nextDouble() - _random.nextDouble()) * 0.01;
		z = (_random.nextDouble() - _random.nextDouble()) * 0.01;
	}
	Vec3 push = Vec3{x, 0.0, z}.normalize().scale(strength);
	_delta	  = {_delta.x / 2.0 - push.x, _onGround ? std::min(0.4, _delta.y / 2.0 + strength) : _delta.y, _delta.z / 2.0 - push.z};
}

void LivingEntity::die(const Combat::DamageSource& source) {
	if (isRemoved() || _dead) return;
	_dead = true;
	dropAllDeathLoot(source);
	broadcastEntityEvent(EVENT_DEATH);
	if (_pose != POSE_DYING) {
		_pose = POSE_DYING;
		markData(DATA_POSE);
	}
}

// LivingEntity.dropAllDeathLoot: the type's loot table (no experience orbs yet, no equipment)
void LivingEntity::dropAllDeathLoot(const Combat::DamageSource& source) {
	if (_type.lootTable.empty()) return;
	LootTables::Context context{_level, blockPosition(), 0};
	context.hasEntity	   = true;
	context.entity		   = this;
	context.damage		   = &source;
	context.killedByPlayer = _lastHurtByPlayerMemoryTime > 0;
	for (ItemStack& stack : _level.loot().entityDrops(_type.lootTable, context)) {
		// Entity.spawnAtLocation: at its feet, the item's usual random push, the default pickup delay
		if (stack.isEmpty()) continue;
		auto item = ItemEntity::create(_level, _position, std::move(stack));
		item->setPickupDelay(DEFAULT_PICKUP_DELAY);
		_level.entities().add(std::move(item));
	}
	// Its XP, when a player killed it (LivingEntity.dropExperience): split into orbs
	if (context.killedByPlayer) {
		int xp = Xp::mobXp(_level.gameData(), _typeId);
		while (xp > 0) {
			int split = std::min(20, xp);
			xp -= split;
			if (auto orb = ExperienceOrb::create(_level, _position, split)) _level.entities().add(std::move(orb));
		}
	}
}

std::vector<ItemStack> LivingEntity::rollLootTable(const std::string& table, const ItemStack* tool) {
	LootTables::Context context{_level, blockPosition(), 0};
	context.hasEntity = true;
	context.entity	  = this;
	context.tool	  = tool;
	return _level.loot().entityDrops(table, context);
}

// ----- Sounds and events -----

std::string LivingEntity::typeSound(const char* name, const char* fallback) const {
	const std::string& type	 = _level.gameData().getStaticName("minecraft:entity_type", _typeId);
	std::string		   sound = "minecraft:entity." + type.substr(type.find(':') + 1) + "." + name;
	return _level.gameData().getStaticId("minecraft:sound_event", sound) >= 0 ? sound : fallback;
}

void LivingEntity::playSound(const std::string& sound, float volume, float pitch) {
	if (sound.empty()) return;
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, sound, _hostileSounds ? Level::SoundSource::Hostile : Level::SoundSource::Neutral,
					   volume, pitch);
}

float LivingEntity::voicePitch() { return (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F; }

void LivingEntity::broadcastEntityEvent(int event) {
	Buffer data;
	data.writeInt(_id);
	data.writeByte(static_cast<int8_t>(event));
	_level.entities().broadcast(*this, PacketId::Play::Clientbound::ENTITY_EVENT, data);
}

// ----- Entity data -----

void LivingEntity::writeEntityData(Buffer& buf) const { writeData(buf, ~0u, true); }

void LivingEntity::writeDirtyEntityData(Buffer& buf) const { writeData(buf, _dirtyData, false); }

void LivingEntity::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	auto wanted = [&](int id, bool isDefault) { return (mask & LIVING_DATA & (1u << id)) && !(onlyNonDefault && isDefault); };
	if (wanted(DATA_SHARED_FLAGS, _sharedFlags == 0)) {
		buf.writeUByte(DATA_SHARED_FLAGS);
		buf.writeVarInt(SERIALIZER_BYTE);
		buf.writeUByte(_sharedFlags);
	}
	if (wanted(DATA_AIR_SUPPLY, _airSupply == MAX_AIR_SUPPLY)) {
		buf.writeUByte(DATA_AIR_SUPPLY);
		buf.writeVarInt(SERIALIZER_INT);
		buf.writeVarInt(_airSupply);
	}
	if (wanted(DATA_POSE, _pose == POSE_STANDING)) {
		buf.writeUByte(DATA_POSE);
		buf.writeVarInt(SERIALIZER_POSE);
		buf.writeVarInt(_pose);
	}
	if (wanted(DATA_HEALTH, _health == 1.0F)) {
		buf.writeUByte(DATA_HEALTH);
		buf.writeVarInt(SERIALIZER_FLOAT);
		buf.writeFloat(_health);
	}
}

void LivingEntity::writeByteData(Buffer& buf, int id, uint8_t value) {
	buf.writeUByte(static_cast<uint8_t>(id));
	buf.writeVarInt(SERIALIZER_BYTE);
	buf.writeUByte(value);
}
void LivingEntity::writeIntData(Buffer& buf, int id, int value) {
	buf.writeUByte(static_cast<uint8_t>(id));
	buf.writeVarInt(SERIALIZER_INT);
	buf.writeVarInt(value);
}
void LivingEntity::writeBoolData(Buffer& buf, int id, bool value) {
	buf.writeUByte(static_cast<uint8_t>(id));
	buf.writeVarInt(SERIALIZER_BOOLEAN);
	buf.writeBool(value);
}
void LivingEntity::writeFloatData(Buffer& buf, int id, float value) {
	buf.writeUByte(static_cast<uint8_t>(id));
	buf.writeVarInt(SERIALIZER_FLOAT);
	buf.writeFloat(value);
}

// ----- Saving -----

void LivingEntity::save(Buffer& buf) const {
	buf.writeUByte(SAVE_VERSION);
	buf.writeUUID(_uuid);
	for (double v : {_position.x, _position.y, _position.z, _delta.x, _delta.y, _delta.z}) buf.writeDouble(v);
	for (float v : {_yRot, _xRot, _yHeadRot, _yBodyRot}) buf.writeFloat(v);
	buf.writeBool(_onGround);
	buf.writeDouble(_fallDistance);
	buf.writeShort(static_cast<int16_t>(std::clamp(_remainingFireTicks, -32768, 32767)));
	buf.writeShort(static_cast<int16_t>(_airSupply));
	buf.writeFloat(_health);
	buf.writeShort(static_cast<int16_t>(_hurtTime));
	buf.writeShort(static_cast<int16_t>(_deathTime));
	_attributes.save(buf);
}

void LivingEntity::load(Buffer& buf) {
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown living entity data version");
	_uuid = buf.readUUID();
	_position.x = buf.readDouble();
	_position.y = buf.readDouble();
	_position.z = buf.readDouble();
	_delta.x	= buf.readDouble();
	_delta.y	= buf.readDouble();
	_delta.z	= buf.readDouble();
	_yRot		= buf.readFloat();
	_xRot		= buf.readFloat();
	_yHeadRot	= buf.readFloat();
	_yBodyRot	= buf.readFloat();
	_onGround	= buf.readBool();
	_fallDistance		= buf.readDouble();
	_remainingFireTicks = buf.readShort();
	_airSupply			= buf.readShort();
	float health		= buf.readFloat();
	_hurtTime			= buf.readShort();
	_deathTime			= buf.readShort();
	_attributes.load(buf); // Before the health: the maximum may have changed
	_health = std::clamp(health, 0.0F, maxHealth());
	if (_health <= 0.0F) _pose = POSE_DYING;
	_oldPosition = _position;
	_attributes.clearDirty();
	_dirtyData = 0;
}
