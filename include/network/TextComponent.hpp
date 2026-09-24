#ifndef TEXT_COMPONENT_HPP
#define TEXT_COMPONENT_HPP

#include <string>
#include <vector>

class Buffer;

// Chat components as the client expects them since 1.20.3: network NBT (a compound without root name).
namespace TextComponent {
	// Plain text: {"text": text, "color": color}
	void writeText(Buffer& buf, const std::string& text, const std::string& color = "");
	// Translated by each client in its own language: {"translate": key, "with": [args], "color": color}
	// Keys are the game's, e.g. "multiplayer.player.joined" -> "%s joined the game"
	void writeTranslatable(Buffer& buf, const std::string& key, const std::vector<std::string>& args, const std::string& color = "");
} // namespace TextComponent

#endif
