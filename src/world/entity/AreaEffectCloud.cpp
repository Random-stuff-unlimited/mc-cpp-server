#include "world/entity/AreaEffectCloud.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/LivingEntity.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;
	// The dragon's breath particles: the color of its purple (dragon_breath's own particle option isn't sent)
	constexpr int DRAGON_BREATH_COLOR = static_cast<int>(0xFFB33CDBu);
	constexpr int DEFAULT_COLOR		  = -1; // ColorParticleOption's default: white
} // namespace

AreaEffectCloud::AreaEffectCloud(Level& level) : AreaEffectCloud(level, Vec3{0, 0, 0}) {}

AreaEffectCloud::AreaEffectCloud(Level& level, const Vec3& position)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:area_effect_cloud"), 6.0F, 0.5F) {
	snapTo(position, 0.0F, 0.0F);
	_noGravity = true;
	setRadius(3.0F);
}

std::unique_ptr<AreaEffectCloud> AreaEffectCloud::lingering(Level& level, const Vec3& position, const std::string& potion, Actor* owner) {
	auto cloud			  = std::make_unique<AreaEffectCloud>(level, position);
	cloud->_owner		  = owner;
	cloud->_ownerId		  = owner ? owner->id() : -1;
	cloud->_radiusOnUse	  = -0.5F;
	cloud->_duration	  = 600;
	cloud->_waitTime	  = 10;
	cloud->_radiusPerTick = -cloud->_radius / static_cast<float>(cloud->_duration);
	cloud->_potion		  = potion;
	cloud->_durationScale = 0.25F; // The lingering potion item's potion_duration_scale
	return cloud;
}

std::unique_ptr<AreaEffectCloud> AreaEffectCloud::dragonBreath(Level& level, const Vec3& position, Actor* owner) {
	auto cloud			  = std::make_unique<AreaEffectCloud>(level, position);
	cloud->_owner		  = owner;
	cloud->_ownerId		  = owner ? owner->id() : -1;
	cloud->_duration	  = 600;
	cloud->_radiusPerTick = (7.0F - cloud->_radius) / static_cast<float>(cloud->_duration);
	cloud->_durationScale = 0.25F;
	cloud->_dragonBreath  = true;
	return cloud;
}

void AreaEffectCloud::setRadius(float radius) {
	_radius		   = std::clamp(radius, 0.0F, 32.0F);
	_width		   = _radius * 2.0F; // refreshDimensions: EntityDimensions.scalable(radius * 2, 0.5)
	entityDataDirty = true;
}

std::vector<MobEffects::Instance> AreaEffectCloud::effects() const {
	if (_dragonBreath) return {MobEffects::Instance(MobEffects::InstantDamage, 1, 1)};
	return _potion.empty() ? std::vector<MobEffects::Instance>{} : MobEffects::potionEffects(_potion);
}

AABB AreaEffectCloud::boundingBox() const {
	return {_position.x - _radius, _position.y, _position.z - _radius, _position.x + _radius, _position.y + 0.5, _position.z + _radius};
}

Actor* AreaEffectCloud::owner() {
	if (_ownerId < 0) return nullptr;
	Actor* actor = _level.actorById(_ownerId);
	return actor && actor == _owner && actor->isAlive() ? actor : nullptr;
}

// PotionContents.getColor: the effects' colors, weighted by level
int AreaEffectCloud::particleColor() const {
	if (_dragonBreath) return DRAGON_BREATH_COLOR;
	int r = 0, g = 0, b = 0, total = 0;
	for (const MobEffects::Instance& effect : effects()) {
		int c	= MobEffects::info(effect.effect).color;
		int amp = effect.amplifier + 1;
		r += amp * (c >> 16 & 0xFF);
		g += amp * (c >> 8 & 0xFF);
		b += amp * (c & 0xFF);
		total += amp;
	}
	if (total == 0) return DEFAULT_COLOR;
	return static_cast<int>(0xFF000000u | static_cast<uint32_t>(r / total) << 16 | static_cast<uint32_t>(g / total) << 8 | static_cast<uint32_t>(b / total));
}

bool AreaEffectCloud::affect(Actor& target, MobEffects::Host& host, MobEffects::Active& active, const std::vector<MobEffects::Instance>& list) {
	double dx = target.position().x - _position.x, dz = target.position().z - _position.z;
	if (dx * dx + dz * dz > static_cast<double>(_radius) * _radius) return true;
	_victims[target.id()] = _tickCount + _reapplicationDelay;
	Actor* by			  = owner();
	for (const MobEffects::Instance& effect : list) {
		if (MobEffects::info(effect.effect).instantaneous) {
			MobEffects::applyInstantaneous(host, effect, 0.5, by, this);
		} else {
			MobEffects::Instance scaled = effect;
			if (!scaled.isInfiniteDuration()) scaled.duration = std::max(1, static_cast<int>(scaled.duration * _durationScale));
			active.add(host, scaled, by != nullptr);
		}
	}
	if (_radiusOnUse != 0.0F) {
		setRadius(_radius + _radiusOnUse);
		if (_radius < 0.5F) {
			discard();
			return false;
		}
	}
	if (_durationOnUse != 0 && _duration != -1) {
		_duration += _durationOnUse;
		if (_duration <= 0) {
			discard();
			return false;
		}
	}
	return true;
}

// AreaEffectCloud.serverTick
void AreaEffectCloud::tick() {
	if (_duration != -1 && _tickCount >= _waitTime + _duration) {
		discard();
		return;
	}
	bool waiting = _tickCount < _waitTime;
	if (waiting != _waiting) {
		_waiting		= waiting;
		entityDataDirty = true;
	}
	if (waiting) return;
	if (_radiusPerTick != 0.0F) {
		setRadius(_radius + _radiusPerTick);
		if (_radius < 0.5F) {
			discard();
			return;
		}
	}
	if (_tickCount % 5 != 0) return;
	for (auto it = _victims.begin(); it != _victims.end();) it = _tickCount >= it->second ? _victims.erase(it) : std::next(it);
	std::vector<MobEffects::Instance> list = effects();
	if (list.empty()) {
		_victims.clear();
		return;
	}
	AABB area = boundingBox();
	for (const auto& player : _level.players()) {
		if (isRemoved()) return;
		// LivingEntity.isAffectedByPotions: not the dying, nor spectators
		if (player->isSpectator() || player->combat().dead || _victims.count(player->id()) || !player->boundingBox().intersects(area)) continue;
		if (!affect(*player, *player, player->effects(), list)) return;
	}
	std::vector<LivingEntity*> living;
	_level.entities().forEachIn(area, [&](Entity& entity) {
		if (LivingEntity* l = entity.asLiving(); l && !l->isRemoved() && l->isAlive() && !_victims.count(l->id())) living.push_back(l);
	});
	for (LivingEntity* target : living) {
		if (isRemoved() || !affect(*target, *target, target->effects(), list)) return;
	}
}

void AreaEffectCloud::writeEntityData(Buffer& buf) const {
	buf.writeUByte(DATA_RADIUS);
	buf.writeVarInt(LivingEntity::SERIALIZER_FLOAT);
	buf.writeFloat(_radius);
	if (!_waiting) {
		buf.writeUByte(DATA_WAITING);
		buf.writeVarInt(8); // BOOLEAN
		buf.writeBool(false);
	}
	// ColorParticleOption entity_effect in the effects' color
	buf.writeUByte(DATA_PARTICLE);
	buf.writeVarInt(LivingEntity::SERIALIZER_PARTICLES);
	buf.writeVarInt(_level.gameData().getStaticId("minecraft:particle_type", "minecraft:entity_effect"));
	buf.writeInt(particleColor());
}

void AreaEffectCloud::save(Buffer& buf) const {
	buf.writeUByte(SAVE_VERSION);
	buf.writeUUID(_uuid);
	buf.writeDouble(_position.x);
	buf.writeDouble(_position.y);
	buf.writeDouble(_position.z);
	buf.writeVarInt(_tickCount);
	buf.writeFloat(_radius);
	buf.writeFloat(_radiusOnUse);
	buf.writeFloat(_radiusPerTick);
	buf.writeVarInt(_duration);
	buf.writeVarInt(_durationOnUse);
	buf.writeVarInt(_waitTime);
	buf.writeVarInt(_reapplicationDelay);
	buf.writeFloat(_durationScale);
	buf.writeString(_potion);
	buf.writeBool(_dragonBreath);
}

void AreaEffectCloud::load(Buffer& buf) {
	buf.readUByte();
	_uuid	 = buf.readUUID();
	double x = buf.readDouble(), y = buf.readDouble(), z = buf.readDouble();
	snapTo({x, y, z}, 0.0F, 0.0F);
	_tickCount			= buf.readVarInt();
	float radius		= buf.readFloat();
	_radiusOnUse		= buf.readFloat();
	_radiusPerTick		= buf.readFloat();
	_duration			= buf.readVarInt();
	_durationOnUse		= buf.readVarInt();
	_waitTime			= buf.readVarInt();
	_reapplicationDelay = buf.readVarInt();
	_durationScale		= buf.readFloat();
	_potion				= buf.readString(256);
	_dragonBreath		= buf.readBool();
	setRadius(radius);
	_waiting = _tickCount < _waitTime;
}
