#include "data/DeathMessages.hpp"

#include "lib/json.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace {
	nlohmann::json readJson(const std::filesystem::path& file) {
		std::ifstream in(file);
		if (!in) throw std::runtime_error("cannot open " + file.string());
		return nlohmann::json::parse(in);
	}

	std::string lowercase(std::string text) {
		for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return text;
	}
} // namespace

void DeathMessages::load(const std::filesystem::path& folder) {
	_damageTypes.clear();
	_languages.clear();

	nlohmann::json damageTypes = readJson(folder / "damage-types.json").value("damage-types", nlohmann::json::object());
	for (const auto& [name, type] : damageTypes.items()) {
		_damageTypes[name] = {type.value("message-id", "generic"), type.value("death-message-type", "default")};
	}

	readJson(folder / (std::string(DEFAULT_LANGUAGE) + ".json")); // Required
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		std::string name = entry.path().stem().string();
		if (entry.path().extension() != ".json" || name == "damage-types") continue;
		nlohmann::json messages = readJson(entry.path()).value("messages", nlohmann::json::object());
		Messages&	   language = _languages[lowercase(name)];
		for (const auto& [key, text] : messages.items()) language[key] = text.get<std::string>();
	}
}

const std::string* DeathMessages::message(const Messages* language, const std::string& key) const {
	if (language) {
		if (auto it = language->find(key); it != language->end()) return &it->second;
	}
	auto fallback = _languages.find(DEFAULT_LANGUAGE);
	if (fallback == _languages.end()) return nullptr;
	auto it = fallback->second.find(key);
	return it == fallback->second.end() ? nullptr : &it->second;
}

std::string DeathMessages::format(const std::string& language, const std::string& damageType, const std::string& victim,
								  const std::string& source, const std::string& killer) const {
	auto			found	 = _languages.find(lowercase(language));
	const Messages* messages = found == _languages.end() ? nullptr : &found->second;

	DamageType type{"generic", "default"};
	if (auto it = _damageTypes.find(damageType); it != _damageTypes.end()) type = it->second;

	// The key doesn't depend on the language: variants are looked up in the default one
	std::string key;
	std::string args[2]	 = {victim, ""};
	size_t		argCount = 1;
	if (type.deathMessageType == "fall_variants") {
		key = killer.empty() ? "death.fell.accident.generic" : "death.fell.assist";
		if (!killer.empty()) args[argCount++] = killer;
	} else if (type.deathMessageType == "intentional_game_design") {
		key						= "death.attack." + type.messageId + ".message";
		const std::string* link = message(messages, "death.attack." + type.messageId + ".link");
		args[argCount++]		= "[" + (link ? *link : std::string("Intentional Game Design")) + "]";
	} else {
		key = "death.attack." + type.messageId;
		if (!source.empty()) {
			args[argCount++] = source;
		} else if (!killer.empty() && message(nullptr, key + ".player")) {
			key += ".player";
			args[argCount++] = killer;
		}
	}

	const std::string* pattern = message(messages, key);
	if (!pattern) pattern = message(messages, "death.attack.generic");
	return pattern ? substitute(*pattern, args, argCount) : victim + " died";
}

std::string DeathMessages::substitute(const std::string& pattern, const std::string* args, size_t argCount) {
	std::string out;
	size_t		next = 0; // For %s
	for (size_t i = 0; i < pattern.size(); i++) {
		if (pattern[i] != '%' || i + 1 >= pattern.size()) {
			out += pattern[i];
			continue;
		}
		if (pattern[i + 1] == '%') {
			out += '%';
			i++;
		} else if (pattern[i + 1] == 's') {
			if (next < argCount) out += args[next];
			next++;
			i++;
		} else if (std::isdigit(static_cast<unsigned char>(pattern[i + 1])) && i + 3 < pattern.size() && pattern[i + 2] == '$' && pattern[i + 3] == 's') {
			size_t index = pattern[i + 1] - '1';
			if (index < argCount) out += args[index];
			i += 3;
		} else {
			out += pattern[i];
		}
	}
	return out;
}
