#include "world/entity/ThrownExperienceBottle.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ExperienceOrb.hpp"

#include <cmath>

ThrownExperienceBottle::ThrownExperienceBottle(Level& level, Player& player)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:experience_bottle"), 0.25f, 0.25f) {
	// ThrowableItemProjectile: at the player's eye
	_position = {player.getX(), player.eyeY() - 0.1, player.getZ()};
	// Projectile.shoot: along the player's look (speed 0.7, divergence 1.0)
	float yaw	 = player.getYaw() * static_cast<float>(M_PI) / 180.0F;
	float pitch	 = player.getPitch() * static_cast<float>(M_PI) / 180.0F;
	Vec3  direction{-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
	_delta = direction.scale(0.7);
	// The divergence: a small random spread
	_delta = _delta + Vec3{(level.random().nextFloat() - 0.5F) * 0.1, (level.random().nextFloat() - 0.5F) * 0.1, (level.random().nextFloat() - 0.5F) * 0.1};
}

void ThrownExperienceBottle::tick() {
	Entity::tick(); // baseTick: portals, below the world, burning
	if (isRemoved()) return;
	// ThrowableItemProjectile: 600 ticks at most
	if (_age++ >= 600) {
		discard();
		return;
	}
	// Gravity, move, break on a block, then friction (0.99, 0.6 in water)
	applyGravity(0.03);
	move(_delta);
	if (_horizontalCollision || _verticalCollision) {
		breakBottle();
		return;
	}
	_delta = isInWater() && _waterHeight > 0.1F ? _delta.scale(0.6) : _delta.scale(0.99);
}

void ThrownExperienceBottle::breakBottle() {
	// ThrownExperienceBottle.dropXp: 3-11 XP as an orb, the glass and pickup sounds
	int value = 3 + _random.nextInt(5) + _random.nextInt(5);
	if (auto orb = ExperienceOrb::create(_level, _position, value)) _level.entities().add(std::move(orb));
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:block.glass.break", Level::SoundSource::Neutral, 0.5F,
					   0.9F + _random.nextFloat() * 0.2F);
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.experience_orb.pickup", Level::SoundSource::Neutral, 0.1F,
					   (_random.nextFloat() - _random.nextFloat()) * 0.35F + 0.9F);
	discard();
}