#include "world/entity/ExperienceOrb.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/EntityManager.hpp"
#include "world/Xp.hpp"

#include <cmath>

ExperienceOrb::ExperienceOrb(Level& level, const Vec3& position, int value)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:experience_orb"), 0.5f, 0.5f), _value(value) {
	_position = position;
	_delta	  = {level.random().nextDouble() * 0.2 - 0.1, 0.2, level.random().nextDouble() * 0.2 - 0.1};
}

std::unique_ptr<ExperienceOrb> ExperienceOrb::create(Level& level, const Vec3& position, int value) {
	if (value <= 0) return nullptr;
	// ExperienceOrb.award: a single orb unless the value is huge (more than MAX_VALUE)
	if (value > MAX_VALUE) {
		std::unique_ptr<ExperienceOrb> first;
		int								remaining = value;
		while (remaining > 0) {
			int split = std::min(MAX_VALUE, remaining);
			remaining -= split;
			auto orb = std::make_unique<ExperienceOrb>(level, position, split);
			if (!first) first = std::move(orb);
			else level.entities().add(std::move(orb));
		}
		return first;
	}
	return std::make_unique<ExperienceOrb>(level, position, value);
}

bool ExperienceOrb::isMergable() const {
	return !isRemoved() && _age < LIFETIME && _value < MAX_VALUE;
}

void ExperienceOrb::mergeWithNeighbours() {
	if (!isMergable()) return;
	// ExperienceOrb.merge: into the oldest nearby orb (up to MAX_VALUE)
	double reach = 0.5;
	Entity* best = nullptr;
	_level.entities().forEachIn(boundingBox().inflate(reach, reach, reach), [&](Entity& other) {
		if (&other == this || other.isRemoved()) return;
		ExperienceOrb* orb = dynamic_cast<ExperienceOrb*>(&other);
		if (!orb || !orb->isMergable()) return;
		if (!best || orb->_age < static_cast<ExperienceOrb*>(best)->_age) best = orb;
	});
	if (!best) return;
	ExperienceOrb& target = *static_cast<ExperienceOrb*>(best);
	target._value		 = std::min(MAX_VALUE, target._value + _value);
	discard();
}

void ExperienceOrb::playerTouch(Player& player) {
	if (_age < 1) return; // ExperienceOrb.age, not picked up on the first tick
	// The pickup sound (ExperienceOrb.playerTouch): to the players, 0.1 volume, a random pitch around 0.9
	_level.playSoundAt(nullptr, player.getX(), player.getY(), player.getZ(), "minecraft:entity.experience_orb.pickup", Level::SoundSource::Players, 0.1F,
					   (_random.nextFloat() - _random.nextFloat()) * 0.35F + 0.9F);
	Xp::addExperience(_level.server(), player, _value);
	discard();
}

void ExperienceOrb::tick() {
	Entity::tick();
	if (isRemoved()) return;
	// The orb's own lifetime and value checks
	if (_value <= 0 || _age >= LIFETIME) {
		discard();
		return;
	}
	Vec3 previous = _position;
	// ExperienceOrb.applyGravity: -0.03 a tick, friction 0.98, bounce 0.8 on the ground
	applyGravity(0.03);
	move(_delta);
	if (_onGround) {
		_delta = _delta.multiply(0.98, -0.8, 0.98);
	} else {
		_delta = _delta.multiply(0.98, 0.98, 0.98);
	}

	// Drawn to the nearest player within 6 blocks (ExperienceOrb.tick), then picked up when touching
	Player* nearest = nullptr;
	double	bestSqr = 6.0 * 6.0;
	for (const auto& other : _level.players()) {
		if (other->isSpectator() || other->combat().dead) continue;
		double dx = other->getX() - _position.x, dy = other->getY() - _position.y, dz = other->getZ() - _position.z;
		double dist = dx * dx + dy * dy + dz * dz;
		if (dist < bestSqr) {
			bestSqr = dist;
			nearest = other.get();
		}
	}
	if (nearest) {
		// Move toward it: 0.03 per tick of the distance, at least 0.5 a tick (ExperienceOrb.moveTowards)
		double dx = nearest->getX() - _position.x, dy = nearest->getY() + 0.5 - _position.y, dz = nearest->getZ() - _position.z;
		double length = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (length > 0.0) {
			double step = 0.03 * length;
			if (step < 0.5) step = 0.5;
			_delta = _delta.multiply(0.98, 0.98, 0.98) + Vec3{dx / length * step, dy / length * step, dz / length * step};
			move(_delta);
			if (nearest->boundingBox().inflate(1.0, 0.5, 1.0).intersects(boundingBox())) {
				playerTouch(*nearest);
				return;
			}
		}
	}

	// Merge every 20 ticks (ExperienceOrb.tick)
	if (_age % 20 == 0 && _age > 0) mergeWithNeighbours();

	bool moved = Mth::floor(previous.x) != Mth::floor(_position.x) || Mth::floor(previous.y) != Mth::floor(_position.y) ||
				 Mth::floor(previous.z) != Mth::floor(_position.z);
	if (moved) hasImpulse = true;
	_age++;
}