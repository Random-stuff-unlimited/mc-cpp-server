#include "world/entity/Snowball.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/LivingEntity.hpp"

#include <algorithm>
#include <cmath>

Snowball::Snowball(Level& level, const Vec3& position, const Vec3& delta, Actor* owner)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:snowball"), 0.25f, 0.25f) {
	_position = position;
	_delta	  = delta;
	if (owner) _owner = EntityRef(*owner);
}

Actor* Snowball::owner() const { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

void Snowball::tick() {
	Entity::tick(); // baseTick
	if (isRemoved()) return;
	if (_age++ >= LIFETIME) {
		discard();
		return;
	}
	Vec3 prev = _position;
	applyGravity(0.03);
	move(_delta);
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
	_delta = _delta.scale(0.99);
}

void Snowball::hitEntity(Actor& target) {
	// SnowballEntity.onEntityHit: 3 damage to a blaze, a knockback otherwise
	bool blaze = _level.gameData().getStaticName("minecraft:entity_type", target.typeId()) == "minecraft:blaze";
	if (blaze) {
		if (auto* player = target.asPlayer()) {
			Combat::damage(_level.server(), *player, 3.0F, {"minecraft:thrown", owner(), this, std::nullopt});
		} else if (auto* living = target.asLiving()) {
			living->hurtServer({"minecraft:thrown", owner(), this, std::nullopt}, 3.0F);
		}
	}
	// The knockback: the throw direction, horizontal, times 0.4
	Vec3 knock = _delta.multiply(0.5, 0.0, 0.5).normalize().scale(0.4);
	target.pushMotion(knock);
	discard();
}

void Snowball::hitBlock() { discard(); }