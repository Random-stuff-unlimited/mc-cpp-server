#include "world/entity/mobs/Bat.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION			 = 1;
	constexpr int LEVEL_EVENT_BAT_LIFTOFF = 1025;

	double sign(double v) { return v > 0.0 ? 1.0 : v < 0.0 ? -1.0 : 0.0; }
} // namespace

Bat::Bat(Level& level, int typeId) : Mob(level, typeId) { _flags = 1; } // setResting(true)

void Bat::setResting(bool resting) {
	uint8_t flags = static_cast<uint8_t>(resting ? _flags | 1 : _flags & ~1);
	if (flags == _flags) return;
	_flags = flags;
	markData(DATA_FLAGS);
}

void Bat::tick() {
	Mob::tick();
	if (isRemoved()) return;
	if (isResting()) {
		// Hanging: still, its head against the block above
		_delta		= {0.0, 0.0, 0.0};
		_position.y = Mth::floor(_position.y) + 1.0 - _height;
	} else {
		_delta = _delta.multiply(1.0, 0.6, 1.0);
	}
}

void Bat::customServerAiStep() {
	Mob::customServerAiStep();
	BlockPos pos   = blockPosition();
	BlockPos above = pos.above();
	if (isResting()) {
		if (_level.isRedstoneConductor(_level.getBlockState(above))) {
			if (_random.nextInt(200) == 0) _yHeadRot = static_cast<float>(_random.nextInt(360));
			TargetingConditions conditions = TargetingConditions::forNonCombat();
			conditions.withRange(4.0);
			if (nearestPlayer(_level, conditions, this, _position.x, _position.y, _position.z)) {
				setResting(false);
				_level.levelEvent(nullptr, LEVEL_EVENT_BAT_LIFTOFF, pos, 0);
			}
		} else {
			setResting(false);
			_level.levelEvent(nullptr, LEVEL_EVENT_BAT_LIFTOFF, pos, 0);
		}
		return;
	}
	if (_hasTarget && (!_level.blocks().isAir(_level.getBlockState(_targetPosition)) || _targetPosition.y <= _level.minY())) _hasTarget = false;
	double tx = _targetPosition.x + 0.5 - _position.x, ty = _targetPosition.y + 0.5 - _position.y, tz = _targetPosition.z + 0.5 - _position.z;
	if (!_hasTarget || _random.nextInt(30) == 0 || tx * tx + ty * ty + tz * tz < 4.0) { // closerToCenterThan(position, 2)
		double x		= _position.x + _random.nextInt(7) - _random.nextInt(7);
		double y		= _position.y + _random.nextInt(6) - 2.0;
		double z		= _position.z + _random.nextInt(7) - _random.nextInt(7);
		_targetPosition = {Mth::floor(x), Mth::floor(y), Mth::floor(z)};
		_hasTarget		= true;
	}
	double dx = _targetPosition.x + 0.5 - _position.x;
	double dy = _targetPosition.y + 0.1 - _position.y;
	double dz = _targetPosition.z + 0.5 - _position.z;
	_delta	  = _delta + Vec3{(sign(dx) * 0.5 - _delta.x) * 0.1F, (sign(dy) * 0.7F - _delta.y) * 0.1F, (sign(dz) * 0.5 - _delta.z) * 0.1F};
	float yaw = static_cast<float>(Mth::atan2(_delta.z, _delta.x) * 180.0F / (float)M_PI) - 90.0F;
	_zza	  = 0.5F;
	_yRot += Mth::wrapDegrees(yaw - _yRot);
	if (_random.nextInt(100) == 0 && _level.isRedstoneConductor(_level.getBlockState(above))) setResting(true);
}

bool Bat::hurtServer(const Combat::DamageSource& source, float amount) {
	if (isResting()) setResting(false);
	return Mob::hurtServer(source, amount);
}

void Bat::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _flags == 0)) writeByteData(buf, DATA_FLAGS, _flags);
}

void Bat::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(_flags); // BatFlags
}

void Bat::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown bat data version");
	_flags	   = buf.readUByte();
	_dirtyData = 0;
}
