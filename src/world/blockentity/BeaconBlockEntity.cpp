#include "world/blockentity/BeaconBlockEntity.hpp"

#include "data/GameData.hpp"
#include "player.hpp"
#include "world/Level.hpp"

namespace {
	constexpr uint8_t SAVE_VERSION = 1;

	// BeaconBlockEntity.BEACON_EFFECTS
	constexpr MobEffects::Effect POWERS[] = {MobEffects::Speed, MobEffects::Haste, MobEffects::Resistance, MobEffects::JumpBoost, MobEffects::Strength,
											 MobEffects::Regeneration};

	void name(std::vector<uint8_t>& out, uint8_t type, const std::string& tag) {
		out.push_back(type);
		out.push_back(static_cast<uint8_t>(tag.size() >> 8));
		out.push_back(static_cast<uint8_t>(tag.size()));
		out.insert(out.end(), tag.begin(), tag.end());
	}
} // namespace

bool BeaconBlockEntity::isValidPower(MobEffects::Effect effect) {
	for (MobEffects::Effect power : POWERS) {
		if (power == effect) return true;
	}
	return false;
}

void BeaconBlockEntity::setPowers(Power primary, Power secondary) {
	_primary   = primary && isValidPower(*primary) ? primary : std::nullopt;
	_secondary = secondary && isValidPower(*secondary) ? secondary : std::nullopt;
	if (level() && _beam) level()->playSound(nullptr, pos(), "minecraft:block.beacon.power_select", Level::SoundSource::Blocks);
	markChanged();
	if (level()) level()->sendBlockUpdated(pos());
}

int BeaconBlockEntity::pyramidLevels(Level& level, const BlockPos& pos) {
	const GameData& data   = level.gameData();
	int				levels = 0;
	for (int i = 1; i <= 4; levels = i++) {
		int y = pos.y - i;
		if (y < level.minY()) break;
		for (int x = pos.x - i; x <= pos.x + i; x++) {
			for (int z = pos.z - i; z <= pos.z + i; z++) {
				if (!data.isInTag("minecraft:block", "minecraft:beacon_base_blocks", level.blocks().blockOf(level.getBlockState({x, y, z})))) return levels;
			}
		}
	}
	return levels;
}

// The beam goes up through everything but opaque blocks (light block 15), bedrock aside
bool BeaconBlockEntity::checkBeam(Level& level) const {
	int bedrock = level.gameData().getStaticId("minecraft:block", "minecraft:bedrock");
	for (int y = pos().y + 1; y < level.maxY(); y++) {
		int state = level.getBlockState({pos().x, y, pos().z});
		if (level.gameData().getStateProperties(state).lightBlock >= 15 && level.blocks().blockOf(state) != bedrock) return false;
	}
	return true;
}

// applyEffects: the players within levels * 10 + 10 blocks (and up to the top of the world)
void BeaconBlockEntity::applyEffects(Level& level) const {
	if (_levels <= 0 || !_primary) return;
	double range	 = _levels * 10 + 10;
	int	   amplifier = _levels >= 4 && _primary == _secondary ? 1 : 0;
	int	   duration	 = (9 + _levels * 2) * 20;
	AABB area{pos().x - range, pos().y - range, pos().z - range, pos().x + 1 + range, static_cast<double>(level.maxY()), pos().z + 1 + range};
	for (const auto& player : level.players()) {
		if (player->isDisconnected() || player->isSpectator() || !player->boundingBox().intersects(area)) continue;
		player->effects().add(*player, MobEffects::Instance(*_primary, duration, amplifier, true, true));
		if (_levels >= 4 && _secondary && _secondary != _primary) {
			player->effects().add(*player, MobEffects::Instance(*_secondary, duration, 0, true, true));
		}
	}
}

void BeaconBlockEntity::tick(Level& level) {
	if (level.getGameTime() % 80 != 0) return;
	int	 before = _levels;
	_beam		= checkBeam(level);
	_levels		= _beam ? pyramidLevels(level, pos()) : 0;
	if (before == 0 && _levels > 0) {
		level.playSound(nullptr, pos(), "minecraft:block.beacon.activate", Level::SoundSource::Blocks);
	} else if (before > 0 && _levels == 0) {
		level.playSound(nullptr, pos(), "minecraft:block.beacon.deactivate", Level::SoundSource::Blocks);
	}
	if (_levels > 0) {
		applyEffects(level);
		level.playSound(nullptr, pos(), "minecraft:block.beacon.ambient", Level::SoundSource::Blocks);
	}
}

void BeaconBlockEntity::save(BlockEntityWriter& out) const {
	out.u8(SAVE_VERSION);
	out.string(_primary ? MobEffects::info(*_primary).name : "");
	out.string(_secondary ? MobEffects::info(*_secondary).name : "");
}

void BeaconBlockEntity::load(BlockEntityReader& in) {
	std::vector<uint8_t> data = in.rest();
	if (data.empty()) return; // Saved before beacons were simulated
	BlockEntityReader reader(in.gameData, data.data(), data.size());
	reader.u8();
	auto power = [](const std::string& text) -> Power {
		if (text.empty()) return std::nullopt;
		MobEffects::Effect effect = MobEffects::byName(text);
		return effect != MobEffects::Count && isValidPower(effect) ? Power(effect) : std::nullopt;
	};
	_primary   = power(reader.string());
	_secondary = power(reader.string());
}

// saveCustomOnly: the powers (the client draws the beam itself)
void BeaconBlockEntity::writeUpdateTag(std::vector<uint8_t>& out) const {
	out.push_back(10);
	for (const auto& [tag, power] : {std::pair<const char*, Power>{"primary_effect", _primary}, {"secondary_effect", _secondary}}) {
		if (!power) continue;
		name(out, 8, tag);
		std::string id = MobEffects::info(*power).name;
		out.push_back(static_cast<uint8_t>(id.size() >> 8));
		out.push_back(static_cast<uint8_t>(id.size()));
		out.insert(out.end(), id.begin(), id.end());
	}
	out.push_back(0);
}
