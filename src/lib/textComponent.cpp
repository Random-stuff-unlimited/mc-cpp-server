#include "network/TextComponent.hpp"

#include "network/buffer.hpp"

namespace {
	constexpr int8_t TAG_END	= 0;
	constexpr int8_t TAG_STRING = 8;
	constexpr int8_t TAG_LIST	= 9;
	constexpr int8_t TAG_COMPOUND = 10;

	// NBT strings: unsigned short length, then the bytes
	void writeNbtString(Buffer& buf, const std::string& value) {
		buf.writeUShort(static_cast<uint16_t>(value.size()));
		buf.writeBytes(value);
	}

	void writeStringField(Buffer& buf, const std::string& name, const std::string& value) {
		buf.writeByte(TAG_STRING);
		writeNbtString(buf, name);
		writeNbtString(buf, value);
	}
} // namespace

namespace TextComponent {

	void writeText(Buffer& buf, const std::string& text, const std::string& color) {
		buf.writeByte(TAG_COMPOUND); // Network NBT: no root name
		writeStringField(buf, "text", text);
		if (!color.empty()) writeStringField(buf, "color", color);
		buf.writeByte(TAG_END);
	}

	void writeTranslatable(Buffer& buf, const std::string& key, const std::vector<std::string>& args, const std::string& color) {
		buf.writeByte(TAG_COMPOUND);
		writeStringField(buf, "translate", key);
		if (!args.empty()) {
			// A plain string is a valid component, so the arguments are a list of strings
			buf.writeByte(TAG_LIST);
			writeNbtString(buf, "with");
			buf.writeByte(TAG_STRING);
			buf.writeInt(static_cast<int32_t>(args.size()));
			for (const std::string& arg : args) writeNbtString(buf, arg);
		}
		if (!color.empty()) writeStringField(buf, "color", color);
		buf.writeByte(TAG_END);
	}

} // namespace TextComponent
