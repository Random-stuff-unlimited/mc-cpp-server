#include "world/entity/PrimedTnt.hpp"

#include "network/buffer.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/Portals.hpp"

#include <cmath>

namespace {
	constexpr int DATA_FUSE = 8, DATA_BLOCK_STATE = 9;		   // PrimedTnt.DATA_FUSE_ID, DATA_BLOCK_STATE_ID
	constexpr int SERIALIZER_INT = 1, SERIALIZER_BLOCK_STATE = 14; // EntityDataSerializers

	// PrimedTnt's USED_PORTAL_DAMAGE_CALCULATOR: a TNT that went through a portal doesn't break portals
	class UsedPortalCalculator : public Explosions::Calculator {
	  public:
		std::optional<float> blockExplosionResistance(Level& level, const BlockPos& pos, int state, const FluidState& fluid) const override {
			if (isPortal(level, state)) return std::nullopt;
			return Calculator::blockExplosionResistance(level, pos, state, fluid);
		}
		bool shouldBlockExplode(Level& level, const BlockPos&, int state, float) const override { return !isPortal(level, state); }

	  private:
		static bool isPortal(Level& level, int state) {
			return level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(state)) == "minecraft:nether_portal";
		}
	};
	const UsedPortalCalculator USED_PORTAL_CALCULATOR;
} // namespace

PrimedTnt::PrimedTnt(Level& level, const Vec3& position, Actor* owner)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:tnt"), 0.98F, 0.98F) {
	_position	 = position;
	_oldPosition = position;
	double angle = level.random().nextDouble() * (float)(M_PI * 2);
	_delta		 = {-std::sin(angle) * 0.02, 0.2F, -std::cos(angle) * 0.02};
	if (owner) _owner = EntityRef(*owner);
}

void PrimedTnt::setFuse(int fuse) {
	_fuse			= fuse;
	entityDataDirty = true;
}

Actor* PrimedTnt::owner() const { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

void PrimedTnt::tick() {
	Portals::handlePortal(_level, *this); // Entity.handlePortal
	if (isRemoved()) return;
	applyGravity(0.04);
	move(_delta);
	_level.checkInsideBlocks(boundingBox(), this); // applyEffectsFromBlocks
	_delta = _delta.scale(0.98);
	if (_onGround) _delta = _delta.multiply(0.7, -0.5, 0.7);
	int fuse = _fuse - 1;
	setFuse(fuse);
	if (fuse <= 0) {
		discard();
		explode();
	} else {
		updateInWaterStateAndDoFluidPushing();
	}
}

void PrimedTnt::explode() {
	// getY(0.0625): a sixteenth of its height up
	Explosions::explode(_level, this, std::nullopt, _usedPortal ? &USED_PORTAL_CALCULATOR : nullptr, {_position.x, _position.y + _height * 0.0625, _position.z},
						_explosionPower, false, Explosions::Interaction::Tnt);
}

void PrimedTnt::writeEntityData(Buffer& buf) const {
	buf.writeUByte(DATA_FUSE);
	buf.writeVarInt(SERIALIZER_INT);
	buf.writeVarInt(_fuse);
	(void)DATA_BLOCK_STATE;
	(void)SERIALIZER_BLOCK_STATE;
}

void PrimedTnt::save(Buffer& buf) const {
	buf.writeUUID(_uuid);
	for (double v : {_position.x, _position.y, _position.z, _delta.x, _delta.y, _delta.z}) buf.writeDouble(v);
	buf.writeShort(static_cast<int16_t>(_fuse));
	buf.writeFloat(_explosionPower);
	buf.writeBool(_owner.isSet());
	if (_owner.isSet()) buf.writeUUID(_owner.uuid);
	buf.writeBool(_usedPortal);
}

void PrimedTnt::load(Buffer& buf) {
	_uuid		   = buf.readUUID();
	_position.x	   = buf.readDouble();
	_position.y	   = buf.readDouble();
	_position.z	   = buf.readDouble();
	_delta.x	   = buf.readDouble();
	_delta.y	   = buf.readDouble();
	_delta.z	   = buf.readDouble();
	_fuse		   = buf.readShort();
	_explosionPower = std::clamp(buf.readFloat(), 0.0F, 128.0F);
	if (buf.readBool()) {
		_owner.uuid = buf.readUUID();
		_owner.id	= -2; // Found again by its UUID
	}
	_usedPortal	 = buf.readBool();
	_oldPosition = _position;
}
