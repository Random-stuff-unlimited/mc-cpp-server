#include "world/entity/EndCrystal.hpp"

#include "network/buffer.hpp"
#include "world/Combat.hpp"
#include "world/EndDragonFight.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/blocks/Fire.hpp"

#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION = 1, SERIALIZER_BOOLEAN = 8;
}

EndCrystal::EndCrystal(Level& level, const Vec3& position) : Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:end_crystal"), 2.0F, 2.0F) {
	_position	 = position;
	_oldPosition = position;
	_noGravity	 = true;
}

void EndCrystal::tick() {
	// EndCrystal.tick: in the End, a fire under it (where there is air)
	if (_level.dimensionType().effects == "minecraft:the_end") {
		BlockPos pos = blockPosition();
		if (_level.blocks().isAir(_level.getBlockState(pos))) _level.setBlock(pos, BaseFireBlock::getState(_level, pos), Level::UPDATE_ALL);
	}
}

bool EndCrystal::hurtServer(const Combat::DamageSource& source, float) {
	if (isRemoved()) return false;
	// Explosions of other crystals set it off too (vanilla ignores only the ender dragon's own)
	if (source.causing && _level.gameData().getStaticName("minecraft:entity_type", source.causing->typeId()) == "minecraft:ender_dragon") return false;
	discard();
	Explosions::explode(_level, this, std::nullopt, nullptr, _position, 6.0F, false, Explosions::Interaction::Block);
	if (EndDragonFight* fight = _level.endFight()) fight->onCrystalDestroyed(*this, source);
	return true;
}

void EndCrystal::writeEntityData(Buffer& buf) const {
	if (_showBottom) return; // Its default
	buf.writeUByte(DATA_SHOW_BOTTOM);
	buf.writeVarInt(SERIALIZER_BOOLEAN);
	buf.writeBool(false);
}

void EndCrystal::save(Buffer& buf) const {
	buf.writeUByte(SAVE_VERSION);
	buf.writeUUID(_uuid);
	buf.writeDouble(_position.x);
	buf.writeDouble(_position.y);
	buf.writeDouble(_position.z);
	buf.writeBool(_showBottom);
}

void EndCrystal::load(Buffer& buf) {
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown end crystal data version");
	_uuid		 = buf.readUUID();
	_position.x	 = buf.readDouble();
	_position.y	 = buf.readDouble();
	_position.z	 = buf.readDouble();
	_oldPosition = _position;
	_showBottom	 = buf.readBool();
}
