#include "world/entity/Projectile.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Level.hpp"
#include "world/entity/LivingEntity.hpp"

#include <cmath>

namespace {
	// RandomSource.triangle
	double triangle(JavaRandom& random, double mode, double deviation) { return mode + deviation * (random.nextDouble() - random.nextDouble()); }
} // namespace

Actor* Projectile::owner() const { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

int Projectile::entityData() const {
	Actor* o = owner();
	return o ? o->id() : 0;
}

void Projectile::shoot(double x, double y, double z, float velocity, float inaccuracy) {
	// getMovementToShoot
	Vec3 direction = Vec3{x, y, z}.normalize();
	direction	   = direction + Vec3{triangle(_random, 0.0, 0.0172275 * inaccuracy), triangle(_random, 0.0, 0.0172275 * inaccuracy),
								  triangle(_random, 0.0, 0.0172275 * inaccuracy)};
	_delta		   = direction.scale(velocity);
	hasImpulse	   = true;
	double horizontal = std::sqrt(_delta.horizontalDistanceSqr());
	_yRot			  = static_cast<float>(Mth::atan2(_delta.x, _delta.z) * 180.0F / (float)M_PI);
	_xRot			  = static_cast<float>(Mth::atan2(_delta.y, horizontal) * 180.0F / (float)M_PI);
	_yRotO			  = _yRot;
	_xRotO			  = _xRot;
}

void Projectile::tick() {
	_hasBeenShot = true; // The game event isn't ported
	checkLeftOwner();
	Entity::tick();
	_leftOwnerChecked = false;
}

void Projectile::checkLeftOwner() {
	if (_leftOwner || _leftOwnerChecked) return;
	// isOutsideOwnerCollisionRange: its path (grown by 1) doesn't touch its owner anymore
	Actor* o = owner();
	if (!o) {
		_leftOwner = true;
	} else {
		AABB area  = boundingBox().expandTowards(_delta).inflate(1.0);
		_leftOwner = !(o->isAlive() && !o->isSpectator() && area.intersects(o->boundingBox()));
	}
	_leftOwnerChecked = true;
}

bool Projectile::canHitEntity(Actor& target) {
	// Entity.canBeHitByProjectile: alive and pickable (living entities and players that aren't spectators)
	if (!target.isAlive() || target.isSpectator()) return false;
	if (!target.asPlayer() && !target.asLiving()) return false;
	Actor* o = owner();
	return o == nullptr || _leftOwner || &target != o;
}

std::optional<Projectile::EntityHit> Projectile::findHitEntity(const Vec3& from, const Vec3& to, const AABB& area, float margin) {
	double					 nearest = std::numeric_limits<double>::max();
	std::optional<EntityHit> hit;
	auto					 consider = [&](Actor& actor) {
		if (&actor == static_cast<Actor*>(this) || !canHitEntity(actor)) return;
		std::optional<Vec3> at = Clip::clipBox(actor.boundingBox().inflate(margin), from, to);
		if (!at) return;
		double distance = (*at - from).lengthSqr();
		if (distance < nearest) {
			nearest = distance;
			hit		= EntityHit{&actor, *at};
		}
	};
	// Level.getEntities: the players, then the other entities
	for (const auto& player : _level.players()) {
		if (player->boundingBox().intersects(area)) consider(*player);
	}
	_level.entities().forEachIn(area, [&](Entity& entity) {
		if (!entity.isRemoved()) consider(entity);
	});
	return hit;
}

float Projectile::lerpRotation(float from, float to) {
	while (to - from < -180.0F) from -= 360.0F;
	while (to - from >= 180.0F) from += 360.0F;
	return from + 0.2F * (to - from); // Mth.lerp
}

void Projectile::updateRotation() {
	double horizontal = std::sqrt(_delta.horizontalDistanceSqr());
	_xRot			  = lerpRotation(_xRotO, static_cast<float>(Mth::atan2(_delta.y, horizontal) * 180.0F / (float)M_PI));
	_yRot			  = lerpRotation(_yRotO, static_cast<float>(Mth::atan2(_delta.x, _delta.z) * 180.0F / (float)M_PI));
}

void Projectile::saveProjectile(Buffer& buf) const {
	buf.writeBool(_owner.isSet());
	if (_owner.isSet()) buf.writeUUID(_owner.uuid);
	buf.writeBool(_leftOwner);
	buf.writeBool(_hasBeenShot);
}

void Projectile::loadProjectile(Buffer& buf) {
	_owner.clear();
	if (buf.readBool()) {
		_owner.uuid = buf.readUUID();
		_owner.id	= -2; // Found again by its UUID
	}
	_leftOwner	 = buf.readBool();
	_hasBeenShot = buf.readBool();
}
