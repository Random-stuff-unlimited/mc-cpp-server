#include "world/entity/ai/Controls.hpp"

#include "world/entity/Mob.hpp"

#include <cmath>

// ----- MoveControl -----

void MoveControl::setWantedPosition(double x, double y, double z, double speedModifier) {
	_wantedX	   = x;
	_wantedY	   = y;
	_wantedZ	   = z;
	_speedModifier = speedModifier;
	if (_operation != Operation::Jumping) _operation = Operation::MoveTo;
}

void MoveControl::strafe(float forwards, float right) {
	_operation		= Operation::Strafe;
	_strafeForwards = forwards;
	_strafeRight	= right;
	_speedModifier	= 0.25;
}

void MoveControl::tick() {
	if (_operation == Operation::Wait) {
		_mob.setZza(0.0F);
		return;
	}
	// MOVE_TO, STRAFE, JUMPING: not ported yet (no goal asks for them). Back to waiting, like vanilla after each move
	_operation = Operation::Wait;
	_mob.setZza(0.0F);
}

// ----- LookControl -----

void LookControl::setLookAt(double x, double y, double z) { setLookAt(x, y, z, static_cast<float>(_mob.headRotSpeed()), static_cast<float>(_mob.maxHeadXRot())); }

void LookControl::setLookAt(double x, double y, double z, float yMaxRotSpeed, float xMaxRotAngle) {
	_wantedX		= x;
	_wantedY		= y;
	_wantedZ		= z;
	_yMaxRotSpeed	= yMaxRotSpeed;
	_xMaxRotAngle	= xMaxRotAngle;
	_lookAtCooldown = 2;
}

void LookControl::tick() {
	if (resetXRotOnTick()) _mob.setXRot(0.0F);
	if (_lookAtCooldown > 0) {
		_lookAtCooldown--;
		const Vec3& position = _mob.position();
		double		dx = _wantedX - position.x, dy = _wantedY - _mob.eyeY(), dz = _wantedZ - position.z;
		// getYRotD / getXRotD: nothing to turn to when the target is right there
		if (std::abs(dz) > 1.0E-5F || std::abs(dx) > 1.0E-5F) {
			float yRot = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
			_mob.setYHeadRot(rotateTowards(_mob.yHeadRot(), yRot, _yMaxRotSpeed));
		}
		double horizontal = std::sqrt(dx * dx + dz * dz);
		if (std::abs(dy) > 1.0E-5F || std::abs(horizontal) > 1.0E-5F) {
			float xRot = static_cast<float>(-(Mth::atan2(dy, horizontal) * 180.0F / (float)M_PI));
			_mob.setXRot(rotateTowards(_mob.xRot(), xRot, _xMaxRotAngle));
		}
	} else {
		_mob.setYHeadRot(rotateTowards(_mob.yHeadRot(), _mob.yBodyRot(), 10.0F));
	}
	clampHeadRotationToBody();
}

void LookControl::clampHeadRotationToBody() {
	if (!_mob.navigation().isDone()) _mob.setYHeadRot(Mth::rotateIfNecessary(_mob.yHeadRot(), _mob.yBodyRot(), static_cast<float>(_mob.maxHeadYRot())));
}

// ----- JumpControl -----

void JumpControl::tick() {
	_mob.setJumping(_jump);
	_jump = false;
}

// ----- BodyRotationControl -----

void BodyRotationControl::clientTick() {
	const Vec3& position = _mob.position();
	const Vec3& old		 = _mob.oldPosition();
	double		dx = position.x - old.x, dz = position.z - old.z;
	float		maxHeadYRot = static_cast<float>(_mob.maxHeadYRot());
	if (dx * dx + dz * dz > 2.5000003E-7F) {
		// isMoving: the body faces where it goes, the head stays within reach of it
		_mob.setYBodyRot(_mob.yRot());
		_mob.setYHeadRot(Mth::rotateIfNecessary(_mob.yHeadRot(), _mob.yBodyRot(), maxHeadYRot));
		_lastStableYHeadRot = _mob.yHeadRot();
		_headStableTime		= 0;
		return;
	}
	// No passengers are ported: never carrying a mob
	if (std::abs(_mob.yHeadRot() - _lastStableYHeadRot) > 15.0F) {
		_headStableTime		= 0;
		_lastStableYHeadRot = _mob.yHeadRot();
		_mob.setYBodyRot(Mth::rotateIfNecessary(_mob.yBodyRot(), _mob.yHeadRot(), maxHeadYRot)); // rotateBodyIfNecessary
	} else {
		_headStableTime++;
		if (_headStableTime > 10) {
			// rotateHeadTowardsFront: the body slowly turns to where the head looks
			float progress = Mth::clamp((_headStableTime - 10) / 10.0F, 0.0F, 1.0F);
			_mob.setYBodyRot(Mth::rotateIfNecessary(_mob.yBodyRot(), _mob.yHeadRot(), maxHeadYRot * (1.0F - progress)));
		}
	}
}

// ----- PathNavigation -----

bool PathNavigation::moveTo(double, double, double, double) { return false; }
