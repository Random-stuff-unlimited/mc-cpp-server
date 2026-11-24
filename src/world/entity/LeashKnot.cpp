#include "world/entity/LeashKnot.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"

#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION = 1;

	bool isFence(Level& level, const BlockPos& pos) {
		return level.gameData().isInTag("minecraft:block", "minecraft:fences", level.blocks().blockOf(level.getBlockState(pos)));
	}
} // namespace

LeashKnot::LeashKnot(Level& level, const BlockPos& fence)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:leash_knot"), 0.375F, 0.5F) {
	snapTo({fence.x + 0.5, fence.y + 0.375, fence.z + 0.5}, 0.0F, 0.0F);
	_noGravity = true;
}

BlockPos LeashKnot::fence() const { return {Mth::floor(_position.x), Mth::floor(_position.y), Mth::floor(_position.z)}; }

int LeashKnot::bindPlayerMobs(Level& level, Player& player, const BlockPos& fence) {
	if (!isFence(level, fence)) return 0;
	Vec3			center{fence.x + 0.5, fence.y + 0.5, fence.z + 0.5};
	AABB			area{center.x - 7.0, center.y - 7.0, center.z - 7.0, center.x + 7.0, center.y + 7.0, center.z + 7.0};
	std::vector<Mob*> mobs;
	for (Actor* actor : level.actorsIn(area)) {
		auto* mob = dynamic_cast<Mob*>(actor);
		if (mob && mob->leashHolder() == &player) mobs.push_back(mob);
	}
	if (mobs.empty()) return 0;
	// The fence's knot, or a new one
	LeashKnot* knot = nullptr;
	for (Actor* actor : level.actorsIn(AABB{static_cast<double>(fence.x), static_cast<double>(fence.y), static_cast<double>(fence.z), fence.x + 1.0,
											fence.y + 1.0, fence.z + 1.0})) {
		if (auto* existing = dynamic_cast<LeashKnot*>(actor)) knot = existing;
	}
	if (!knot) {
		auto made = std::make_unique<LeashKnot>(level, fence);
		knot	  = made.get();
		level.entities().add(std::move(made));
		level.playSoundAt(nullptr, center.x, center.y, center.z, "minecraft:item.lead.tied", Level::SoundSource::Blocks, 1.0F, 1.0F);
	}
	for (Mob* mob : mobs) mob->setLeashedTo(*knot);
	return static_cast<int>(mobs.size());
}

void LeashKnot::tick() {
	// LeashFenceKnotEntity.survives: on its fence
	if (_tickCount % 20 == 0 && !isFence(_level, fence())) discard();
}

bool LeashKnot::interact(Player& player, int hand) {
	if (hand != 0) return false;
	// The player's leashed mobs move to it; else it comes off, the mobs it held free
	if (bindPlayerMobs(_level, player, fence()) > 0) return true;
	discard();
	return true;
}

bool LeashKnot::hurtServer(const Combat::DamageSource&, float) {
	if (!isRemoved()) discard();
	return true;
}

void LeashKnot::save(Buffer& buf) const {
	buf.writeUByte(SAVE_VERSION);
	buf.writeUUID(_uuid);
	buf.writeDouble(_position.x);
	buf.writeDouble(_position.y);
	buf.writeDouble(_position.z);
}

void LeashKnot::load(Buffer& buf) {
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown leash knot data version");
	_uuid		 = buf.readUUID();
	_position.x	 = buf.readDouble();
	_position.y	 = buf.readDouble();
	_position.z	 = buf.readDouble();
	_oldPosition = _position;
}
