#include "world/Stats.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <fstream>

namespace {
	const char* const TYPE_NAMES[] = {"minecraft:mined",  "minecraft:crafted", "minecraft:used",	  "minecraft:broken", "minecraft:picked_up",
									  "minecraft:dropped", "minecraft:killed", "minecraft:killed_by", "minecraft:custom"};
	constexpr int	  DATA_VERSION = 4556; // 1.21.10
} // namespace

const char* PlayerStats::registryOf(Type type) {
	switch (type) {
	case Mined: return "minecraft:block";
	case Killed:
	case KilledBy: return "minecraft:entity_type";
	case Custom: return "minecraft:custom_stat";
	default: return "minecraft:item";
	}
}

int PlayerStats::get(Type type, int id) const {
	auto it = _values.find({type, id});
	return it == _values.end() ? 0 : it->second;
}

void PlayerStats::award(Type type, int id, int amount) {
	if (id < 0 || amount == 0) return;
	int64_t value		  = static_cast<int64_t>(get(type, id)) + amount;
	_values[{type, id}] = static_cast<int>(std::min<int64_t>(value, 0x7FFFFFFF));
}

void PlayerStats::set(Type type, int id, int value) {
	if (id >= 0) _values[{type, id}] = value;
}

void PlayerStats::load(const std::filesystem::path& file, const GameData& gameData) {
	_values.clear();
	std::ifstream in(file);
	if (!in) return;
	try {
		nlohmann::json data = nlohmann::json::parse(in);
		for (int type = 0; type <= Custom; type++) {
			if (!data.contains("stats") || !data["stats"].contains(TYPE_NAMES[type])) continue;
			for (const auto& [name, value] : data["stats"][TYPE_NAMES[type]].items()) {
				int id = gameData.getStaticId(registryOf(static_cast<Type>(type)), name);
				if (id >= 0 && value.is_number_integer()) _values[{type, id}] = value.get<int>();
			}
		}
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Couldn't read " + file.string() + ": " + e.what(), "Stats");
	}
}

std::string PlayerStats::toJson(const GameData& gameData) const {
	nlohmann::json data;
	data["stats"] = nlohmann::json::object();
	for (const auto& [key, value] : _values) {
		std::string name = gameData.getStaticName(registryOf(static_cast<Type>(key.first)), key.second);
		if (!name.empty()) data["stats"][TYPE_NAMES[key.first]][name] = value;
	}
	data["DataVersion"] = DATA_VERSION;
	return data.dump();
}

namespace Stats {
	void custom(Player& player, const GameData& gameData, const char* name, int amount) {
		player.stats().award(PlayerStats::Custom, gameData.getStaticId("minecraft:custom_stat", name), amount);
	}

	void send(Server& server, Player& player) {
		// ClientboundAwardStatsPacket: (stat type, stat id, value) for each
		const auto& values = player.stats().values();
		Buffer		data;
		data.writeVarInt(static_cast<int>(values.size()));
		for (const auto& [key, value] : values) {
			data.writeVarInt(key.first);
			data.writeVarInt(key.second);
			data.writeVarInt(value);
		}
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::AWARD_STATS, data, server);
	}
} // namespace Stats
