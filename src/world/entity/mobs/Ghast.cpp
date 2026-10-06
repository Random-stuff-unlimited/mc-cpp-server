#include "world/entity/mobs/Ghast.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Fireballs.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int LEVEL_EVENT_GHAST_WARNING = 1015, LEVEL_EVENT_GHAST_FIREBALL = 1016;

	// Ghast.GhastMoveControl: every few ticks, a push toward the wanted position if nothing is in the way
	class GhastMoveControl : public MoveControl {
	  public:
		explicit GhastMoveControl(Mob& ghast) : MoveControl(ghast) {}
		void tick() override {
			if (_operation != Operation::MoveTo) return;
			if (_floatDuration-- > 0) return;
			_floatDuration += _mob.random().nextInt(5) + 2;
			Vec3 to{_wantedX - _mob.position().x, _wantedY - _mob.position().y, _wantedZ - _mob.position().z};
			if (canReach(to)) {
				int flyingSpeed = _mob.attributeIds().flyingSpeed;
				_mob.setDeltaMovement(_mob.deltaMovement() + to.normalize().scale(_mob.getAttributeValue(flyingSpeed) * 5.0 / 3.0));
			} else {
				_operation = Operation::Wait;
			}
		}

	  private:
		int _floatDuration = 0;

		// Its box swept along the way meets no block's collision (the blocks it is already in don't count)
		bool canReach(const Vec3& to) {
			Level&			level = _mob.level();
			const GameData& data  = level.gameData();
			AABB			box	  = _mob.boundingBox();
			double			length = to.length();
			int				steps  = std::max(1, Mth::ceil(length * 2.0));
			for (int i = 1; i <= steps; i++) {
				AABB moved = box.move(to.scale(static_cast<double>(i) / steps));
				for (int x = Mth::floor(moved.minX); x <= Mth::floor(moved.maxX); x++) {
					for (int y = Mth::floor(moved.minY); y <= Mth::floor(moved.maxY); y++) {
						for (int z = Mth::floor(moved.minZ); z <= Mth::floor(moved.maxZ); z++) {
							AABB cell{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), x + 1.0, y + 1.0, z + 1.0};
							if (box.intersects(cell)) continue;
							int state = level.getBlockState({x, y, z});
							if (level.blocks().isAir(state)) continue;
							for (const GameData::Box& b : data.getCollisionShape(state)) {
								if (moved.intersects({x + b.minX, y + b.minY, z + b.minZ, x + b.maxX, y + b.maxY, z + b.maxZ})) return false;
							}
						}
					}
				}
			}
			return true;
		}
	};

	// Ghast.RandomFloatAroundGoal: a new spot within 16 blocks when it has none, reached it or is too far from it
	class RandomFloatAroundGoal : public Goal {
	  public:
		explicit RandomFloatAroundGoal(Mob& ghast) : _ghast(ghast) { setFlags({Flag::Move}); }
		bool canUse() override {
			MoveControl& control = _ghast.moveControl();
			if (!control.hasWanted()) return true;
			double dx = control.wantedX() - _ghast.position().x, dy = control.wantedY() - _ghast.position().y, dz = control.wantedZ() - _ghast.position().z;
			double distance = dx * dx + dy * dy + dz * dz;
			return distance < 1.0 || distance > 3600.0;
		}
		bool canContinueToUse() override { return false; }
		void start() override {
			// getSuitableFlyToPosition (distanceToBlocks 0: the first try is good)
			JavaRandom& random = _ghast.random();
			Level&		level  = _ghast.level();
			const Vec3& p	   = _ghast.position();
			Vec3		pos;
			bool		found = false;
			for (int i = 0; i < 64 && !found; i++) {
				pos	  = {p.x + (random.nextFloat() * 2.0F - 1.0F) * 16.0F, p.y + (random.nextFloat() * 2.0F - 1.0F) * 16.0F,
						 p.z + (random.nextFloat() * 2.0F - 1.0F) * 16.0F};
				found = !_ghast.hasHome() || _ghast.isWithinHome({Mth::floor(pos.x), Mth::floor(pos.y), Mth::floor(pos.z)});
			}
			if (!found) {
				pos = {p.x + (random.nextFloat() * 2.0F - 1.0F) * 16.0F, p.y + (random.nextFloat() * 2.0F - 1.0F) * 16.0F,
					   p.z + (random.nextFloat() * 2.0F - 1.0F) * 16.0F};
			}
			// Above the open sky's ground: no higher than it is now
			int height = level.getHeight(Level::Heightmap::MotionBlocking, Mth::floor(pos.x), Mth::floor(pos.z));
			if (height < Mth::floor(pos.y) && height > level.minY()) pos.y = p.y - std::abs(p.y - pos.y);
			_ghast.moveControl().setWantedPosition(pos.x, pos.y, pos.z, 1.0);
		}

	  private:
		Mob& _ghast;
	};

	class GhastLookGoal : public Goal {
	  public:
		explicit GhastLookGoal(Mob& ghast) : _ghast(ghast) { setFlags({Flag::Look}); }
		bool canUse() override { return true; }
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override { Ghast::faceMovementDirection(_ghast); }

	  private:
		Mob& _ghast;
	};

	// Ghast.GhastShootFireballGoal: charges for a second while it sees its target, shoots, rests 2 seconds
	class GhastShootFireballGoal : public Goal {
	  public:
		explicit GhastShootFireballGoal(Ghast& ghast) : _ghast(ghast) {}
		bool canUse() override { return _ghast.getTarget() != nullptr; }
		void start() override { _chargeTime = 0; }
		void stop() override { _ghast.setCharging(false); }
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _ghast.getTarget();
			if (!target) return;
			if (target->distanceToSqr(_ghast) < 4096.0 && _ghast.hasLineOfSight(*target)) {
				Level& level = _ghast.level();
				_chargeTime++;
				if (_chargeTime == 10) level.levelEvent(nullptr, LEVEL_EVENT_GHAST_WARNING, _ghast.blockPosition(), 0);
				if (_chargeTime == 20) {
					Vec3   view	  = _ghast.viewVector();
					AABB   box	  = target->boundingBox();
					double middle = _ghast.position().y + _ghast.height() * 0.5;
					double dx	  = target->position().x - (_ghast.position().x + view.x * 4.0);
					double dy	  = target->position().y + (box.maxY - box.minY) * 0.5 - (0.5 + middle);
					double dz	  = target->position().z - (_ghast.position().z + view.z * 4.0);
					level.levelEvent(nullptr, LEVEL_EVENT_GHAST_FIREBALL, _ghast.blockPosition(), 0);
					auto ball = std::make_unique<LargeFireball>(level, &_ghast, Vec3{dx, dy, dz}.normalize(), _ghast.explosionPower());
					ball->setPos({_ghast.position().x + view.x * 4.0, middle + 0.5, ball->position().z + view.z * 4.0});
					level.entities().add(std::move(ball));
					_chargeTime = -40;
				}
			} else if (_chargeTime > 0) {
				_chargeTime--;
			}
			_ghast.setCharging(_chargeTime > 10);
		}

	  private:
		Ghast& _ghast;
		int	   _chargeTime = 0;
	};

	// A ghast's own fireball, sent back by a player
	bool isReflectedFireball(const Combat::DamageSource& source) {
		return dynamic_cast<LargeFireball*>(source.direct) != nullptr && source.causing && source.causing->asPlayer();
	}
} // namespace

Ghast::Ghast(Level& level, int typeId) : Mob(level, typeId) {
	_xpReward = 5;
	setMoveControl(std::make_unique<GhastMoveControl>(*this));
}

void Ghast::registerGoals() {
	_goalSelector.addGoal(5, std::make_unique<RandomFloatAroundGoal>(*this));
	_goalSelector.addGoal(7, std::make_unique<GhastLookGoal>(*this));
	_goalSelector.addGoal(7, std::make_unique<GhastShootFireballGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false, [this](Actor& target, Level&) {
		return std::abs(target.position().y - _position.y) <= 4.0;
	}));
}

void Ghast::setCharging(bool charging) {
	if (charging == _charging) return;
	_charging = charging;
	markData(DATA_IS_CHARGING);
}

Vec3 Ghast::viewVector() const {
	const float toRadians = static_cast<float>(M_PI / 180.0);
	float		pitch = _xRot * toRadians, yaw = -_yRot * toRadians;
	float		cy = Mth::cos(yaw), sy = Mth::sin(yaw), cp = Mth::cos(pitch), sp = Mth::sin(pitch);
	return {sy * cp, -sp, cy * cp}; // Entity.calculateViewVector
}

void Ghast::faceMovementDirection(Mob& mob) {
	Actor* target = mob.getTarget();
	if (!target) {
		const Vec3& delta = mob.deltaMovement();
		mob.setYRotPublic(-static_cast<float>(Mth::atan2(delta.x, delta.z)) * (180.0F / (float)M_PI));
		mob.setYBodyRot(mob.yRot());
	} else if (target->distanceToSqr(mob) < 4096.0) {
		double dx = target->position().x - mob.position().x, dz = target->position().z - mob.position().z;
		mob.setYRotPublic(-static_cast<float>(Mth::atan2(dx, dz)) * (180.0F / (float)M_PI));
		mob.setYBodyRot(mob.yRot());
	}
}

bool Ghast::hurtServer(const Combat::DamageSource& source, float amount) {
	if (isReflectedFireball(source)) {
		Mob::hurtServer(source, 1000.0F);
		return true;
	}
	return Mob::hurtServer(source, amount);
}

void Ghast::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_IS_CHARGING, onlyNonDefault, !_charging)) writeBoolData(buf, DATA_IS_CHARGING, _charging);
}

void Ghast::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(static_cast<uint8_t>(_explosionPower));
}

void Ghast::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown ghast data version");
	_explosionPower = buf.readUByte();
	_dirtyData		= 0;
}
