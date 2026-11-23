#include "world/entity/Fireballs.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/blocks/Fire.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/item/Enchantments.hpp"

#include <cmath>

namespace {
	int typeOf(Level& level, const char* type) { return level.gameData().getStaticId("minecraft:entity_type", type); }
} // namespace

HurtingProjectile::HurtingProjectile(Level& level, const char* type, float size, LivingEntity* shooter, const Vec3& direction)
	: Projectile(level, typeOf(level, type), size, size) {
	if (shooter) {
		_position	 = shooter->position();
		_oldPosition = _position;
		setOwner(shooter);
		_yRot = _yRotO = shooter->yRot();
		_xRot = _xRotO = shooter->xRot();
	}
	// assignDirectionalMovement
	_delta	   = direction.normalize().scale(accelerationPower);
	hasImpulse = true;
}

void HurtingProjectile::tick() {
	Actor* owner = this->owner();
	// applyInertia: it speeds up along its movement
	_delta = (_delta + _delta.normalize().scale(accelerationPower)).scale(isInWater() ? liquidInertia() : inertia());
	if (owner && !owner->isAlive() && owner->asEntity() && owner->asEntity()->isRemoved()) {
		discard();
		return;
	}
	// ProjectileUtil.getHitResultOnMoveVector
	Vec3			to	  = _position + _delta;
	Clip::HitResult block = Clip::clip(_level, _position, to, Clip::BlockMode::Collider, Clip::FluidMode::None);
	if (block.hit) to = block.location;
	std::optional<EntityHit> entity = findHitEntity(_position, to, boundingBox().expandTowards(_delta).inflate(1.0), computeMargin());
	bool					 miss	= !entity && !block.hit;
	Vec3					 end	= entity ? entity->location : block.hit ? block.location : _position + _delta;
	// ProjectileUtil.rotateTowardsMovement
	if (_delta.lengthSqr() != 0.0) {
		double horizontal = std::sqrt(_delta.horizontalDistanceSqr());
		float  yRot		  = static_cast<float>(Mth::atan2(_delta.z, _delta.x) * 180.0F / (float)M_PI) + 90.0F;
		float  xRot		  = static_cast<float>(Mth::atan2(horizontal, _delta.y) * 180.0F / (float)M_PI) - 90.0F;
		while (xRot - _xRotO < -180.0F) _xRotO -= 360.0F;
		while (xRot - _xRotO >= 180.0F) _xRotO += 360.0F;
		while (yRot - _yRotO < -180.0F) _yRotO -= 360.0F;
		while (yRot - _yRotO >= 180.0F) _yRotO += 360.0F;
		_xRot = _xRotO + 0.2F * (xRot - _xRotO);
		_yRot = _yRotO + 0.2F * (yRot - _yRotO);
	}
	_position = end;
	_level.checkInsideBlocks(boundingBox(), this);
	Projectile::tick();
	if (isRemoved()) return;
	if (shouldBurn()) igniteForTicks(20);
	if (!miss) {
		Hit hit;
		hit.location = _position;
		if (entity) {
			hit.actor = entity->actor;
		} else {
			hit.pos	 = block.pos;
			hit.face = block.face;
		}
		onHit(hit);
	}
}

void HurtingProjectile::writeEntityData(Buffer& buf) const {
	buf.writeUByte(0); // Entity.DATA_SHARED_FLAGS_ID: on fire
	buf.writeVarInt(0);
	buf.writeUByte(1);
}

void HurtingProjectile::deflectAim(Actor& by, const Vec3& look) {
	_delta			  = look;
	hasImpulse		  = true;
	accelerationPower = 0.1; // onDeflection(true)
	setOwner(&by);
}

// ===================== SmallFireball =====================

SmallFireball::SmallFireball(Level& level, LivingEntity* shooter, const Vec3& direction)
	: HurtingProjectile(level, "minecraft:small_fireball", 0.3125F, shooter, direction) {}

void SmallFireball::onHit(const Hit& hit) {
	if (hit.actor) {
		Actor&				 target = *hit.actor;
		Entity*				 entity = target.asEntity();
		int					 fire	= entity ? entity->remainingFireTicks() : 0;
		Combat::DamageSource source{"minecraft:fireball", owner(), this, std::nullopt};
		target.igniteForTicks(100);
		bool hurt = target.asPlayer() ? Combat::damage(_level.server(), *target.asPlayer(), 5.0F, source) : target.hurtServer(source, 5.0F);
		if (!hurt && entity) entity->setRemainingFireTicks(fire);
	} else {
		// A fire against the face it hit (mobGriefing: always on here)
		BlockPos at = hit.pos.relative(hit.face);
		if (_level.blocks().isAir(_level.getBlockState(at))) _level.setBlock(at, BaseFireBlock::getState(_level, at), Level::UPDATE_ALL);
	}
	discard();
}

// ===================== LargeFireball =====================

LargeFireball::LargeFireball(Level& level, LivingEntity* shooter, const Vec3& direction, int explosionPower)
	: HurtingProjectile(level, "minecraft:fireball", 1.0F, shooter, direction), _explosionPower(explosionPower) {}

void LargeFireball::onHit(const Hit& hit) {
	if (hit.actor) {
		Combat::DamageSource source{"minecraft:fireball", owner(), this, std::nullopt};
		if (Player* player = hit.actor->asPlayer()) {
			Combat::damage(_level.server(), *player, 6.0F, source);
		} else {
			hit.actor->hurtServer(source, 6.0F);
		}
	}
	// Level.explode(this, x, y, z, power, fire: mobGriefing, ExplosionInteraction.MOB)
	Explosions::explode(_level, this, std::nullopt, nullptr, _position, static_cast<float>(_explosionPower), true, Explosions::Interaction::Mob);
	discard();
}
