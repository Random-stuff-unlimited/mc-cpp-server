#include "world/entity/SmallFireball.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/LivingEntity.hpp"

#include <algorithm>
#include <cmath>

SmallFireball::SmallFireball(Level& level, const Vec3& position, const Vec3& delta, Actor* owner)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:small_fireball"), 0.3125f, 0.3125f) {
	_position = position;
	_delta	  = delta;
	if (owner) _owner = EntityRef(*owner);
}

Actor* SmallFireball::owner() const { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

void SmallFireball::tick() {
	Entity::tick(); // baseTick
	if (isRemoved()) return;
	if (_age++ >= LIFETIME) {
		discard();
		return;
	}
	Vec3 prev = _position;
	move(_delta); // No gravity: it flies straight
	if (_horizontalCollision || _verticalCollision) {
		hitBlock();
		return;
	}

	AABB box	 = boundingBox();
	AABB prevBox = boundingBox().move(prev.x - _position.x, prev.y - _position.y, prev.z - _position.z);
	AABB span{std::min(box.minX, prevBox.minX), std::min(box.minY, prevBox.minY), std::min(box.minZ, prevBox.minZ),
			 std::max(box.maxX, prevBox.maxX), std::max(box.maxY, prevBox.maxY), std::max(box.maxZ, prevBox.maxZ)};
	Actor* owner = this->owner();
	for (const auto& other : _level.players()) {
		if (other->isSpectator() || other->combat().dead || other.get() == owner) continue;
		if (other->boundingBox().intersects(span)) {
			hitEntity(*other);
			return;
		}
	}
	Actor* hit = nullptr;
	_level.entities().forEachIn(span, [&](Entity& entity) {
		if (hit || &entity == this || entity.isRemoved() || &entity == owner) return;
		hit = &entity;
	});
	if (hit) {
		hitEntity(*hit);
		return;
	}
	_delta = _delta.scale(0.98); // ExplosiveProjectile: a little drag
}

void SmallFireball::hitEntity(Actor& target) {
	// SmallFireballEntity.onEntityHit: 5 damage from a fireball, then 5 seconds on fire
	if (auto* player = target.asPlayer()) {
		Combat::damage(_level.server(), *player, 5.0F, {"minecraft:fireball", owner(), this, std::nullopt});
	} else if (auto* living = target.asLiving()) {
		living->hurtServer({"minecraft:fireball", owner(), this, std::nullopt}, 5.0F);
	}
target.igniteForTicks(100); // setOnFireFor(5)
	discard();
}

void SmallFireball::hitBlock() {
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:block.fire.ambient", Level::SoundSource::Neutral, 1.0F, 1.0F);
	discard();
}