#ifndef DEATH_MESSAGES_HPP
#define DEATH_MESSAGES_HPP

#include <filesystem>
#include <string>
#include <unordered_map>

// Death messages, from the death-messages/ folder next to the executable (editable):
//   damage-types.json  { "damage-types": { "minecraft:fall": { "message-id": "fall", "death-message-type": "fall_variants" } } }
//   en_us.json         { "messages": { "death.attack.fall": "%1$s hit the ground too hard", ... } }   (required)
//   fr_fr.json...      same, for the clients in that language; the messages it lacks come from en_us.json
//
// The message is picked like vanilla (DamageSource.getLocalizedDeathMessage and CombatTracker.getDeathMessage):
//   - death.attack.<message-id> with (victim, source) when an entity dealt the damage
//   - death.attack.<message-id>.player with (victim, killer) when not, but a player is credited with the kill
//   - death.attack.<message-id> with (victim) otherwise
//   - falls: death.fell.assist with (victim, killer), else death.fell.accident.generic
// then written by the server in the client's language.
class DeathMessages {
  public:
	static constexpr const char* DEFAULT_LANGUAGE = "en_us";

	// Loads every language of the folder. Throws if damage-types.json or en_us.json can't be read
	void load(const std::filesystem::path& folder);

	// damageType: e.g. "minecraft:fall". source: name of the entity that dealt the damage, killer: of the player
	// credited with the kill; empty if none. language: the client's, e.g. "fr_fr"
	std::string format(const std::string& language, const std::string& damageType, const std::string& victim, const std::string& source,
					   const std::string& killer) const;

  private:
	using Messages = std::unordered_map<std::string, std::string>;
	struct DamageType {
		std::string messageId;
		std::string deathMessageType; // "default", "fall_variants" or "intentional_game_design"
	};
	std::unordered_map<std::string, DamageType> _damageTypes;
	std::unordered_map<std::string, Messages>	_languages; // By language, lowercase

	// In this language, else in the default one; null if neither has it
	const std::string* message(const Messages* language, const std::string& key) const;
	// Replaces %1$s, %2$s... and %s (next argument) like Java's String.format
	static std::string substitute(const std::string& pattern, const std::string* args, size_t argCount);
};

#endif
