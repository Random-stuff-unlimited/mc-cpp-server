#include "world/entity/Mob.hpp"

#include "network/buffer.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/MobRegistry.hpp"

#include <limits>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int SAVED_NO_AI = 1, SAVED_LEFT_HANDED = 2, SAVED_PERSISTENT = 4;
} // namespace

Mob::Mob(Level& level, int typeId)
	: LivingEntity(level, typeId), _moveControl(std::make_unique<MoveControl>(*this)), _lookControl(std::make_unique<LookControl>(*this)),
	  _jumpControl(std::make_unique<JumpControl>(*this)), _bodyRotationControl(*this), _navigation(std::make_unique<PathNavigation>(*this)) {}

void Mob::setMobFlag(int flag, bool set) {
	uint8_t flags = static_cast<uint8_t>(set ? (_mobFlags | flag) : (_mobFlags & ~flag));
	if (flags == _mobFlags) return;
	_mobFlags = flags;
	markData(DATA_MOB_FLAGS);
}

void Mob::finalizeSpawn() {
	JavaRandom&		   random = _level.random();
	AttributeInstance* follow = _attributes.getInstance(_ids.followRange);
	if (follow && !follow->hasModifier("minecraft:random_spawn_bonus")) {
		double bonus = 0.0 + 0.11485000000000001 * (random.nextDouble() - random.nextDouble()); // RandomSource.triangle
		follow->addModifier({"minecraft:random_spawn_bonus", bonus, AttributeModifier::Operation::AddMultipliedBase}, true);
	}
	setLeftHanded(random.nextFloat() < 0.05F);
}

void Mob::playAmbientSound() {
	std::string sound = typeSound("ambient", "");
	if (!sound.empty()) playSound(sound, 1.0F, voicePitch());
}

void Mob::livingBaseTick() {
	LivingEntity::livingBaseTick();
	if (isAlive() && _random.nextInt(1000) < _ambientSoundTime++) {
		_ambientSoundTime = -ambientSoundInterval();
		playAmbientSound();
	}
}

void Mob::playHurtSound() {
	_ambientSoundTime = -ambientSoundInterval();
	LivingEntity::playHurtSound();
}

void Mob::tick() {
	LivingEntity::tick();
	if (_tickCount % 5 == 0) {
		// updateControlFlags: nothing rides or is ridden, every control stays available
		_goalSelector.setControlFlag(Goal::Flag::Move, true);
		_goalSelector.setControlFlag(Goal::Flag::Jump, true);
		_goalSelector.setControlFlag(Goal::Flag::Look, true);
	}
}

// Mob.serverAiStep: where the AI runs, in vanilla's order
void Mob::serverAiStep() {
	_noActionTime++;
	_sensing.tick();
	if ((_tickCount + _id) % 2 != 0 && _tickCount > 1) {
		_targetSelector.tickRunningGoals(false);
		_goalSelector.tickRunningGoals(false);
	} else {
		_targetSelector.tick();
		_goalSelector.tick();
	}
	_navigation->tick();
	customServerAiStep();
	_moveControl->tick();
	_lookControl->tick();
	_jumpControl->tick();
}

bool Mob::removeWhenFarAway(double) const {
	// The overrides of removeWhenFarAway that don't depend on state not ported yet (taming, buckets, raids)
	for (const char* keeps : {"Animal", "AbstractGolem", "Villager", "WanderingTrader", "Warden", "Allay"}) {
		if (_type.is(keeps)) return false;
	}
	return true;
}

// Mob.checkDespawn: monsters in peaceful go, the others far from every player
void Mob::checkDespawn() {
	if (_level.server().getConfig().getDifficulty() == "peaceful" && !_type.allowedInPeaceful) {
		discard();
		return;
	}
	if (_persistenceRequired) {
		_noActionTime = 0;
		return;
	}
	double nearest = std::numeric_limits<double>::max();
	bool   found   = false;
	for (const auto& player : _level.server().getGamePlayers()) {
		if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator) continue; // EntitySelector.NO_SPECTATORS
		double dx = player->getX() - _position.x, dy = player->getY() - _position.y, dz = player->getZ() - _position.z;
		nearest	  = std::min(nearest, dx * dx + dy * dy + dz * dz);
		found	  = true;
	}
	if (!found) return;
	// MobCategory.getDespawnDistance / getNoDespawnDistance: 128 (64 for water ambient) and 32
	int despawn = _type.category == GameData::MobCategory::WaterAmbient ? 64 : 128;
	if (nearest > despawn * despawn && removeWhenFarAway(nearest)) {
		discard();
		return;
	}
	if (_noActionTime > 600 && _random.nextInt(800) == 0 && nearest > 32 * 32 && removeWhenFarAway(nearest)) {
		discard();
	} else if (nearest < 32 * 32) {
		_noActionTime = 0;
	}
}

void Mob::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	LivingEntity::writeData(buf, mask, onlyNonDefault);
	if ((mask & (1u << DATA_MOB_FLAGS)) && !(onlyNonDefault && _mobFlags == 0)) {
		buf.writeUByte(DATA_MOB_FLAGS);
		buf.writeVarInt(SERIALIZER_BYTE);
		buf.writeUByte(_mobFlags);
	}
}

void Mob::save(Buffer& buf) const {
	LivingEntity::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(static_cast<uint8_t>((isNoAi() ? SAVED_NO_AI : 0) | (isLeftHanded() ? SAVED_LEFT_HANDED : 0) |
										(_persistenceRequired ? SAVED_PERSISTENT : 0)));
}

void Mob::load(Buffer& buf) {
	LivingEntity::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown mob data version");
	uint8_t flags		 = buf.readUByte();
	_mobFlags			 = static_cast<uint8_t>((flags & SAVED_NO_AI ? FLAG_NO_AI : 0) | (flags & SAVED_LEFT_HANDED ? FLAG_LEFT_HANDED : 0));
	_persistenceRequired = flags & SAVED_PERSISTENT;
	_dirtyData			 = 0;
}
