#include "world/entity/Arrow.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/LivingEntity.hpp"

#include <algorithm>
#include <cmath>

Arrow::Arrow(Level& level, const Vec3& position, const Vec3& delta, Actor* owner)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:arrow"), 0.5f, 0.5f) {
	_position = position;
	_delta	  = delta;
	if (owner) _owner = EntityRef(*owner);
}

Actor* Arrow::owner() const { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

void Arrow::tick() {
	Entity::tick(); // baseTick: portals, below the world, burning
	if (isRemoved()) return;
	// AbstractArrow: 1200 ticks at most
	if (_age++ >= LIFETIME) {
		discard();
		return;
	}
	Vec3 prev = _position;
	applyGravity(0.05);
	move(_delta);
	if (_horizontalCollision || _verticalCollision) {
		hitBlock();
		return;
	}

	// The volume swept this tick: what the arrow passed through
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
	if (Mth::floor(prev.x) != Mth::floor(_position.x) || Mth::floor(prev.y) != Mth::floor(_position.y) || Mth::floor(prev.z) != Mth::floor(_position.z)) {
		hasImpulse = true;
	}
}

void Arrow::hitEntity(Actor& target) {
	Actor* owner = this->owner();
	if (auto* player = target.asPlayer()) {
		Combat::damage(_level.server(), *player, BASE_DAMAGE, {"minecraft:arrow", owner, this, std::nullopt});
	} else if (auto* living = target.asLiving()) {
		living->hurtServer({"minecraft:arrow", owner, this, std::nullopt}, BASE_DAMAGE);
	}
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, target.isPlayer() ? "minecraft:entity.arrow.hit_player" : "minecraft:entity.arrow.hit",
					   Level::SoundSource::Neutral, 1.0F, 1.0F);
	discard();
}

void Arrow::hitBlock() {
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.arrow.hit", Level::SoundSource::Neutral, 1.0F, 1.0F);
	discard();
}