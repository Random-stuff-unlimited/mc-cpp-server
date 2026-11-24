#include "world/blockentity/SignBlockEntity.hpp"

#include "world/Level.hpp"

#include <algorithm>

namespace {
	constexpr uint8_t SAVE_VERSION = 1;

	void i16(std::vector<uint8_t>& out, uint16_t value) {
		out.push_back(static_cast<uint8_t>(value >> 8));
		out.push_back(static_cast<uint8_t>(value));
	}
	void i32(std::vector<uint8_t>& out, int32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(value >> shift));
	}
	void name(std::vector<uint8_t>& out, uint8_t type, const char* tag) {
		out.push_back(type);
		std::string text(tag);
		i16(out, static_cast<uint16_t>(text.size()));
		out.insert(out.end(), text.begin(), text.end());
	}
	// NBT strings are modified UTF-8: plain UTF-8 is the same for the characters a sign holds (no NUL, BMP)
	void stringPayload(std::vector<uint8_t>& out, const std::string& value) {
		i16(out, static_cast<uint16_t>(std::min<size_t>(value.size(), 65535)));
		out.insert(out.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(value.size(), 65535)));
	}

	// SignText.DIRECT_CODEC: {messages: [4 components], color, has_glowing_text}. A string tag is a plain text component
	void writeSide(std::vector<uint8_t>& out, const char* tag, const SignText& text) {
		name(out, 10, tag);
		name(out, 9, "messages");
		out.push_back(8);
		i32(out, SignText::LINES);
		for (const std::string& line : text.messages) stringPayload(out, line);
		name(out, 8, "color");
		stringPayload(out, text.color);
		name(out, 1, "has_glowing_text");
		out.push_back(text.glowingText ? 1 : 0);
		out.push_back(0);
	}

	// SignBlockEntity's text filter: no formatting codes (§) and no control characters
	std::string sanitize(const std::string& line) {
		std::string clean;
		for (size_t i = 0; i < line.size() && clean.size() < SignBlockEntity::MAX_LINE_LENGTH; i++) {
			unsigned char c = static_cast<unsigned char>(line[i]);
			// "§" is C2 A7 in UTF-8: it and the character after it go
			if (c == 0xC2 && i + 1 < line.size() && static_cast<unsigned char>(line[i + 1]) == 0xA7) {
				i += 2;
				continue;
			}
			if (c < 0x20 || c == 0x7F) continue;
			clean += line[i];
		}
		return clean;
	}
} // namespace

bool SignText::hasMessage() const {
	return std::any_of(messages.begin(), messages.end(), [](const std::string& line) { return !line.empty(); });
}

void SignBlockEntity::setWaxed(bool waxed) {
	if (_waxed == waxed) return;
	_waxed = waxed;
	setChanged();
}

void SignBlockEntity::updateText(bool front, const std::array<std::string, SignText::LINES>& lines) {
	SignText& side = text(front);
	for (int i = 0; i < SignText::LINES; i++) side.messages[i] = sanitize(lines[i]);
	_editor.reset();
	setChanged();
}

void SignBlockEntity::setChanged() {
	markChanged();
	if (level()) level()->sendBlockUpdated(pos());
}

void SignBlockEntity::save(BlockEntityWriter& out) const {
	out.u8(SAVE_VERSION);
	for (const SignText* side : {&_front, &_back}) {
		for (const std::string& line : side->messages) out.string(line);
		out.string(side->color);
		out.u8(side->glowingText);
	}
	out.u8(_waxed);
}

void SignBlockEntity::load(BlockEntityReader& in) {
	// Signs saved before they were simulated kept no data (a generic block entity): blank
	std::vector<uint8_t> data = in.rest();
	if (data.empty()) return;
	BlockEntityReader reader(in.gameData, data.data(), data.size());
	reader.u8(); // Version
	for (SignText* side : {&_front, &_back}) {
		for (std::string& line : side->messages) line = reader.string();
		side->color		  = reader.string();
		side->glowingText = reader.u8() != 0;
	}
	_waxed = reader.u8() != 0;
}

// getUpdateTag: both sides and the wax
void SignBlockEntity::writeUpdateTag(std::vector<uint8_t>& out) const {
	out.push_back(10);
	writeSide(out, "front_text", _front);
	writeSide(out, "back_text", _back);
	name(out, 1, "is_waxed");
	out.push_back(_waxed ? 1 : 0);
	out.push_back(0);
}
