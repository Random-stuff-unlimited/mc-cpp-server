#include "world/entity/ai/Controls.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"

#include <algorithm>
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
	if (_operation == Operation::Strafe) {
		float speed	   = static_cast<float>(_mob.getAttributeValue(_mob.attributeIds().movementSpeed));
		float scaled   = static_cast<float>(_speedModifier) * speed;
		float forwards = _strafeForwards, right = _strafeRight;
		float length   = std::sqrt(forwards * forwards + right * right);
		if (length < 1.0F) length = 1.0F;
		length = scaled / length;
		forwards *= length;
		right *= length;
		float sin = Mth::sin(_mob.yRot() * (float)(M_PI / 180.0)), cos = Mth::cos(_mob.yRot() * (float)(M_PI / 180.0));
		float dx = forwards * cos - right * sin, dz = right * cos + forwards * sin;
		if (!isWalkable(dx, dz)) {
			_strafeForwards = 1.0F;
			_strafeRight	= 0.0F;
		}
		_mob.setSpeed(scaled);
		_mob.setZza(_strafeForwards);
		_mob.setXxa(_strafeRight);
		_operation = Operation::Wait;
	} else if (_operation == Operation::MoveTo) {
		_operation = Operation::Wait;
		const Vec3& position = _mob.position();
		double		dx = _wantedX - position.x, dz = _wantedZ - position.z, dy = _wantedY - position.y;
		if (dx * dx + dy * dy + dz * dz < 2.5000003E-7F) {
			_mob.setZza(0.0F);
			return;
		}
		float yRot = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
		_mob.setYRot(rotlerp(_mob.yRot(), yRot, 90.0F));
		_mob.setSpeed(static_cast<float>(_speedModifier * _mob.getAttributeValue(_mob.attributeIds().movementSpeed)));
		Level&	 level = _mob.level();
		BlockPos pos   = _mob.blockPosition();
		int		 state = level.getBlockState(pos);
		const auto& shape = level.gameData().getCollisionShape(state);
		double	 top	= 0.0;
		for (const auto& box : shape) top = std::max(top, box.maxY);
		static std::vector<bool> doors, fences;
		static const GameData*	 cached = nullptr;
		if (cached != &level.gameData()) {
			cached = &level.gameData();
			doors  = cached->blockTag("minecraft:doors");
			fences = cached->blockTag("minecraft:fences");
		}
		int block = level.blocks().blockOf(state);
		if ((dy > _mob.maxUpStep() && dx * dx + dz * dz < std::max(1.0F, _mob.width())) ||
			(!shape.empty() && position.y < top + pos.y && !doors[block] && !fences[block])) {
			_mob.jumpControl().jump();
			_operation = Operation::Jumping;
		}
	} else if (_operation == Operation::Jumping) {
		_mob.setSpeed(static_cast<float>(_speedModifier * _mob.getAttributeValue(_mob.attributeIds().movementSpeed)));
		if (_mob.onGround() || (_mob.isInLiquid() && _mob.isAffectedByFluids())) _operation = Operation::Wait;
	} else {
		_mob.setZza(0.0F);
	}
}

bool MoveControl::isWalkable(float x, float z) {
	NodeEvaluator& evaluator = _mob.navigation().getNodeEvaluator();
	BlockPos	   pos{Mth::floor(_mob.position().x + x), _mob.blockPosition().y, Mth::floor(_mob.position().z + z)};
	return evaluator.getPathType(_mob, pos) == PathType::Walkable;
}

float MoveControl::rotlerp(float from, float to, float max) {
	float delta = Mth::wrapDegrees(to - from);
	delta		= std::clamp(delta, -max, max);
	float result = from + delta;
	if (result < 0.0F) {
		result += 360.0F;
	} else if (result > 360.0F) {
		result -= 360.0F;
	}
	return result;
}

// ----- Sensing -----

bool Sensing::hasLineOfSight(Actor& target) {
	int id = target.id();
	if (_seen.count(id)) return true;
	if (_unseen.count(id)) return false;
	bool visible = _mob.hasLineOfSight(target);
	(visible ? _seen : _unseen).insert(id);
	return visible;
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

namespace {
	double wantedY(Actor& target) {
		if (target.asLiving() || target.isPlayer()) return target.eyeY();
		AABB box = target.boundingBox();
		return (box.minY + box.maxY) / 2.0;
	}
} // namespace

void LookControl::setLookAt(Actor& target) { setLookAt(target.position().x, wantedY(target), target.position().z); }

void LookControl::setLookAt(Actor& target, float yMaxRotSpeed, float xMaxRotAngle) {
	setLookAt(target.position().x, wantedY(target), target.position().z, yMaxRotSpeed, xMaxRotAngle);
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


