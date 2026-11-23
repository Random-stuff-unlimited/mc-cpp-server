#include "world/blockentity/BeehiveBlockEntity.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/mobs/FlyingAnimals.hpp"

#include <stdexcept>

bool BeehiveBlockEntity::addOccupant(Bee& bee) {
	if (isFull()) return false;
	Occupant occupant;
	occupant.hasNectar		= bee.hasNectar();
	occupant.minTicksInHive = occupant.hasNectar ? MIN_TICKS_NECTAR : MIN_TICKS_NO_NECTAR;
	occupant.typeId			= bee.typeId();
	bee.setHive(pos());
	Buffer saved;
	bee.save(saved);
	occupant.data = saved.getData();
	_occupants.push_back(std::move(occupant));
	if (Level* level = this->level()) level->playSound(nullptr, pos(), "minecraft:block.beehive.enter", Level::SoundSource::Blocks, 1.0F, 1.0F);
	bee.discard();
	markChanged();
	return true;
}

bool BeehiveBlockEntity::releaseOccupant(Level& level, Occupant& occupant, ReleaseStatus status, Player* angryAt) {
	// Out of its front, if it is free; not at night nor in the rain (but in an emergency)
	if (status != ReleaseStatus::Emergency && (level.isDarkOutside() || level.isRaining())) return false;
	int			state  = level.getBlockState(pos());
	std::string facing = level.gameData().getProperty(state, "facing");
	Direction	front  = facing == "north" ? Direction::North : facing == "south" ? Direction::South : facing == "west" ? Direction::West : Direction::East;
	BlockPos	exit   = pos().relative(front);
	if (status != ReleaseStatus::Emergency && !level.gameData().getCollisionShape(level.getBlockState(exit)).empty()) return false;
	std::unique_ptr<Mob> mob = level.mobs().create(level, occupant.typeId);
	auto*				 bee = dynamic_cast<Bee*>(mob.get());
	if (!bee) return true; // Not a bee anymore (data from elsewhere): dropped
	try {
		Buffer in(occupant.data);
		bee->load(in);
	} catch (const std::exception&) {
		return true;
	}
	// BeehiveBlockEntity.setBeeReleaseData: the time inside counts toward growing up, and heals
	if (bee->isBaby()) bee->ageUp(occupant.ticksInHive / 20);
	bee->heal(occupant.ticksInHive / 20.0F);
	if (status == ReleaseStatus::HoneyDelivered) {
		bee->setHasNectar(false);
		bee->cropsGrownSincePollination = 0;
		std::string honey = level.gameData().getProperty(state, "honey_level");
		if (!honey.empty() && std::stoi(honey) < 5) {
			int add	 = level.random().nextInt(100) == 0 ? 2 : 1;
			int next = std::min(5, std::stoi(honey) + add);
			int s	 = level.gameData().withProperty(state, "honey_level", std::to_string(next));
			if (s >= 0) level.setBlock(pos(), s, Level::UPDATE_ALL);
		}
	}
	bee->stayOutOfHiveCountdown = 400;
	double offset				= 0.55 + bee->width() / 2.0;
	bee->snapTo({pos().x + 0.5 + Directions::stepX(front) * offset, pos().y + 0.5 - bee->height() / 2.0, pos().z + 0.5 + Directions::stepZ(front) * offset},
				bee->yRot(), bee->xRot());
	Bee* added = static_cast<Bee*>(level.addFreshEntity(std::move(mob)));
	if (angryAt && added) added->setTarget(angryAt);
	level.playSound(nullptr, pos(), "minecraft:block.beehive.exit", Level::SoundSource::Blocks, 1.0F, 1.0F);
	return true;
}

void BeehiveBlockEntity::tick(Level& level) {
	bool changed = false;
	for (auto it = _occupants.begin(); it != _occupants.end();) {
		if (++it->ticksInHive > it->minTicksInHive &&
			releaseOccupant(level, *it, it->hasNectar ? ReleaseStatus::HoneyDelivered : ReleaseStatus::BeeReleased)) {
			it		= _occupants.erase(it);
			changed = true;
		} else {
			++it;
		}
	}
	if (changed) markChanged();
	// Now and then, the buzz of the bees inside
	if (!_occupants.empty() && level.random().nextDouble() < 0.005) {
		level.playSound(nullptr, pos(), "minecraft:block.beehive.work", Level::SoundSource::Blocks, 1.0F, 1.0F);
	}
}

void BeehiveBlockEntity::releaseAll(Level& level, Player* player) {
	for (Occupant& occupant : _occupants) releaseOccupant(level, occupant, ReleaseStatus::Emergency, player);
	_occupants.clear();
}

void BeehiveBlockEntity::preRemoveSideEffects(Level& level) {
	// The hive is gone: its bees out, angry at the nearest player within 8 blocks (who likely broke it)
	Player* nearest = nullptr;
	double	best	= 8.0 * 8.0;
	Vec3	center{pos().x + 0.5, pos().y + 0.5, pos().z + 0.5};
	for (const auto& player : level.players()) {
		if (player->isDisconnected() || player->isCreative() || player->isSpectator()) continue;
		double d = (player->position() - center).lengthSqr();
		if (d < best) nearest = player.get(), best = d;
	}
	releaseAll(level, nearest);
}

void BeehiveBlockEntity::save(BlockEntityWriter& out) const {
	out.u8(1); // Version
	out.varint(static_cast<uint32_t>(_occupants.size()));
	for (const Occupant& occupant : _occupants) {
		out.string(out.gameData.getStaticName("minecraft:entity_type", occupant.typeId));
		out.bytes(occupant.data);
		out.varint(static_cast<uint32_t>(occupant.ticksInHive));
		out.varint(static_cast<uint32_t>(occupant.minTicksInHive));
		out.u8(occupant.hasNectar);
	}
}

void BeehiveBlockEntity::load(BlockEntityReader& in) {
	_occupants.clear();
	std::vector<uint8_t> rest = in.rest();
	if (rest.empty() || rest[0] != 1) return; // Imported or older data: no bees kept
	try {
		BlockEntityReader data(in.gameData, rest.data() + 1, rest.size() - 1);
		uint32_t		  count = data.varint();
		for (uint32_t i = 0; i < count; i++) {
			Occupant occupant;
			occupant.typeId			= in.gameData.getStaticId("minecraft:entity_type", data.string());
			occupant.data			= data.bytes();
			occupant.ticksInHive	= static_cast<int>(data.varint());
			occupant.minTicksInHive = static_cast<int>(data.varint());
			occupant.hasNectar		= data.u8() != 0;
			if (occupant.typeId >= 0) _occupants.push_back(std::move(occupant));
		}
	} catch (const std::exception&) {
		_occupants.clear();
	}
}
