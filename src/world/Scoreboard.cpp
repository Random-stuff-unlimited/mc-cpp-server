#include "world/Scoreboard.hpp"

#include "PacketIds.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace {
	const char* const COLORS[16] = {"black", "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple", "gold",	  "gray",
									"dark_gray", "blue", "green",	  "aqua",	   "red",	   "light_purple", "yellow", "white"};
	const char* const AUTOMATIC[] = {"deathCount", "playerKillCount", "totalKillCount", "health", "food", "level", "xp", "air", "armor"};
	const char* const READ_ONLY[] = {"health", "food", "level", "xp", "air", "armor"};

	// The value of a read-only criterion for a player
	int valueOf(const std::string& criterion, Player& player, const GameData& gameData) {
		if (criterion == "health") return static_cast<int>(std::ceil(player.combat().health));
		if (criterion == "food") return player.foodData().getFoodLevel();
		if (criterion == "level") return player.getXpLevel();
		if (criterion == "xp") return player.getXpTotal();
		if (criterion == "air") return player.survival().airSupply;
		if (criterion == "armor") return static_cast<int>(Combat::armorValue(gameData, player));
		return 0;
	}
} // namespace

bool Scoreboard::isCriterion(const std::string& name) {
	if (name == "dummy" || name == "trigger") return true;
	return std::any_of(std::begin(AUTOMATIC), std::end(AUTOMATIC), [&](const char* c) { return name == c; });
}

bool Scoreboard::isReadOnly(const std::string& criterion) {
	return std::any_of(std::begin(READ_ONLY), std::end(READ_ONLY), [&](const char* c) { return criterion == c; });
}

int Scoreboard::slotByName(const std::string& name) {
	if (name == "list") return 0;
	if (name == "sidebar") return 1;
	if (name == "below_name" || name == "belowName") return 2;
	for (int i = 0; i < 16; i++) {
		if (name == std::string("sidebar.team.") + COLORS[i]) return 3 + i;
	}
	return -1;
}

std::string Scoreboard::slotName(int slot) {
	if (slot == 0) return "list";
	if (slot == 1) return "sidebar";
	if (slot == 2) return "below_name";
	return std::string("sidebar.team.") + COLORS[slot - 3];
}

// ----- Objectives -----

const Scoreboard::Objective* Scoreboard::objective(const std::string& name) const {
	auto it = _objectives.find(name);
	return it == _objectives.end() ? nullptr : &it->second;
}

std::vector<const Scoreboard::Objective*> Scoreboard::objectives() const {
	std::vector<const Objective*> list;
	for (const auto& [name, objective] : _objectives) list.push_back(&objective);
	return list;
}

bool Scoreboard::addObjective(const std::string& name, const std::string& criterion, const std::string& displayName) {
	if (_objectives.count(name)) return false;
	Objective& objective = _objectives[name];
	objective			 = {name, criterion, displayName.empty() ? name : displayName, criterion == "health"};
	sendObjective(objective, 0);
	save();
	return true;
}

bool Scoreboard::removeObjective(const std::string& name) {
	auto it = _objectives.find(name);
	if (it == _objectives.end()) return false;
	for (int slot = 0; slot < SLOT_COUNT; slot++) {
		if (_display[slot] == name) _display[slot].clear(); // The client drops it with the objective
	}
	for (auto& [owner, scores] : _scores) scores.erase(name);
	sendObjective(it->second, 1);
	_objectives.erase(it);
	save();
	return true;
}

void Scoreboard::setDisplayName(const std::string& name, const std::string& displayName) {
	auto it = _objectives.find(name);
	if (it == _objectives.end()) return;
	it->second.displayName = displayName;
	sendObjective(it->second, 2);
	save();
}

void Scoreboard::setHearts(const std::string& name, bool hearts) {
	auto it = _objectives.find(name);
	if (it == _objectives.end()) return;
	it->second.hearts = hearts;
	sendObjective(it->second, 2);
	save();
}

void Scoreboard::setDisplay(int slot, const std::string& objective) {
	if (slot < 0 || slot >= SLOT_COUNT) return;
	_display[slot] = objective;
	sendDisplay(slot);
	save();
}

// ----- Scores -----

std::optional<int> Scoreboard::score(const std::string& owner, const std::string& objective) const {
	auto it = _scores.find(owner);
	if (it == _scores.end()) return std::nullopt;
	auto score = it->second.find(objective);
	return score == it->second.end() ? std::nullopt : std::optional<int>(score->second);
}

void Scoreboard::setScore(const std::string& owner, const std::string& objective, int value) {
	if (!_objectives.count(objective)) return;
	auto& scores = _scores[owner];
	auto  it	 = scores.find(objective);
	if (it != scores.end() && it->second == value) return;
	scores[objective] = value;
	sendScore(owner, objective, value);
	save();
}

int Scoreboard::addScore(const std::string& owner, const std::string& objective, int amount) {
	int value = score(owner, objective).value_or(0) + amount;
	setScore(owner, objective, value);
	return value;
}

bool Scoreboard::resetScore(const std::string& owner, const std::string& objective) {
	auto it = _scores.find(owner);
	if (it == _scores.end() || !it->second.erase(objective)) return false;
	if (it->second.empty()) _scores.erase(it);
	sendReset(owner, &objective);
	save();
	return true;
}

bool Scoreboard::resetScores(const std::string& owner) {
	if (!_scores.erase(owner)) return false;
	sendReset(owner, nullptr);
	save();
	return true;
}

std::vector<std::string> Scoreboard::owners() const {
	std::vector<std::string> list;
	for (const auto& [owner, scores] : _scores) {
		if (!scores.empty()) list.push_back(owner);
	}
	return list;
}

std::map<std::string, int> Scoreboard::scoresOf(const std::string& owner) const {
	auto it = _scores.find(owner);
	return it == _scores.end() ? std::map<std::string, int>{} : it->second;
}

// ----- Criteria -----

void Scoreboard::addToCriterion(const std::string& criterion, const std::string& owner, int amount) {
	for (const auto& [name, objective] : _objectives) {
		if (objective.criterion == criterion) addScore(owner, name, amount);
	}
}

void Scoreboard::onPlayerDeath(Player& victim, Player* killer) {
	addToCriterion("deathCount", victim.getPlayerName(), 1);
	if (killer && killer != &victim) {
		addToCriterion("playerKillCount", killer->getPlayerName(), 1);
		addToCriterion("totalKillCount", killer->getPlayerName(), 1);
	}
}

void Scoreboard::onKill(Player& killer) { addToCriterion("totalKillCount", killer.getPlayerName(), 1); }

void Scoreboard::tick() {
	for (const auto& [name, objective] : _objectives) {
		if (!isReadOnly(objective.criterion)) continue;
		for (const auto& player : _server.getGamePlayers()) {
			if (player->isDisconnected()) continue;
			int value = valueOf(objective.criterion, *player, _server.getGameData());
			if (score(player->getPlayerName(), name) != value) {
				_scores[player->getPlayerName()][name] = value;
				sendScore(player->getPlayerName(), name, value);
			}
		}
	}
}

// ----- Network -----

void Scoreboard::broadcast(int packetId, Buffer& data) {
	for (const auto& player : _server.getGamePlayers()) Packet::send(player, packetId, data, _server);
}

// ClientboundSetObjectivePacket: name, mode (0 add, 1 remove, 2 change), then the display name and the render type
void Scoreboard::sendObjective(const Objective& objective, int mode, Player* to) {
	Buffer data;
	data.writeString(objective.name);
	data.writeByte(static_cast<int8_t>(mode));
	if (mode != 1) {
		TextComponent::writeJson(data, objective.displayName);
		data.writeVarInt(objective.hearts ? 1 : 0);
		data.writeBool(false); // No number format
	}
	if (to) {
		Packet::send(to->shared_from_this(), PacketId::Play::Clientbound::SET_OBJECTIVE, data, _server);
	} else {
		broadcast(PacketId::Play::Clientbound::SET_OBJECTIVE, data);
	}
}

void Scoreboard::sendScore(const std::string& owner, const std::string& objective, int value) {
	Buffer data;
	data.writeString(owner);
	data.writeString(objective);
	data.writeVarInt(value);
	data.writeBool(false); // No display name
	data.writeBool(false); // No number format
	broadcast(PacketId::Play::Clientbound::SET_SCORE, data);
}

void Scoreboard::sendReset(const std::string& owner, const std::string* objective) {
	Buffer data;
	data.writeString(owner);
	data.writeBool(objective != nullptr);
	if (objective) data.writeString(*objective);
	broadcast(PacketId::Play::Clientbound::RESET_SCORE, data);
}

void Scoreboard::sendDisplay(int slot, Player* to) {
	Buffer data;
	data.writeVarInt(slot);
	data.writeString(_display[slot]);
	if (to) {
		Packet::send(to->shared_from_this(), PacketId::Play::Clientbound::SET_DISPLAY_OBJECTIVE, data, _server);
	} else {
		broadcast(PacketId::Play::Clientbound::SET_DISPLAY_OBJECTIVE, data);
	}
}

void Scoreboard::sendAll(Player& player) {
	for (const auto& [name, objective] : _objectives) sendObjective(objective, 0, &player);
	for (int slot = 0; slot < SLOT_COUNT; slot++) {
		if (!_display[slot].empty()) sendDisplay(slot, &player);
	}
	for (const auto& [owner, scores] : _scores) {
		for (const auto& [objective, value] : scores) {
			Buffer data;
			data.writeString(owner);
			data.writeString(objective);
			data.writeVarInt(value);
			data.writeBool(false);
			data.writeBool(false);
			Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SET_SCORE, data, _server);
		}
	}
}

// ----- Saving -----

void Scoreboard::load(const std::filesystem::path& file) {
	_file = file;
	std::ifstream in(file);
	if (!in) return;
	try {
		nlohmann::json data = nlohmann::json::parse(in);
		for (const auto& o : data.value("objectives", nlohmann::json::array())) {
			Objective objective{o.value("name", ""), o.value("criterion", "dummy"), o.value("display-name", ""), o.value("hearts", false)};
			if (!objective.name.empty()) _objectives[objective.name] = objective;
		}
		for (const auto& [owner, scores] : data.value("scores", nlohmann::json::object()).items()) {
			for (const auto& [objective, value] : scores.items()) {
				if (_objectives.count(objective) && value.is_number_integer()) _scores[owner][objective] = value.get<int>();
			}
		}
		for (const auto& [slot, objective] : data.value("display", nlohmann::json::object()).items()) {
			int index = slotByName(slot);
			if (index >= 0 && objective.is_string() && _objectives.count(objective.get<std::string>())) _display[index] = objective.get<std::string>();
		}
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Couldn't read " + file.string() + ": " + e.what(), "Scoreboard");
	}
}

void Scoreboard::save() const {
	if (_file.empty()) return;
	nlohmann::json data;
	data["objectives"] = nlohmann::json::array();
	for (const auto& [name, o] : _objectives) {
		data["objectives"].push_back({{"name", o.name}, {"criterion", o.criterion}, {"display-name", o.displayName}, {"hearts", o.hearts}});
	}
	data["scores"] = nlohmann::json::object();
	for (const auto& [owner, scores] : _scores) {
		for (const auto& [objective, value] : scores) {
			if (!isReadOnly(_objectives.at(objective).criterion)) data["scores"][owner][objective] = value;
		}
	}
	data["display"] = nlohmann::json::object();
	for (int slot = 0; slot < SLOT_COUNT; slot++) {
		if (!_display[slot].empty()) data["display"][slotName(slot)] = _display[slot];
	}
	std::ofstream out(_file);
	if (out) out << data.dump(2) << "\n";
}
